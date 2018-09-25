/* Copyright (c) 2018 Patrick Fay
 *
 * License http://opensource.org/licenses/mit-license.php MIT License
 */

/*
 * trace-cmd reference:
 *   trace-cmd/Documentation/trace-cmd.dat.5.txt
 */

#define NOMINMAX
#ifdef _WIN32
#include <windows.h>
#define pid_t int
#endif


#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <cstddef>
#include <inttypes.h>
#include <algorithm>
#include <unordered_map>
#include <thread>
#ifdef __linux__
#include <unistd.h>
#endif
#include <sstream>
#include <iomanip>
#include <signal.h>
#include "perf_event_subset.h"
#include "rd_json2.h"
#include "printf.h"
#include "oppat.h"
#include "prf_read_data.h"
#include "lua_rtns.h"
#include "utils.h"
#include "rd_json.h"
#include "ck_nesting.h"
#include "web_api.h"
#include "MemoryMapped.h"
#include "mygetopt.h"
#include "dump.hpp"
#define TC_RD_DATA_CPP
#include "tc_read_data.h"

#define BUF_MAX 4096
static char buf[BUF_MAX];

struct tc_fte_area_str {
	std::string area;
	int beg, len;
	tc_fte_area_str(): beg(0), len(0) {}
};
struct tc_fte_fmt_str {
	std::string area;
	std::string format;
};
struct tc_opts_str {
	int opt;
	std::string str;
};

struct tc_cpu_data_str {
	long off, len;
};

enum tc_evt_hdr_typ {
	tc_eh_typ_data_len_max = 28,
	tc_eh_typ_padding,
	tc_eh_typ_time_extend,
	tc_eh_typ_timestamp,
};

static char *tc_decode_evt_hdr_typ(uint32_t eh_typ)
{
	static char *eh_typ_strs[] = {"len_max", "padding", "tm_extnd", "ts"};
	static char *reg_typ = "len";

	if (eh_typ < tc_eh_typ_data_len_max) {
		return reg_typ;
	}
	uint32_t indx = eh_typ - tc_eh_typ_data_len_max;
	if (indx < 4) {
		return eh_typ_strs[indx];
	}
	return "unknown";
}

static int tc_read_evt(long max_pos, int cpu, long page_sz, const unsigned char *mm_buf,
		long &pos, uint64_t &ts, int line, prf_obj_str &prf_obj, int verbose, prf_obj_str *prf_obj_prev)
{
	static int orig_order=0;
	int evt_len = 0;
do_page_hdr:
	if (((pos+4) % page_sz) == 0) {
		//read_n_bytes(file, pos, 4, __LINE__);
		mm_read_n_bytes(mm_buf, pos, 4, __LINE__, buf, BUF_MAX);
		evt_len += 4;
	}
	if (pos >= max_pos) {
		return evt_len;
	}

	if ((pos % page_sz) == 0) {
		int sz = 16;
		//read_n_bytes(file, pos, sz, __LINE__);
		mm_read_n_bytes(mm_buf, pos, sz, __LINE__, buf, BUF_MAX);
		ts = *(buf_uint64_ptr(buf, 0));
		if (verbose)
			printf("cpu[%d] ts= %" PRIu64 "\n", cpu, ts);
		evt_len += sz;
	}
	long pos_sv = pos;
	//read_n_bytes(file, pos, 4, __LINE__);
	mm_read_n_bytes(mm_buf, pos, 4, __LINE__, buf, BUF_MAX);
	evt_len += 4;
	uint32_t evt_hdr = *(buf_uint32_ptr(buf, 0));
	uint32_t evt_hdr_typ = (0x1f & evt_hdr);
	uint32_t evt_hdr_tm_delta = (evt_hdr >> 5);
	if (evt_hdr_typ == tc_eh_typ_time_extend) {
		uint64_t tm_delta = evt_hdr_tm_delta;
		//read_n_bytes(file, pos, 4, __LINE__);
		mm_read_n_bytes(mm_buf, pos, 4, __LINE__, buf, BUF_MAX);
		uint32_t ts_top = *(buf_uint32_ptr(buf, 0));
		uint64_t ts_top64 = ts_top;
		tm_delta |= (ts_top64 << 27);
		ts += tm_delta;
		if (verbose)
			printf("tm_delta= %" PRIu64 " at %s %d\n", tm_delta, __FILE__, __LINE__);
		evt_len += 4;
	} else if (evt_hdr_typ == tc_eh_typ_padding) {
		if (evt_hdr_tm_delta == 0) {
		} else {
			//read_n_bytes(file, pos, 4, __LINE__);
			mm_read_n_bytes(mm_buf, pos, 4, __LINE__, buf, BUF_MAX);
			evt_len += 4;
			uint32_t len = *(buf_uint32_ptr(buf, 0));
			//read_n_bytes(file, pos, len, __LINE__);
			mm_read_n_bytes(mm_buf, pos, (int)len, __LINE__, buf, BUF_MAX);
			evt_len += len;
		}
	} else if (evt_hdr_typ <= tc_eh_typ_data_len_max) {
		if (evt_hdr_typ == 0) {
			//read_n_bytes(file, pos, 4, __LINE__);
			mm_read_n_bytes(mm_buf, pos, 4, __LINE__, buf, BUF_MAX);
			evt_len += 4;
			uint32_t len = *(buf_uint32_ptr(buf, 0));
			if (len == 0 && (pos % page_sz) == 0) {
				goto do_page_hdr;
			}
			if (evt_hdr_tm_delta == 0 && len == 0) {
				// this is just a filler, no timestamp delta, no data
				return evt_len;
			}
			//read_n_bytes(file, pos, len, __LINE__);
			mm_read_n_bytes(mm_buf, pos, (int)len, __LINE__, buf, BUF_MAX);
			evt_len += len;
			printf("evt_hdr_typ == 0, len= %d, tot_len= %d, pos_sv= %ld, pos= %ld\n", len, evt_len, pos_sv, pos);
			ts += evt_hdr_tm_delta;
		} else {
			uint32_t len = evt_hdr_typ << 2;
			//read_n_bytes(file, pos, len, __LINE__);
			uint64_t mm_off = (uint64_t)pos;
			mm_read_n_bytes(mm_buf, pos, (int)len, __LINE__, buf, BUF_MAX);
			evt_len += len;
			ts += evt_hdr_tm_delta;
			//char *hdr_typ_str = tc_decode_evt_hdr_typ(evt_hdr_typ);
			//printf("evt_hdr_typ == 0x%x(%s), len= %d, tot_len= %d, pos_sv= %ld, pos= %ld at %s %d\n",
			//	evt_hdr_typ, hdr_typ_str, len, evt_len, pos_sv, pos, __FILE__, __LINE__);
			if (evt_hdr_typ < tc_eh_typ_data_len_max) {
			//if (evt_hdr_typ == 0x10 || evt_hdr_typ == 0x4 || evt_hdr_typ == 0x8)
#pragma pack(push, 1)
				struct cmn_hdr_str {
					uint16_t type;
					uint8_t flags;
					uint8_t preempt_count;
					int32_t pid;
				} *cmn_hdr;
#pragma pack(pop)
				prf_samples_str pss;
				cmn_hdr = (cmn_hdr_str *)buf;
				pss.mm_off = (long)mm_off;
				double tsn = 1.e-9 * (double)ts;
				if (verbose > 0)
					printf("tc_evt_num= %u, flags= 0x%x, prempt= %d, pid= %d, cpu= %d ts= %.9f, off= %ld\n",
					cmn_hdr->type, cmn_hdr->flags, cmn_hdr->preempt_count, cmn_hdr->pid, cpu, tsn, pss.mm_off);
				if (pss.mm_off == 5030836) {
					printf("ckck: tc_evt_num= %u, flags= 0x%x, prempt= %d, pid= %d, cpu= %d ts= %.9f, off= %ld, len= %d at %s %d\n",
					cmn_hdr->type, cmn_hdr->flags, cmn_hdr->preempt_count, cmn_hdr->pid, cpu, tsn, pss.mm_off, len, __FILE__, __LINE__);
				}

				uint32_t ck_idx = prf_obj.ids_2_evt_indxp1[cmn_hdr->type];
				uint32_t ck_evt_idx = prf_obj.ids_vec[ck_idx-1];
				std::string evt_nm = prf_obj.events[ck_evt_idx].event_name;

				if (prf_obj.events[ck_evt_idx].pea.type == PERF_TYPE_TRACEPOINT &&
					prf_obj.events[ck_evt_idx].lst_ft_fmt_idx == -2) {
					prf_obj.events[ck_evt_idx].lst_ft_fmt_idx = -1;
					for (uint32_t i=0; i < lst_ft_fmt_vec.size(); i++) {
						if (lst_ft_fmt_vec[i].event == evt_nm && lst_ft_fmt_vec[i].area == prf_obj.events[ck_evt_idx].event_area) {
							printf("got lst_ft_fmt: match on trace-cmd event= %s at %s %d\n", evt_nm.c_str(), __FILE__, __LINE__);
							prf_obj.events[ck_evt_idx].lst_ft_fmt_idx = (int)i;
							break;
						}
					}
				}
				int pid = cmn_hdr->pid;
#if 1
				int pid_indx = (int)prf_obj.tid_2_comm_indxp1[(uint32_t)pid];
				if (pid_indx == 0) {
					printf("missed loookup of comm for pid= %d, tid= %d, evt_nm= %s len= %d at %s %d\n", pid, pid, evt_nm.c_str(), (int)len, __FILE__, __LINE__);
					// trace-cmd will sometimes not record a pid in its list. But it will be in the sched_switch event data.
					// so if we are doing sched_switch then lets add it now.
					if (evt_nm == "sched_switch") {
#pragma pack(push, 1)
						struct sched_switch_str {
							uint8_t hdr[8];
							char    prev_comm[16];
							int     prev_pid;
							int     prev_prio;
							uint64_t prev_state;
							char    next_comm[16];
							int     next_pid;
						} *s_sw;
#pragma pack(pop)
						s_sw = (sched_switch_str *)buf;
						printf("prev_pid= %d, prev_comm= %s, next_pid= %d, next_comm= %s at %s %d\n",
							s_sw->prev_pid, s_sw->prev_comm, s_sw->next_pid, s_sw->next_comm, __FILE__, __LINE__);
						prf_add_comm((uint32_t)s_sw->prev_pid, (uint32_t)s_sw->prev_pid, std::string(s_sw->prev_comm), prf_obj, tsn);
						pid_indx = prf_obj.tid_2_comm_indxp1[(uint32_t)pid];
						if (pid_indx == 0) {
							printf("still missed loookup of comm for pid= %d, tid= %d, evt_nm= %s at %s %d\n", pid, pid, evt_nm.c_str(), __FILE__, __LINE__);
							exit(1);
						}
					} else {
						// see if we've already read the prf bin file and if prf_bin is the same 'file_tag'
						// prf bin has more complete pid/tid info. see if we can find the tid there.
						if (prf_obj_prev != NULL) {
						printf("prf_obj_prev= %p, ft0= %d, ft1= %d at %s %d\n",
							prf_obj_prev, prf_obj.file_tag_idx, prf_obj_prev->file_tag_idx, __FILE__, __LINE__);
						}
						if (prf_obj_prev != NULL && prf_obj.file_tag_idx == prf_obj_prev->file_tag_idx) {
							pid_indx = (int)prf_obj_prev->tid_2_comm_indxp1[(uint32_t)pid];
							if (pid_indx == 0) {
								printf("didn't find the pid in PERF prf_obj either. Bye at %s %d\n", __FILE__, __LINE__);
								exit(1);
							} else {
							prf_add_comm((uint32_t)prf_obj_prev->comm[pid_indx-1].pid, (uint32_t)prf_obj_prev->comm[pid_indx-1].tid,
								std::string(prf_obj_prev->comm[pid_indx-1].comm), prf_obj, tsn);
							printf("old pid_indx= %d, got pid in prf_obj_prev: pid= %d, tid= %d, comm= %s at %s %d\n",
								pid_indx, (uint32_t)prf_obj_prev->comm[pid_indx-1].pid, (uint32_t)prf_obj_prev->comm[pid_indx-1].tid,
								std::string(prf_obj_prev->comm[pid_indx-1].comm).c_str(), __FILE__, __LINE__);
							pid_indx = prf_obj.tid_2_comm_indxp1[(uint32_t)pid];
							}
						}
						if (pid_indx == 0) {
							printf("didn't find the pid in PERF prf_obj either2. Bye at %s %d\n", __FILE__, __LINE__);
							exit(1);
						}
					}
				}
				if (pid_indx <= 0 || pid_indx > prf_obj.comm.size()) {
					printf("invalid pid_indx= %d , sz= %d at %s %d\n", pid_indx, (int)prf_obj.comm.size(), __FILE__, __LINE__);
					exit(1);
				}
				pss.comm   = prf_obj.comm[(uint64_t)(pid_indx-1)].comm;
				pss.event  = evt_nm;
				pss.evt_idx= ck_evt_idx;
				pss.pid    = (uint32_t)pid;
				pss.tid    = (uint32_t)pid;
				pss.cpu    = (uint32_t)cpu;
				pss.ts     = ts;
				pss.period = 1;
				pss.mm_off = (long)mm_off;
				pss.orig_order = orig_order++;
				char num_buf[256];
#define TRACE_CMD_CLOCK_MONO
#ifdef TRACE_CMD_CLOCK_MONO
				double tm = (double)ts;
				int slen = snprintf(num_buf, sizeof(num_buf), " %.0f: ", tm);
#endif
#ifdef TRACE_CMD_CLOCK_LOCAL
				double tm = 1.e-9 * (double)ts;
				int slen = snprintf(num_buf, sizeof(num_buf), " %.9f: ", tm);
#endif
				if (slen == sizeof(num_buf)) {
					printf("overflow for tm= %.9f at %s %d\n", tm, __FILE__, __LINE__);
					exit(1);
				}
				pss.tm_str = std::string(num_buf);
				if (verbose > 0)
					printf("hdr:[%d] %-16.16s %d/%d [%.3d]%s %s:\n",
					(int)prf_obj.samples.size(), pss.comm.c_str(), pid, pid, cpu, pss.tm_str.c_str(), evt_nm.c_str());
				bool use_sample;
				use_sample = true;
				if ((options.tm_clip_beg_valid && tsn < options.tm_clip_beg) ||
					(options.tm_clip_end_valid && tsn > options.tm_clip_end)) {
					use_sample = false;
				}
				//printf("ts= %f use= %d at %s %d\n", tsn, use_sample, __FILE__, __LINE__);
				if (use_sample) {
					prf_obj.samples.push_back(pss);
				}
#endif
			} else {
				char *hdr_typ_str = tc_decode_evt_hdr_typ(evt_hdr_typ);
				printf("unhandled evt_hdr_typ == 0x%x(%s), len= %d, tot_len= %d, pos_sv= %ld, pos= %ld at %s %d\n",
					evt_hdr_typ, hdr_typ_str, len, evt_len, pos_sv, pos, __FILE__, __LINE__);
				exit(1);
			}
		}
	}
	char *hdr_typ_str = tc_decode_evt_hdr_typ(evt_hdr_typ);
	if (verbose > 0) {
	printf("pos_sv= %ld, pos= %ld, evt_hdr typ= 0x%x(%s) rest= 0x%x, evt_len= %d, ts= %" PRIu64 " at %s %d\n",
		pos_sv, pos, evt_hdr_typ, hdr_typ_str,
		(evt_hdr - evt_hdr_typ), evt_len, ts, __FILE__, __LINE__);
	}

#if 1
	if (strcmp(hdr_typ_str, "unknown") == 0) {
		mm_read_n_bytes(mm_buf, pos_sv, 64, __LINE__, buf, BUF_MAX);
		hex_dump_n_bytes_from_buf(buf, 64, "err on evt_hdr_typ:", __LINE__);
		exit(1);
	}
#endif
	return evt_len;
}


int bth_ck_fmt(std::string lkfor, std::string fmt, int verbose)
{
	int last, rc;
	size_t n = fmt.find(lkfor);
	if (n == std::string::npos) {
		return 0;
	}
	std::vector <lst_fld_beg_end_str> fld_beg_end;
	std::string sub = fmt.substr(n);
	rc = ck_parens(sub.c_str(), last, fld_beg_end, verbose);
	if (verbose > 0) {
		printf("rc = %d, last= %d, len= %d str:\n%s\n", rc, last, (int)sub.size(), sub.c_str());
		for (int i=0; i < last; i++) {
			printf("%c", '0'+(i % 10));
		}
		printf("^\n");
		for (int i=0; i < last; i++) {
			if ((i%10) != 0) {
				printf(" ");
			} else {
				int j = i % 100;
				printf("%c", '0'+(j/10));
			}
		}
		printf("^\n");
	}
	std::string str_tst;
	for (uint32_t i=0; i < fld_beg_end.size(); i++) {
		uint32_t b = (uint32_t)(fld_beg_end[i].beg+1);
		uint32_t e = (uint32_t)fld_beg_end[i].end;
		if (b < sub.size() && b < e) {
			str_tst = sub.substr(b, e - b);
			if (verbose > 0)
				printf("fld[%d] = '%s'\n", i, str_tst.c_str());
		}
	}
	if (str_tst.size() > 0) {
		//fld_beg_end.resize(0);
		fld_beg_end.clear();
		std::string str_tst2;
		rc = ck_parens(str_tst.c_str(), last, fld_beg_end, verbose);
		for (uint32_t i=0; i < fld_beg_end.size(); i++) {
			uint32_t b = (uint32_t)(fld_beg_end[i].beg+1);
			uint32_t e = (uint32_t)fld_beg_end[i].end;
			if (b < sub.size() && b < e) {
				str_tst2 = str_tst.substr(b, e - b);
				if (verbose > 0)
					printf("sub_fld[%d] = '%s'\n", i, str_tst2.c_str());
			}
		}
	}
	return 0;
}

static int tc_add_event(int evt_id, std::string evt_str, std::string area, prf_obj_str &prf_obj, int verbose)
{
	prf_events_str pes;
	pes.event_name = evt_str;
	pes.pea.type = PERF_TYPE_TRACEPOINT;
	pes.pea.config = (uint64_t)evt_id;
	if (area.size() > 0) {
		pes.event_area = area;
	}
	if (verbose)
		printf("add tc event name= '%s', ID= %d, num_evts= %d\n", evt_str.c_str(), evt_id, (int)prf_obj.events.size());
	prf_add_ids((uint32_t)evt_id, (int)prf_obj.events.size(), prf_obj);
	prf_obj.events.push_back(pes);
	return (int)prf_obj.events.size();
}

static int tc_build_event(char *buf_in, const char *area, prf_obj_str &prf_obj, int verbose)
{
	char evt_str[256];
	int evt_id= -1;
	char *cp, *cp1;

	cp = strstr(buf_in, "name: ");
	if (!cp) {
		printf("didn't find 'name: ' string in buf= '%s'. Bye at %s %d\n", buf_in, __FILE__, __LINE__);
		exit(1);
	}
	cp += 6;
	cp1 = strchr(cp, '\n');
	if (!cp1) {
		printf("didn't find newline after 'name: ' string in buf= '%s'. Bye at %s %d\n", buf_in, __FILE__, __LINE__);
		exit(1);
	}
	int slen = (int)(cp1 - cp);
	evt_str[0] = 0;
	if (slen < (int)sizeof(evt_str)) {
		memcpy(evt_str, cp, (size_t)slen);
		evt_str[slen]=0;
	}
	cp = strstr(cp1, "ID: ");
	if (!cp) {
		printf("didn't find 'ID: ' string in buf= '%s'. Bye at %s %d\n", cp, __FILE__, __LINE__);
		exit(1);
	}
	cp += 4;
	evt_id = atoi(cp);
	std::string event_name = std::string(evt_str);
	std::string sarea;
	if (area) {
		sarea = std::string(area);
	}
	return tc_add_event(evt_id, event_name, sarea, prf_obj, verbose);
}

static int tc_parse_pid_buf(int buf_sz, char *buf_in, prf_obj_str &prf_obj, double tm, int verbose)
{
	char pid_str[256];
	int slen, pid_num= -1;
	char *cp, *cp1, *cp_end;

	cp = buf_in;
	cp_end = cp + buf_sz;
	if (verbose)
		printf("tc_parse_pid_buf: buf_sz= %d, strlen= %d\n", buf_sz, (int)strlen(buf_in));
	while(cp < cp_end && cp != NULL) {
		pid_num = atoi(cp);
		cp1 = strchr(cp, ' ');
		if (!cp1) { break;}
		cp1++;
		cp = strchr(cp1, '\n');
		if (cp) {
			slen = (int)(cp - cp1);
		} else {
			slen = (int)strlen(cp1);
		}
		if (slen > 16) {
			printf("ummm... pid strlen should be <= 16, got %d. ck logic at %s %d\n", slen, __FILE__, __LINE__);
			exit(1);
		}
		memcpy(pid_str, cp1, (size_t)slen);
		pid_str[slen] = 0;
		if (verbose)
			printf("%d %s\n", pid_num, pid_str);
		prf_add_comm((uint32_t)pid_num, (uint32_t)pid_num, std::string(pid_str), prf_obj, tm);
	}
	return 0;
}

int tc_read_data_bin(std::string flnm, int verbose, prf_obj_str &prf_obj, double tm_beg_in, prf_obj_str *prf_obj_prev)
{
	long pos = 0;

	int mm_idx = (int)mm_vec.size();
	prf_obj.mm_idx = mm_idx;
	mm_vec.push_back(new MemoryMapped(flnm, MemoryMapped::WholeFile, MemoryMapped::SequentialScan));
	if (!(mm_vec[mm_idx]->isValid()))   {
		printf("File not found\n");
		return -2;
	}
	prf_obj.mm_buf = mm_vec[mm_idx]->getData();
	const unsigned char* mm_buf = prf_obj.mm_buf;
	printf("filename %s, mm_buf= %p at %s %d\n", flnm.c_str(),  mm_buf, __FILE__, __LINE__);
#if 0
	file.open (flnm, std::ios::in | std::ios::binary);
	if (!file.is_open()) {
		printf("messed up fopen of flnm= %s at %s %d\n", flnm.c_str(), __FILE__, __LINE__);
		exit(1);
	}
#endif
	mm_read_n_bytes(mm_buf, pos, 10, __LINE__, buf, BUF_MAX);
#if 0
	if(!file.read (buf, 10)) {
		// An error occurred!
		// myFile.gcount() returns the number of bytes read.
		// calling myFile.clear() will reset the stream state
		// so it is usable again.
	}
#endif
	if (buf[0] == 0x17 && buf[1] == 0x08 && buf[2] == 0x44) {
		printf("got trace-cmd magic bytes 0-2\n");
	}
	if (memcmp(buf+3, "tracing", 7) == 0) {
		printf("got trace-cmd 'tracing' string\n");
	}
	prf_obj.filename_bin = flnm;
	pos = 10;
	//int i = read_till_null(file, pos, __LINE__);
	int i = mm_read_till_null(mm_buf, pos, __LINE__, buf, BUF_MAX);

	printf("got trace-cmd ver_str= %s, i=%d\n", buf, i);
	//i = read_n_bytes(file, pos, 1, __LINE__);
	i = mm_read_n_bytes(mm_buf, pos, 1, __LINE__, buf, BUF_MAX);
	int endian = buf[0];
	if (verbose)
		printf("got endian= %d\n", endian);
	if (endian != 0) {
		fprintf(stderr, "trace_cmd file %s is (probably) big endian... I don't have code for big endian. Expect problems. at %s %d\n",
				flnm.c_str(), __FILE__, __LINE__);
	}

	//i = read_n_bytes(file, pos, 1, __LINE__);
	i = mm_read_n_bytes(mm_buf, pos, 1, __LINE__, buf, BUF_MAX);
	int long_sz = buf[0];
	//if (verbose)
	printf("got long_sz= %d\n", long_sz);
	if (long_sz == 0) {
		printf("got invalid long sz= 0 at %s %d\n", __FILE__, __LINE__);
		exit(1);
	}

	//i = read_n_bytes(file, pos, 4, __LINE__);
	i = mm_read_n_bytes(mm_buf, pos, 4, __LINE__, buf, BUF_MAX);
	int page_sz = *(buf_int32_ptr(buf, 0));
	if (verbose)
		printf("got page_sz= %d\n", page_sz);

	//i = read_n_bytes(file, pos, 12, __LINE__);
	i = mm_read_n_bytes(mm_buf, pos, 12, __LINE__, buf, BUF_MAX);
	if (strcmp(buf, "header_page") == 0) {
		if (verbose)
			printf("got header_page at %s %d\n", __FILE__, __LINE__);
	} else {
		printf("missed header_page at %s %d\n", __FILE__, __LINE__);
		exit(1);
	}
	//i = read_n_bytes(file, pos, 8, __LINE__);
	i = mm_read_n_bytes(mm_buf, pos, 8, __LINE__, buf, BUF_MAX);
	long hdr_page_sz = *(buf_long_ptr(buf, 0));
	if (verbose)
		printf("got hdr_page_sz= %ld\n", hdr_page_sz);

	//i = read_n_bytes(file, pos, hdr_page_sz, __LINE__);
	i = mm_read_n_bytes(mm_buf, pos, hdr_page_sz, __LINE__, buf, BUF_MAX);
	buf[hdr_page_sz] = 0;
	std::string hdr_page = std::string(buf);
	if (verbose)
		printf("got hdr_page= %s\n", buf);

	//i = read_n_bytes(file, pos, 13, __LINE__);
	i = mm_read_n_bytes(mm_buf, pos, 13, __LINE__, buf, BUF_MAX);
	if (verbose)
		printf("got str= %s\n", buf);

	// see https://github.com/spotify/linux/blob/master/include/linux/ring_buffer.h for expl of ring buf hdr
	//i = read_n_bytes(file, pos, 8, __LINE__);
	i = mm_read_n_bytes(mm_buf, pos, 8, __LINE__, buf, BUF_MAX);
	long evt_hdr_info_sz = *(buf_long_ptr(buf, 0));
	if (verbose)
		printf("got evt_hdr_info_sz= %ld\n", evt_hdr_info_sz);

	//i = read_n_bytes(file, pos, evt_hdr_info_sz, __LINE__);
	i = mm_read_n_bytes(mm_buf, pos, evt_hdr_info_sz, __LINE__, buf, BUF_MAX);
	buf[evt_hdr_info_sz] = 0;
	std::string evt_hdr_info = std::string(buf);
	if (verbose)
		printf("got evt_hdr_info= %s\n", buf);

	//i = read_n_bytes(file, pos, 4, __LINE__);
	i = mm_read_n_bytes(mm_buf, pos, 4, __LINE__, buf, BUF_MAX);
	int num_ftrace_evts = *(buf_int32_ptr(buf, 0));
	if (verbose)
		printf("got num_ftrace_evts= %d\n", num_ftrace_evts);

	for (int j=0; j < num_ftrace_evts; j++) {
		//i = read_n_bytes(file, pos, 8, __LINE__);
		i = mm_read_n_bytes(mm_buf, pos, 8, __LINE__, buf, BUF_MAX);
		long evt_sz = *(buf_long_ptr(buf, 0));
		//i = read_n_bytes(file, pos, evt_sz, __LINE__);
		i = mm_read_n_bytes(mm_buf, pos, evt_sz, __LINE__, buf, BUF_MAX);
		buf[evt_sz] = 0;
		if (verbose)
			printf("got evt_sz= %ld, ftrace_evt_fmt= \n%s\n", evt_sz, buf);
		tc_build_event(buf, NULL, prf_obj, verbose);
	}

	std::vector < tc_fte_area_str > ft_areas;

	std::vector <tc_fte_fmt_str> ft_fmts;

	//i = read_n_bytes(file, pos, 4, __LINE__);
	i = mm_read_n_bytes(mm_buf, pos, 4, __LINE__, buf, BUF_MAX);
	int num_evt_systems = *(buf_int32_ptr(buf, 0));
	if (verbose)
		printf("got num_evt_systems= %d\n", num_evt_systems);

	int evt_cur=0;
	for (int j=0; j < num_evt_systems; j++) {
		//i = read_till_null(file, pos, __LINE__);
		i = mm_read_till_null(mm_buf, pos, __LINE__, buf, BUF_MAX);
		std::string area = std::string(buf);
		//i = read_n_bytes(file, pos, 4, __LINE__);
		i = mm_read_n_bytes(mm_buf, pos, 4, __LINE__, buf, BUF_MAX);
		int num_evts_in_area = *(buf_int32_ptr(buf, 0));
		if (verbose)
			printf("got num_evts_in_area= %d\n", num_evts_in_area);
		tc_fte_area_str fas;
		fas.area = area;
		fas.beg = evt_cur;
		fas.len = num_evts_in_area;
		ft_areas.push_back(fas);
		for (int k=0; k < num_evts_in_area; k++) {
			//i = read_n_bytes(file, pos, 8, __LINE__);
			i = mm_read_n_bytes(mm_buf, pos, 8, __LINE__, buf, BUF_MAX);
			long fmt_sz = *(buf_long_ptr(buf, 0));
			//i = read_n_bytes(file, pos, fmt_sz, __LINE__);
			i = mm_read_n_bytes(mm_buf, pos, fmt_sz, __LINE__, buf, BUF_MAX);
			buf[fmt_sz] = 0;
			tc_fte_fmt_str ffs;
			ffs.area = area;
			ffs.format = std::string(buf);
			bth_ck_fmt("__print_flags", ffs.format, verbose);
			ft_fmts.push_back(ffs);
			if (verbose)
				printf("area= %s, evt_fmt= %s\n", area.c_str(), buf);
			tc_build_event(buf, area.c_str(), prf_obj, verbose);
			evt_cur++;
		}
	}
	if (verbose)
		printf("got ft_areas.sz= %d, ft_fmts= %d\n", (int)ft_areas.size(), (int)ft_fmts.size());
#if 0
	const char * vogon_poem = R"V0G0N(
	__print_flags(REC->flags, "", { (1 << BH_New), "N" }, { (1 << BH_Mapped), "M" }, { (1 << BH_Unwritten), "U" }, { (1 << BH_Boundary), "B" })
	)V0G0N";
	std::string t1 = std::string(vogon_poem);
	bth_ck_fmt("__print_flags", t1, verbose);
#endif

	//i = read_n_bytes(file, pos, 4, __LINE__);
	i = mm_read_n_bytes(mm_buf, pos, 4, __LINE__, buf, BUF_MAX);
	int kallsyms_sz = *(buf_int32_ptr(buf, 0));
	printf("got kallsyms_sz= %d\n", kallsyms_sz);
	char *kallsyms_buf = (char *)malloc((size_t)(kallsyms_sz+1));

	i = mm_read_n_bytes_buf(mm_buf, pos, kallsyms_sz, kallsyms_buf, __LINE__);
	kallsyms_buf[kallsyms_sz] = 0;

	// below prints a lot of data
	//printf("got kallsyms= %s\n", kallsyms_buf);

	//i = read_n_bytes(file, pos, 4, __LINE__);
	i = mm_read_n_bytes(mm_buf, pos, 4, __LINE__, buf, BUF_MAX);
	int printk_fmt_sz = *(buf_int32_ptr(buf, 0));
	printf("got printk_fmt_sz= %d\n", printk_fmt_sz);
	//i = read_n_bytes(file, pos, printk_fmt_sz, __LINE__);
	i = mm_read_n_bytes(mm_buf, pos, printk_fmt_sz, __LINE__, buf, BUF_MAX);
	buf[printk_fmt_sz] = 0;
	// below printk formats are not too interesting
	//printf("printk_fmt= %s\n", buf);


	//i = read_n_bytes(file, pos, 8, __LINE__);
	i = mm_read_n_bytes(mm_buf, pos, 8, __LINE__, buf, BUF_MAX);
	long pid_to_proc_sz = *(buf_long_ptr(buf, 0));
	if (verbose)
		printf("\n\ngot pid_to_proc_sz= %ld\n", pid_to_proc_sz);
	char *pid_to_proc_buf = (char *)malloc((size_t)(pid_to_proc_sz+1));
	//i = read_n_bytes_buf(file, pos, pid_to_proc_sz, pid_to_proc_buf, __LINE__);
	i = mm_read_n_bytes_buf(mm_buf, pos, pid_to_proc_sz, pid_to_proc_buf, __LINE__);
	pid_to_proc_buf[pid_to_proc_sz] = 0;
	if (verbose)
		printf("pid_to_proc buf=\n%s\n", pid_to_proc_buf);
	tc_parse_pid_buf(pid_to_proc_sz, pid_to_proc_buf, prf_obj, 0.0, verbose);

	//i = read_n_bytes(file, pos, 4, __LINE__);
	i = mm_read_n_bytes(mm_buf, pos, 4, __LINE__, buf, BUF_MAX);
	int num_cpus = *(buf_int32_ptr(buf, 0));
	printf("got num_cpus= %d\n", num_cpus);
	prf_obj.features_nr_cpus_online = num_cpus;
	prf_obj.features_nr_cpus_available = num_cpus;

	//i = read_n_bytes(file, pos, 10, __LINE__);
	i = mm_read_n_bytes(mm_buf, pos, 10, __LINE__, buf, BUF_MAX);
	printf("got option= %s\n", buf);

	std::vector <tc_opts_str> tc_opts;

	if (strcmp(buf, "options  ") == 0) {
		printf("got options str\n");
read_opt_again:
		//i = read_n_bytes(file, pos, 2, __LINE__);
		i = mm_read_n_bytes(mm_buf, pos, 2, __LINE__, buf, BUF_MAX);
		int cur_opt = *(buf_uint16_ptr(buf, 0));
		printf("cur_opt= %d\n", cur_opt);
		if (cur_opt != 0) {
			tc_opts_str tos;
			tos.opt = cur_opt;
			//i = read_n_bytes(file, pos, 4, __LINE__);
			i = mm_read_n_bytes(mm_buf, pos, 4, __LINE__, buf, BUF_MAX);
			int cur_opt_len = (int)*(buf_uint32_ptr(buf, 0));
			printf("cur_opt_sz= %d\n", cur_opt_len);
			//i = read_n_bytes(file, pos, cur_opt_len, __LINE__);
			i = mm_read_n_bytes(mm_buf, pos, cur_opt_len, __LINE__, buf, BUF_MAX);
			buf[cur_opt_len] = 0;
			printf("\topt_str?= %s\n", buf);
			tos.str = std::string(buf);
			if (cur_opt == 5) {
				/*
				tc shows: Linux x86_64_laptop 4.10.0+ x86_64
				which seems to be from
				  pfay@x86_64_laptop:~/ppat$ uname -snrm
				  Linux x86_64_laptop 4.10.0+ x86_64
				probably should try to parse this into its pieces but...
				*/
				prf_obj.features_cpudesc = prf_obj.features_arch = prf_obj.features_hostname = tos.str;
				printf("tc: prf_obj hostname= '%s'\n", tos.str.c_str());
			}
			tc_opts.push_back(tos);
			goto read_opt_again;
		}
		//i = read_n_bytes(file, pos, 10, __LINE__);
		i = mm_read_n_bytes(mm_buf, pos, 10, __LINE__, buf, BUF_MAX);
	}
	printf("got next option= %s\n", buf);


	std::vector <tc_cpu_data_str> tc_cpu_data;

	if (strcmp(buf, "flyrecord") == 0) {
		for (int j= 0; j < num_cpus; j++) {
			tc_cpu_data_str tcds;
			//i = read_n_bytes(file, pos, 8, __LINE__);
			i = mm_read_n_bytes(mm_buf, pos, 8, __LINE__, buf, BUF_MAX);
			long cpu_off = *(buf_long_ptr(buf, 0));
			//i = read_n_bytes(file, pos, 8, __LINE__);
			i = mm_read_n_bytes(mm_buf, pos, 8, __LINE__, buf, BUF_MAX);
			long cpu_len = *(buf_long_ptr(buf, 0));
			if (verbose)
				printf("cpu[%d] offset= %ld, len= %ld\n", j, cpu_off, cpu_len);
			tcds.off = cpu_off;
			tcds.len = cpu_len;
			tc_cpu_data.push_back(tcds);
		}
	}

	if (verbose)
		printf("hdr_page=\n%s\n", hdr_page.c_str());
	if (verbose)
		printf("hdr_evnt=\n%s\n", evt_hdr_info.c_str());
	for (uint32_t j=0; j < tc_cpu_data.size(); j++) {
		pos = tc_cpu_data[j].off;
		i = mm_read_n_bytes(mm_buf, pos, 64, __LINE__, buf, BUF_MAX);
		if (verbose)
			hex_dump_n_bytes_from_buf(buf, 64, "page_hdr on evt_hdr_typ:", __LINE__);
		//hex_dump_bytes(file, tc_cpu_data[j].off, 64, "page_hdr on evt_hdr_typ:", __LINE__);
		//file.seekg(tc_cpu_data[j].off);
		pos = tc_cpu_data[j].off;
#if 0
		int sz = 16;
		i = read_n_bytes(file, pos, sz, __LINE__);
		uint64_t ts = *(buf_uint64_prt(buf, 0));
		printf("cpu[%d] ts= %lu\n", j, ts);
#endif

		long evt_data_read = 0;
		uint64_t ts = 0;
		while (evt_data_read < tc_cpu_data[j].len) {
			evt_data_read += tc_read_evt(tc_cpu_data[j].len+tc_cpu_data[j].off, (int)j, page_sz, mm_buf, pos, ts, __LINE__, prf_obj, verbose, prf_obj_prev);
		}
	}
	std::sort(prf_obj.samples.begin(), prf_obj.samples.end(), compareByTime);

	//file.close();
	return 0;
}

int tp_read_event_formats(std::string flnm, int verbose)
{
	std::ifstream file;
	//file.open(flnm, std::ifstream::in);
	file.open(flnm.c_str());
	if (!file.is_open()) {
		printf("messed up fopen of flnm= %s at %s %d\n", flnm.c_str(), __FILE__, __LINE__);
		exit(1);
	}
	std::string str, ev_file, fstr;
	lst_ft_fmt_str ffs;
	int doing_common=0;
	while (std::getline(file, str))
	{
		if (verbose > 0)
		printf("line len %zu : %s\n", str.size(), str.c_str());
		// will test below break if debug dir is mounted to /debug/tracing ?
		//if (str.find("flnm=/sys/kernel/debug/tracing") != std::string::npos)
		if (str.find("flnm=/") == 0) {
			doing_common = -1;
			//ffs.per_fld.resize(0);
			//ffs.fields.resize(0);
			ffs.per_fld.clear();
			ffs.fields.clear();
			ev_file = str;
			str = str.substr(0, str.size()-7);
			size_t n = str.find_last_of("/");
			if (n == std::string::npos || n == 0) {
				printf("mess up here at %s %d\n", __FILE__, __LINE__);
				exit(1);
			}
			std::string evt = str.substr(n+1);
			str = str.substr(0, n);
			n = str.find_last_of("/");
			if (n <= 0) {
				printf("mess up here at %s %d\n", __FILE__, __LINE__);
				exit(1);
			}
			std::string area = str.substr(n+1);
			ffs.area = area;
			ffs.event = evt;
			if (verbose > 0)
			printf("===sys: area= %s, evt= %s, full= %s\n", area.c_str(), evt.c_str(), ev_file.c_str());
		} else if (str.find("ID: ") == 0) {
			ffs.id = atoi(str.substr(4).c_str());
			tp_event_str tes;
			tes.id = ffs.id;
			tes.area = ffs.area;
			tes.event = ffs.event;
			tp_events.push_back(tes);
			tp_id_2_event_indxp1[ffs.id] = (int)tp_events.size();
		}
		size_t j = str.find("\tfield:");
		if (doing_common == 1 && j == std::string::npos) {
			// will there always be a blank line between the common fields and the rest?
			doing_common = 2;
		}
		if (j == 0) {
			//field:unsigned char common_preempt_count;	offset:3;	size:1;	signed:0;
			lst_fld_str fs;
			if (doing_common == -1) {
				doing_common = 1;
			}
			str = str.substr(7);
			fs.common = (uint32_t)doing_common;
			for (int i=0; i < 4; i++) {
				size_t n = str.find(";");
				std::string sb = str.substr(0, n);
				if (i == 0) {
					std::string s0 = sb;
					size_t m;

					fs.arr_sz = 0;
					if (s0.size() > 0 && s0.substr(s0.size()-1) == "]") {
						m = s0.find_last_of("[");
						if (m != std::string::npos) {
							fs.arr_sz = (uint32_t)atoi(s0.substr(m+1).c_str());
							s0 = s0.substr(0, m);
						}
					}
					m = s0.find_last_of(" ");
					if (m != std::string::npos) {
						fs.name = s0.substr(m+1);
						fs.typ = s0.substr(0, m);
					} else {
						fs.name = s0;
					}
					str = str.substr(n+1);
				}
				else if (i == 1) {
					size_t m = sb.find("offset:");
					if (m != std::string::npos) {
						fs.offset = (uint32_t)atoi(sb.substr(m+7).c_str());
					} else {
						printf("str= '%s', error at %s %d\n", str.c_str(), __FILE__, __LINE__);
						exit(1);
					}
					str = str.substr(n+1);
				}
				else if (i == 2) {
					size_t m = sb.find("size:");
					if (m != std::string::npos) {
						fs.size = (uint32_t)atoi(sb.substr(m+5).c_str());
					} else {
						printf("str= '%s', error at %s %d\n", str.c_str(), __FILE__, __LINE__);
						exit(1);
					}
					str = str.substr(n+1);
				}
				else if (i == 3) {
					size_t m = sb.find("signed:");
					if (m != std::string::npos) {
						fs.sgned = (uint32_t)atoi(sb.substr(m+7).c_str());
					} else {
						printf("str= '%s', error at %s %d\n", str.c_str(), __FILE__, __LINE__);
						exit(1);
					}
					//str = str.substr(n+1);
				}
			}
			if (verbose > 0)
			printf("got field: cmn= %d, typ= '%s', name= '%s', arr_sz= %d, offset= %d, size= %d, signed= %d\n",
				fs.common, fs.typ.c_str(), fs.name.c_str(), fs.arr_sz, fs.offset, fs.size, fs.sgned);
			ffs.fields.push_back(fs);
		}
		if (str.find("print fmt") != std::string::npos) {
			size_t n;
			//bth_ck_fmt("__print_flags", str);
			bth_ck_fmt("__print_symbolic", str, verbose);
			fstr = "";
			n = str.find("\", ");
			if (n != std::string::npos && n > 0) {
				fstr = str.substr(11, n-10);
				//printf("fstr= '%s'\n", fstr.c_str());
			} else {
				if (str == "print fmt: \"\"") {
					fstr = str.substr(11, n-10);
					//printf("===fstr= '%s'\n", fstr.c_str());
				} else {
				std::string str1, str2;
				n = str.find("\"");
				str1 = str.substr(11, n-10);
				std::getline(file, str);
				size_t n2 = str.find("\"");
	  			str2 = str.substr(0, n2+1);
				fstr = str1 + str2;
				//printf("====fstr= '%s', str1= %s, str2= %s\n", fstr.c_str(), str1.c_str(), str2.c_str());
				}
			}
			ffs.fmt = fstr;
			buf[0] = 0;
			if (fstr.size() < 2 || fstr[0] != '"' || fstr[fstr.size()-1] != '"') {
				printf("something horribly wrong with fstr= '%s' at %s %d\n", fstr.c_str(), __FILE__, __LINE__);
				exit(1);
			}
			// some (4 on my ubuntu x86 laptop) events use trace_seq_printf to create a whole diff print string... what can you do
			// see for example /sys/kernel/debug/tracing/events/kvmmmu/kvm_mmu_sync_page/format
			int got_flds = my_vsnprintf_dbg(buf, sizeof(buf), fstr.substr(1, fstr.size()-2).c_str(), ffs.per_fld);
			if (verbose > 0)
				printf("ret_fmt flds= %d, vsz= %d str= %s\n", got_flds, (int)ffs.per_fld.size(), buf);
			for (uint32_t i=0; i < ffs.per_fld.size(); i++) {
				std::string flgs;
				flags_decode(ffs.per_fld[i].flags, flgs);
				if (verbose > 0)
				printf("fld[%d].prefix= '%s', flgs= %s\n", i, ffs.per_fld[i].prefix.c_str(), flgs.c_str());
			}
			lst_ft_fmt_vec.push_back(ffs);

		}
	}
	file.close();
	return 0;
}


static uint32_t add_evt_and_cols(prf_obj_str &prf_obj, std::string event_name, std::vector <std::string> cols, int verbose)
{
	int evt_id = 10000 + prf_obj.events.size(); // add a bogus number (10000) for the id
	std::string area;
	uint32_t evt_idx = tc_add_event(evt_id, event_name, area, prf_obj, verbose) - 1;
	prf_obj.events[evt_idx].new_cols.resize(cols.size());
	prf_obj.events[evt_idx].new_cols = cols;
	return evt_idx;
}


static uint32_t ck_for_match_on_event_name(std::string evt, prf_obj_str &prf_obj, int verbose)
{
	uint32_t hsh_ck = UINT32_M1;
	std::string evt_w_area = evt;
	std::string evt_wo_area = evt;
	size_t pos = evt_wo_area.find(":");
	if (pos != std::string::npos && evt_wo_area.size() > (pos+1)) {
		evt_wo_area = evt_wo_area.substr(pos+1);
		printf("evt_wo_area= %s at %s %d\n", evt_wo_area.c_str(), __FILE__, __LINE__);
	}
	for (uint32_t k=0; k < prf_obj.events.size(); k++) {
		if (verbose)
			printf("ck derived evt match on nm= %s vs evt_w_area[%d]= %s and evt= %s at %s %d\n",
				evt_w_area.c_str(), k, prf_obj.events[k].event_name_w_area.c_str(),
				prf_obj.events[k].event_name.c_str(), __FILE__, __LINE__);
		if (prf_obj.events[k].event_name_w_area == evt_w_area ||
			prf_obj.events[k].event_name == evt_w_area ||
			prf_obj.events[k].event_name == evt_wo_area) {
			printf("got derived evt match on nm= %s at %s %d\n",
				prf_obj.events[k].event_name.c_str(), __FILE__, __LINE__);
			hsh_ck = k;
			break;
		}
	}
	if (verbose)
		fflush(NULL);
	return hsh_ck;
}

static uint32_t ck_got_evts_derived_dependents(prf_obj_str &prf_obj,  evt_str &evt_tbl2,
		bool do_trigger, evts_derived_str &eds, int verbose)
{
	if (do_trigger) {
		return ck_for_match_on_event_name(evt_tbl2.evt_derived.evt_trigger, prf_obj, verbose);
	}

	uint32_t hsh_ck = UINT32_M1;
	for (uint32_t j=0; j < evt_tbl2.evt_derived.evts_used.size(); j++) {
		hsh_ck = ck_for_match_on_event_name(evt_tbl2.evt_derived.evts_used[j], prf_obj, verbose);
		if (hsh_ck == UINT32_M1) {
			return UINT32_M1;
		}
		eds.evts_used.push_back(hsh_ck);
	}
	return hsh_ck;
}

void ck_evts_derived(prf_obj_str &prf_obj, std::vector <evt_str> &evt_tbl2,
		std::vector <evts_derived_str> &evts_derived, int verbose)
{

	for (uint32_t i=0; i < evt_tbl2.size(); i++) {
		if (evt_tbl2[i].event_type == "ftrace" && evt_tbl2[i].evt_derived.evts_used.size() > 0) {
			bool okay= true;
			evts_derived_str eds;
			eds.evts_used.resize(0);
			printf("ck derived evt= %s at %s %d\n", evt_tbl2[i].event_name.c_str(), __FILE__, __LINE__);
			fflush(NULL);
			uint32_t hsh_ck = ck_got_evts_derived_dependents(prf_obj,  evt_tbl2[i], false, eds, verbose);
			if (hsh_ck == UINT32_M1) {
				okay = false;
			}
			uint32_t trgr_idx = UINT32_M1;
			if (okay) {
				trgr_idx = ck_got_evts_derived_dependents(prf_obj,  evt_tbl2[i], true, eds, verbose);
				if (trgr_idx == UINT32_M1) {
					okay = false;
				}
				printf("derived_evt got trigger idx= %d at %s %d\n", trgr_idx, __FILE__, __LINE__);
			}
			if (okay) {
				std::string new_nm  = evt_tbl2[i].event_name;
				std::vector <std::string> tkns;
				for (uint32_t j=0; j < evt_tbl2[i].evt_derived.new_cols.size(); j++) {
					tkns.push_back(evt_tbl2[i].evt_derived.new_cols[j]);
				}
				uint32_t new_idx = add_evt_and_cols(prf_obj, new_nm, tkns, verbose);
				eds.evt_tbl2_idx = i;
				eds.trigger_idx = trgr_idx;
				eds.evt_new_idx = new_idx;
				evts_derived.push_back(eds);
				for (uint32_t j=0; j < evt_tbl2[i].evt_derived.new_cols.size(); j++) {
					evts_derived.back().new_cols.push_back(evt_tbl2[i].evt_derived.new_cols[j]);
				}
				printf("trigger= %s, tkns[0]= %s, new_nm= %s trg_idx= %d, new_idx= %d evts_der.back().used.sz= %d at %s %d\n",
					evt_tbl2[i].evt_derived.evt_trigger.c_str(), tkns[0].c_str(), new_nm.c_str(), trgr_idx, new_idx,
					(int)evts_derived.back().evts_used.size(), __FILE__, __LINE__);
			}
		}
	}
	return;
}

static double tm_lua_derived = 0.0;

void ck_if_evt_used_in_evts_derived(int mtch, prf_obj_str &prf_obj, int verbose, std::vector <evt_str> &evt_tbl2,
	std::vector <prf_samples_str> &samples, std::vector <evts_derived_str> &evts_derived)
{
	bool got_it = false;
	for (uint32_t j=0; j < evts_derived.size(); j++) {
		uint32_t i = mtch;
		uint32_t tbl2_idx = evts_derived[j].evt_tbl2_idx;
		uint32_t trig_idx = evts_derived[j].trigger_idx;
		uint32_t new_idx = evts_derived[j].evt_new_idx;
		uint32_t evt_idx = prf_obj.samples[i].evt_idx;
		std::vector <std::string> &cols = prf_obj.events[evt_idx].etw_cols;
		for (uint32_t k=0; k < evts_derived[j].evts_used.size(); k++) {
			if (evts_derived[j].evts_used[k] == evt_idx) {
				std::vector <std::string> tkns;
				//printf("got derived evt= %s at %s %d\n", prf_obj.samples[i].event.c_str(), __FILE__, __LINE__);
				uint32_t new_sz = evts_derived[j].new_cols.size();
				uint32_t old_sz = tkns.size();
				got_it = true;
				//printf("got evts_used= %s ts %s, at %s %d\n", tkns[0].c_str(), tkns[1].c_str(), __FILE__, __LINE__);
				std::string lua_file = evt_tbl2[tbl2_idx].evt_derived.lua_file;
				std::string lua_rtn  = evt_tbl2[tbl2_idx].evt_derived.lua_rtn;
				std::vector <std::string> new_vals;
				new_vals.resize(new_sz);

				double tm_beg = dclock();
				lua_derived_tc_prf(lua_file, lua_rtn, prf_obj.events[new_idx].event_name, prf_obj.samples[i],
						evts_derived[j].new_cols, new_vals, verbose);
				double tm_end = dclock();
				tm_lua_derived += tm_end - tm_beg;
				if (trig_idx == evt_idx) {
#if 1
					samples.push_back(prf_obj.samples[mtch]);
					samples.back().event      = prf_obj.events[new_idx].event_name;
					samples.back().orig_order++;
					samples.back().evt_idx = new_idx;
					samples.back().new_vals.resize(new_vals.size());
					samples.back().new_vals = new_vals;
#endif
#if 0
					for (uint32_t kk=0; kk < new_sz; kk++) {
						printf("new_vals[%d]= '%s' at %s %d\n",
							kk, samples.back().new_vals[kk].c_str(), __FILE__, __LINE__);
					}
#endif
					if (verbose > 0)
						printf("got trigger event %s at %s %d\n",
								prf_obj.events[new_idx].event_name.c_str(), __FILE__, __LINE__);
				}
				//run_heapchk("etw_parse:", __FILE__, __LINE__, 1);
				break;
			}
		}
		if (got_it) {
			break;
		}
	}
}

int tc_parse_text(std::string flnm, prf_obj_str &prf_obj, double tm_beg_in, int verbose, std::vector <evt_str> &evt_tbl2)
{
	std::ifstream file;
	//long pos = 0;
	std::string line;
	int lines_comments = 0, lines_samples = 0, lines_callstack = 0, lines_null=0;
	int samples_count = -1, s_idx = -1;
	std::vector <evts_derived_str> evts_derived;
	std::vector <prf_samples_str> samples;

	struct nms_str {
		std::string str;
		int count, len, ext_strs, evt;
	};

	std::vector <nms_str> nms;

	for (int i=0; i < (int)prf_obj.events.size(); i++) {
		struct nms_str ns;
		ns.str = " " + prf_obj.events[i].event_name + ": ";
		ns.count = 0;
		ns.ext_strs = 0;
		ns.evt = i;
		ns.len = ns.str.size();
		nms.push_back(ns);
	}
	ck_evts_derived(prf_obj, evt_tbl2, evts_derived, verbose);

	prf_obj.tm_beg = 1.0e-9 * (double)prf_obj.samples[0].ts;
	prf_obj.tm_end = 1.0e-9 * (double)prf_obj.samples.back().ts;
	prf_obj.filename_text = flnm;
	int store_callstack_idx = -1;
	double tm_beg = dclock();
	file.open (flnm.c_str(), std::ios::in);
	if (!file.is_open()) {
		printf("messed up fopen of flnm= %s at %s %d\n", flnm.c_str(), __FILE__, __LINE__);
		exit(1);
	}
	double tm_lua = 0, tm_lua_iters=0, tm_parse=0;
	double tm_lua_der0 = tm_lua_derived;
	double tm_parse_beg = dclock();
	int64_t line_num = 0;
	std::string unknown_mod;
	while(!file.eof()) {
		//read data from file
		std::getline (file, line);
		line_num++;
		int sz = line.size();
		if (sz > 5 && line.substr(0, 5) == "cpus=") {
			//lines_comments++;
		} else if (sz > 0 && line[0] == '#') {
			lines_comments++;
		} else if (sz > 0 && line[0] == '\t') {
			lines_callstack++;
			size_t pos = line.find_first_not_of(" \t");
			if (pos != std::string::npos) {
				size_t pos2 = line.substr(pos).find(' ');
				std::string rtn, module;
				if (pos2 != std::string::npos) {
					std::string nstr = line.substr(pos+pos2+1);
					size_t pos3 = nstr.find("+0x");
					if (pos3 != std::string::npos) {
						rtn = nstr.substr(0, pos3);
						size_t pos4 = nstr.substr(pos3).find(" ");
						if (pos4 != std::string::npos) {
							module = nstr.substr(pos3+pos4+1);
						}
					} else {
						pos3 = nstr.find(" ");
						if (pos3 != std::string::npos) {
							rtn = nstr.substr(0, pos3);
							size_t pos4 = nstr.substr(pos3).find(" ");
							if (pos4 != std::string::npos) {
								module = nstr.substr(pos3+pos4+1);
							}
						}
					}
					int msz = module.size();
					if (msz > 2 && module.substr(0, 1) == "(" && module.substr(msz-1, 1) == ")") {
						module = module.substr(1, msz - 2);
					}
					if (rtn == "[unknown]") {
						if (msz > 0 && module == unknown_mod) {
							// if we have more than 1 unknown rtns for a module then don't add
							continue;
						}
						unknown_mod = module;
					}
					if (store_callstack_idx > -1) {
						struct prf_callstack_str pcs;
						pcs.mod = module;
						pcs.rtn = rtn;
						prf_obj.samples[store_callstack_idx].callstack.push_back(pcs);
					}
					//printf("cs rtn= %s, mod= %s\n", rtn.c_str(), module.c_str());
				}
			}
		} else if (sz == 0 ) {
			lines_null++;
		} else {
			lines_samples++;
			store_callstack_idx = -1;
			samples_count++;
			unknown_mod = "";
			int evt = -1;
			std::string extra_str;
			for (uint32_t i=0; i < nms.size(); i++) {
				size_t pos = line.find(nms[i].str);
				if (pos != std::string::npos) {
					size_t pos2 = line.substr(pos + nms[i].len).find_first_not_of(' ');
					evt = nms[i].evt;
					if (pos2 != std::string::npos) {
						nms[evt].ext_strs++;
						extra_str = line.substr(pos + nms[i].len + pos2);
						//printf("extra_str= '%s' at %s %d\n", extra_str.c_str(), __FILE__, __LINE__);
					}
					nms[evt].count++;
					break;
				}
			}
			if (evt == -1) {
				printf("missed event lookup for line= %s at %s %d\n",
					line.c_str(), __FILE__, __LINE__);
				exit(1);
			}
			int i_beg = s_idx, sz = prf_obj.samples.size();
			if (i_beg < 0) {
				i_beg = 0;
			}
			if (sz > (i_beg+100)) {
				sz = i_beg + 100;
			}
			int mtch=-1, nxt=-1;
			for (int i=i_beg; i < sz; i++) {
				//printf("ck tm[%d]= %s, line= %s\n", i, prf_obj.samples[i].tm_str.c_str(), line.c_str());
				if (line.find(prf_obj.samples[i].tm_str) != std::string::npos) {
					s_idx = i;
					store_callstack_idx = i;
					nxt = i+1;
					mtch = i;
					if (extra_str.size() > 0) {
						//prf_obj.samples[i].args.push_back(extra_str);
						prf_obj.samples[i].extra_str = extra_str;
					}
					prf_obj.samples[i].line_num = line_num;
					prf_obj.samples[i].evt_idx = evt;
					//printf("got tm[%d]= %s, line= %s\n", i, prf_obj.samples[i].tm_str.c_str(), line.c_str());
					break;
				}
			}
			if (options.tm_clip_beg_valid && mtch == -1) {
				continue;
			}
			if (mtch == -1) {
				std::string tm;
				if (s_idx >= 0 && s_idx < (int)prf_obj.samples.size()) {
					tm = prf_obj.samples[s_idx].tm_str;
				}
				printf("missed samples %d, s_idx= %d, i_beg= %d, tm[%d]= '%s', line= %s\nsz= %d at %s %d",
					samples_count, s_idx, i_beg, s_idx, tm.c_str(), line.c_str(), sz, __FILE__, __LINE__);
				exit(1);
			}
			if (s_idx == -1) {
				s_idx = -2;
			}
			if (nxt != -1) {
				s_idx = nxt;
			}
			if (mtch != -1) {
#if 0
				printf("line= %s, evt= %s, tm_str= %s at %s %d\n",
						line.c_str(),
						prf_obj.samples[mtch].event.c_str(),
						prf_obj.samples[mtch].tm_str.c_str(), __FILE__, __LINE__);
#endif
				double tm_lua_beg = dclock();
				ck_if_evt_used_in_evts_derived(mtch, prf_obj, verbose, evt_tbl2, samples, evts_derived);
				double tm_lua_end = dclock();
				tm_lua += tm_lua_end - tm_lua_beg;
				tm_lua_iters++;
			}
		}
	}
	double tm_parse_end = dclock();
	double tm_lua_der1 = tm_lua_derived;
	double tm_parse_dff = tm_parse_end - tm_parse_beg;
	double tm_per_iter  = 1.0e6 * (tm_lua_iters > 0.0 ? tm_parse_dff / tm_lua_iters : 0.0);
	fprintf(stderr, "tc_parse_text tm_parse= %f, tm_lua= %f iters= %.0f usecs/iter= %f lua_rtn= %f at %s %d\n",
			tm_parse_end-tm_parse_beg, tm_lua, tm_lua_iters, tm_per_iter, tm_lua_der1-tm_lua_der0,  __FILE__, __LINE__);
	file.close();
	if (evts_derived.size() > 0 && samples.size() > 0) {
		double tm_cpy_beg = dclock();
		for (uint32_t i=0; i < samples.size(); i++) {
			prf_obj.samples.push_back(samples[i]);
		}
		std::sort(prf_obj.samples.begin(), prf_obj.samples.end(), compareByTime);
		double tm_cpy_end = dclock();
		printf("tc_parse_text: samples copy tm= %f at %s %d\n", tm_cpy_end-tm_cpy_beg, __FILE__, __LINE__);
	}
	double tm_end = dclock();
	printf("tc_parse_text: tm_end - tm_beg = %f, tm from begin= %f\n", tm_end - tm_beg, tm_end - tm_beg_in);
	printf("cmmnts= %d, samples= %d, callstack= %d, null= %d\n",
		lines_comments, lines_samples, lines_callstack, lines_null);
	for (uint32_t i=0; i < nms.size(); i++) {
		int evt = nms[i].evt;
		printf("event[%d]: %s, count= %d, extra_strings= %d\n",
			evt, prf_obj.events[evt].event_name.c_str(), nms[evt].count, nms[evt].ext_strs);
	}
	return 0;
}

