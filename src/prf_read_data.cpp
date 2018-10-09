/* Copyright (c) 2018 Patrick Fay
 *
 * License http://opensource.org/licenses/mit-license.php MIT License
 */

/*
 * perf references:
 *   https://github.com/torvalds/linux/blob/master/tools/perf/util/header.h
 *   https://www.slideshare.net/chimerawang/perf-file-format
 *   https://elixir.bootlin.com/linux/latest/source/include/uapi/linux/perf_event.h
 *   https://openlab-mu-internal.web.cern.ch/openlab-mu-internal/03_Documents/3_Technical_Documents/Technical_Reports/2011/Urs_Fassler_report.pdf
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
#include "rd_json.h"
#include "printf.h"
#include "oppat.h"
#include "lua_rtns.h"
#include "utils.h"
#include "ck_nesting.h"
#include "web_api.h"
#include "MemoryMapped.h"
#include "mygetopt.h"
#include "dump.hpp"
#include "tc_read_data.h"
#define PRF_RD_DATA_CPP
#include "prf_read_data.h"


#define BUF_MAX 4096
static char buf[BUF_MAX];

int prf_parse_text(std::string flnm, prf_obj_str &prf_obj, double tm_beg_in, int verbose, std::vector <evt_str> &evt_tbl2)
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
		int count, len, ext_strs;
	};

	std::vector <nms_str> nms;
	prf_obj.tm_beg = 1.0e-9 * (double)prf_obj.samples[0].ts;
	prf_obj.tm_end = 1.0e-9 * (double)prf_obj.samples.back().ts;
	std::vector <std::string> cpu_strs;

	for (uint32_t i=0; i < prf_obj.features_nr_cpus_online; i++) {
		char cstr[32];
		snprintf(cstr, sizeof(cstr), " [%03d] ", i);
		cpu_strs.push_back(std::string(cstr));
	}
	for (uint32_t i=0; i < prf_obj.features_event_desc.size(); i++) {
		struct nms_str ns;
		ns.str = " " + prf_obj.features_event_desc[i].event_string + ": ";
		ns.count = 0;
		ns.ext_strs = 0;
		ns.len = ns.str.size();
		nms.push_back(ns);
	}
	printf("bef prf_read_data ck_evts_derived at %s %d\n", __FILE__, __LINE__);
	ck_evts_derived(prf_obj, evt_tbl2, evts_derived, verbose);
	printf("aft prf_read_data ck_evts_derived at %s %d\n", __FILE__, __LINE__);

	prf_obj.filename_text = flnm;
	int store_callstack_idx = -1;
	double tm_beg = dclock();
	file.open (flnm.c_str(), std::ios::in);
	if (!file.is_open()) {
		printf("messed up fopen of flnm= %s at %s %d\n", flnm.c_str(), __FILE__, __LINE__);
		exit(1);
	}
	double tm_lua = 0, tm_lua_iters=0;
	int64_t line_num = 0;
	std::string unknown_mod;
	while(!file.eof()) {
		//read data from file
		std::getline (file, line);
		line_num++;
		int sz = line.size();
		if (sz > 0 && line[0] == '#') {
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
					if (pos2 != std::string::npos) {
						nms[i].ext_strs++;
						extra_str = line.substr(pos + nms[i].len + pos2);
					}
					nms[i].count++;
					evt = i;
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
#if 0
			static bool need_one_time_tm_beg_clip = true;
			if (options.tm_clip_beg_valid && need_one_time_tm_beg_clip) {
				sz = prf_obj.samples.size();
				need_one_time_tm_beg_clip = false;
				printf("bumped sz= %d at %s %d\n", sz, __FILE__, __LINE__);
			}
#endif
			int mtch=-1, nxt=-1;
			if (s_idx == -1 && i_beg == 0) {
				printf("line[0].tm= %s at %s %d\n", prf_obj.samples[0].tm_str.c_str(), __FILE__, __LINE__);
			}
			for (int i=i_beg; i < sz; i++) {
				if (line[0] == '\t') {
					continue;
				}
				if (line.find(prf_obj.samples[i].tm_str) != std::string::npos) {
					s_idx = i;
					nxt = i+1;
					store_callstack_idx = i;
					mtch = i;
					if (extra_str.size() > 0) {
						prf_obj.samples[i].extra_str = extra_str;
					}
					prf_obj.samples[i].line_num = line_num;
					prf_obj.samples[i].evt_idx = evt;
					//printf("ck tm[%d]= %s, line= %s\n", i, prf_obj.samples[i].tm_str.c_str(), line.c_str());
					break;
				}
#if 1
				//printf("lkfor event= '%s' line= %s at %s %d\n", prf_obj.samples[i].event.c_str(), line.c_str(), __FILE__, __LINE__);
				if (line.find(prf_obj.samples[i].event) != std::string::npos) {
					uint32_t cpu = prf_obj.samples[i].cpu;
					//printf("lkfor cpu= '%s' at %s %d\n", cpu_strs[cpu].c_str(), __FILE__, __LINE__);
					if (cpu < cpu_strs.size()) {
						if (line.find(cpu_strs[cpu]) != std::string::npos) {
							s_idx = i;
							nxt = i+1;
							store_callstack_idx = i;
							mtch = i;
							if (extra_str.size() > 0) {
								prf_obj.samples[i].extra_str = extra_str;
							}
							prf_obj.samples[i].line_num = line_num;
							prf_obj.samples[i].evt_idx = evt;
							break;
						}
					}
				}
#endif
				printf("missed line= '%s' at %s %d\n", line.c_str(), __FILE__, __LINE__);
			}
			if (mtch == -1 && options.tm_clip_beg_valid == 1) {
				continue;
			}
			if (mtch == -1) {
				std::string tm;
				if (s_idx >= 0 && s_idx < (int)prf_obj.samples.size()) {
					tm = prf_obj.samples[s_idx].tm_str;
				}
				printf("missed samples %d, s_idx= %d, i_beg= %d, tm[%d]= '%s', line= %s\n",
					samples_count, s_idx, i_beg, s_idx, tm.c_str(), line.c_str());
				exit(1);
			}
			if (s_idx == -1) {
				s_idx = -2;
			}
			if (nxt != -1) {
				s_idx = nxt;
			}
			if (mtch != -1) {
				double tm_lua_beg = dclock();
				ck_if_evt_used_in_evts_derived(mtch, prf_obj, verbose, evt_tbl2, samples, evts_derived);
				double tm_lua_end = dclock();
				tm_lua += tm_lua_end - tm_lua_beg;
				tm_lua_iters++;
			}
		}
	}
	file.close();
	if (tm_lua > 0.0) {
		fprintf(stderr, "prf_parse_text tm_lua= %f iters= %.0f at %s %d\n", tm_lua, tm_lua_iters, __FILE__, __LINE__);
	}
	if (evts_derived.size() > 0 && samples.size() > 0) {
		double tm_cpy_beg = dclock();
		for (uint32_t i=0; i < samples.size(); i++) {
			prf_obj.samples.push_back(samples[i]);
		}
		std::sort(prf_obj.samples.begin(), prf_obj.samples.end(), compareByTime);
		double tm_cpy_end = dclock();
		printf("prf_parse_text: samples copy tm= %f at %s %d\n", tm_cpy_end-tm_cpy_beg, __FILE__, __LINE__);
	}
	double tm_end = dclock();
	printf("prf_parse_text: tm_end - tm_beg = %f, tm from begin= %f\n", tm_end - tm_beg, tm_end - tm_beg_in);
	printf("cmmnts= %d, samples= %d, callstack= %d, null= %d\n",
		lines_comments, lines_samples, lines_callstack, lines_null);
	for (uint32_t i=0; i < nms.size(); i++) {
		printf("event[%d]: %s, count= %d, extra_strings= %d\n",
			i, prf_obj.features_event_desc[i].event_string.c_str(), nms[i].count, nms[i].ext_strs);
	}
	return 0;
}

static int str_app(std::string &str, std::string nw_str, std::string str_or_chr)
{
	if (str.size() > 0 && str_or_chr.size() > 0) {
		str += str_or_chr + nw_str;
	} else {
		str += nw_str;
	}
	return 0;
}

static struct perf_header perf_hdr;

static int prf_decode_sample_type(std::string &str, uint64_t typ)
{
	int len = 0;
	if (typ & PERF_SAMPLE_IP) {
		str_app(str, "IP", "|");
	}
	if (typ & PERF_SAMPLE_TID) {
		str_app(str, "TID", "|");
		len++;
	}
	if (typ & PERF_SAMPLE_TIME) {
		str_app(str, "TIME", "|");
		len++;
	}
	if (typ & PERF_SAMPLE_ADDR) {
		str_app(str, "ADDR", "|");
	}
	if (typ & PERF_SAMPLE_READ) {
		str_app(str, "READ", "|");
	}
	if (typ & PERF_SAMPLE_CALLCHAIN) {
		str_app(str, "CALLCHAIN", "|");
	}
	if (typ & PERF_SAMPLE_ID) {
		str_app(str, "ID", "|");
		len++;
	}
	if (typ & PERF_SAMPLE_CPU) {
		str_app(str, "CPU", "|");
		len++;
	}
	if (typ & PERF_SAMPLE_PERIOD) {
		str_app(str, "PERIOD", "|");
	}
	if (typ & PERF_SAMPLE_STREAM_ID) {
		str_app(str, "STREAM_ID", "|");
		len++;
	}
	if (typ & PERF_SAMPLE_RAW) {
		str_app(str, "RAW", "|");
	}
	if (typ & PERF_SAMPLE_BRANCH_STACK) {
		str_app(str, "BR_STK", "|");
	}
	if (typ & PERF_SAMPLE_REGS_USER) {
		str_app(str, "REGS_USER", "|");
	}
	if (typ & PERF_SAMPLE_STACK_USER) {
		str_app(str, "STACK_USER", "|");
	}
	if (typ & PERF_SAMPLE_WEIGHT) {
		str_app(str, "SAMPLE_WEIGHT", "|");
	}
	if (typ & PERF_SAMPLE_DATA_SRC) {
		str_app(str, "DATA_SRC", "|");
	}
	if (typ & PERF_SAMPLE_IDENTIFIER) {
		str_app(str, "IDENTIFIER", "|");
		len++;
	}
	if (typ & PERF_SAMPLE_TRANSACTION) {
		str_app(str, "TRANSACTION", "|");
	}
	if (typ & PERF_SAMPLE_REGS_INTR) {
		str_app(str, "REG_INTR", "|");
	}
#ifdef PERF_SAMPLE_PHYS_ADDR
	if (typ & PERF_SAMPLE_PHYS_ADDR) {
		str_app(str, "PHYS_ADDR", "|");
	}
#endif
	return len;
}

static int prf_decode_read_format_len(uint64_t fmt, int &grp, int &tm_ena, int &tm_run, int &id)
{
	int len = 0;
	if (fmt & PERF_FORMAT_GROUP) {
		grp = 1;
	} else {
		grp = 0;
	}
	tm_ena = 0;
	if (fmt & PERF_FORMAT_TOTAL_TIME_ENABLED) {
		len++;
		tm_ena = 1;
	}
	tm_run = 0;
	if (fmt & PERF_FORMAT_TOTAL_TIME_RUNNING) {
		len++;
		tm_run = 1;
	}
	id = 0;
	if (fmt & PERF_FORMAT_ID) {
		len++;
		id = 1;
	}
	return len;
}

static int prf_decode_read_format(std::string &str, uint64_t fmt)
{
	if (fmt & PERF_FORMAT_GROUP) {
		str_app(str, "GROUP", "|");
	}
	if (fmt & PERF_FORMAT_TOTAL_TIME_ENABLED) {
		str_app(str, "TIME_ENABLED", "|");
	}
	if (fmt & PERF_FORMAT_TOTAL_TIME_RUNNING) {
		str_app(str, "TIME_RUNNING", "|");
	}
	if (fmt & PERF_FORMAT_ID) {
		str_app(str, "ID", "|");
	}
	int grp, tm_ena, tm_run, id;
	return prf_decode_read_format_len(fmt, grp, tm_ena, tm_run, id);
}


static int prf_ck_for_id(const prf_obj_str &prf_obj)
{
	if (prf_obj.has_ids) {
		return 1;
	}
	return 0;
}

size_t prf_sample_to_string(int idx, std::string &ostr, prf_obj_str &prf_obj)
{
	static char *snbuf = NULL;
	static size_t sz = 0;
	size_t osz = snprintf(snbuf, sz, "%s %d/%d [%.3d]%s %" PRIu64 " %s:",
		prf_obj.samples[idx].comm.c_str(), prf_obj.samples[idx].pid, prf_obj.samples[idx].tid, prf_obj.samples[idx].cpu,
		prf_obj.samples[idx].tm_str.c_str(), prf_obj.samples[idx].period, prf_obj.samples[idx].event.c_str());
	if (osz >= sz) {
		sz = 2 * osz;
		if (snbuf != NULL) {
			free(snbuf);
		}
		if (sz < (4*1024)) {
			sz = 4 * 1024;
		}
		snbuf = (char *)malloc(sz);
		osz = snprintf(snbuf, sz, "%s %d/%d [%.3d]%s %" PRIu64 " %s:",
			prf_obj.samples[idx].comm.c_str(), prf_obj.samples[idx].pid, prf_obj.samples[idx].tid, prf_obj.samples[idx].cpu,
			prf_obj.samples[idx].tm_str.c_str(), prf_obj.samples[idx].period, prf_obj.samples[idx].event.c_str());
		if (osz >= sz) {
			printf("ummm... how is this possible? osz = %d, sz = %d at %s %d\n", (int)osz, (int)sz, __FILE__, __LINE__);
			exit(1);
		}
	}
	ostr = std::string(snbuf);
	return ostr.size();
}

void prf_add_comm(uint32_t pid, uint32_t tid, std::string comm, prf_obj_str &prf_obj, double tm)
{
	struct prf_comm_str pcs;
	if (prf_obj.comm.size() == 0 && tid != 0 && prf_obj.file_type != FILE_TYP_ETW) {
		// prf 4.10 doesn't seem to put swapper, 0, 0 in the COMM list
		pcs.pid = 0;
		pcs.tid = 0;
		pcs.comm = std::string("swapper");
		prf_obj.comm.push_back(pcs);
		prf_obj.tid_2_comm_indxp1[pcs.tid] = prf_obj.comm.size();
	}
	pcs.pid = pid;
	pcs.tid = tid;
	pcs.comm = comm;
	prf_obj.comm.push_back(pcs);
	prf_obj.tid_2_comm_indxp1[pcs.tid] = prf_obj.comm.size();
}

void prf_add_ids(uint32_t id, int evt_idx, prf_obj_str &prf_obj)
{
	hash_uint32(prf_obj.ids_2_evt_indxp1, prf_obj.ids_vec, id, evt_idx);
}

struct tmr_str {
	double b, e, cumu;
	int b_line, e_line;
	tmr_str(): b(0.0), e(0.0), cumu(0.0), b_line(0), e_line(0) {}
};
static std::vector <tmr_str> dec_tm;

static double tm_set(int i, int line)
{
	if ((int)dec_tm.size()  <= i) {
		dec_tm.resize(i+1);
	}
	double elap = dclock();
	dec_tm[i].b = elap;
	dec_tm[i].b_line = line;
	return elap;
}

static double tm_accum(int &i, int line)
{
	if ((int)dec_tm.size()  <= (i+1)) {
		dec_tm.resize(i+2);
	}
	double elap = dclock();
	dec_tm[i].e = elap;
	dec_tm[i].cumu += dec_tm[i].e - dec_tm[i].b;
	dec_tm[i].e_line = line;
	dec_tm[++i].b = elap;
	dec_tm[i].b_line = line;
	return elap;
}

static bool compareByCumu(const tmr_str &a, const tmr_str &b)
{
	return a.cumu > b.cumu;
}
static void tm_print(void)
{
	std::vector <tmr_str> dec_srt;
	for (uint32_t i=0; i < dec_tm.size(); i++) {
		dec_srt.push_back(dec_tm[i]);
	}
	std::sort(dec_srt.begin(), dec_srt.end(), compareByCumu);
	for (uint32_t i=0; i < dec_srt.size(); i++) {
		fprintf(stderr, "time: prf_decode[%d] cumu= %f, line b= %d e= %d\n", i, dec_srt[i].cumu, dec_srt[i].b_line, dec_srt[i].e_line);
	}
	
}

static int prf_decode_perf_record(const long pos_rec, uint64_t typ, char *rec, int disp,
		prf_obj_str &prf_obj, double tm_beg_in, file_list_str &file_list)
{
	int verbose = 0;
	static int orig_order= 0;

	switch(typ) {
		case PERF_RECORD_SAMPLE:
		{
			int tmi;
			uint64_t id2 = (uint64_t)-1;
			long off=0;
			std::string evt_nm;
			static double tm_cumu = 0;
			int whch_evt = 0; // if no IDs then only 1 event?
			double tm_beg0;
			tmi=0;
			tm_beg0 = tm_set(tmi, __LINE__);
			int exp_id = prf_ck_for_id(prf_obj);
			if (!exp_id && prf_obj.events.size() != 1) {
				printf("ummm... got no IDs in the prf event list but event list size is(%" PRIu64 ") != 1. ck yer logic dude. Bye at %s %d\n",
					prf_obj.events.size(), __FILE__, __LINE__);
				exit(1);
			}
			tm_accum(tmi, __LINE__);
		 //	{ u64			id;	  } && PERF_SAMPLE_IDENTIFIER
			if (exp_id) {
				whch_evt = -1;
				id2 = *(buf_uint64_ptr(buf, 0));
				off += sizeof(id2);
				for (uint32_t i=0; i < prf_obj.events.size(); i++) {
					for (uint32_t j=0; j < prf_obj.events[i].ids.size(); j++) {
						if (prf_obj.events[i].ids[j] == id2) {
							whch_evt = (int)i;
							//printf("got match on event[%d].id= %ld at %s %d\n", i, id2, __FILE__, __LINE__);
							evt_nm = prf_obj.events[whch_evt].event_name;
							break;
						}	
					}
					if (whch_evt != -1) {
						break;
					}
				}
				if (whch_evt == -1) {
					printf("missed id= %" PRIu64 ", ck yer code dude! Bye at %s %d\n", id2, __FILE__, __LINE__);
					exit(1);
				}
				for (uint32_t i=0; i < prf_obj.features_event_desc.size(); i++) {
					for (uint32_t j=0; j < prf_obj.features_event_desc[i].ids.size(); j++) {
						if (prf_obj.features_event_desc[i].ids[j] == id2) {
							evt_nm = prf_obj.features_event_desc[i].event_string;
							if (whch_evt != (int)i) {
								printf("mess up here. Expected event number match. Got whch_evt(%d) != i(%d) at %s %d\n",
									whch_evt, i, __FILE__, __LINE__);
								exit(1);
							}
							break;
						}
					}
				}
			}
			tm_accum(tmi, __LINE__);
		 //	{ u64			ip;	  } && PERF_SAMPLE_IP
			if (prf_obj.events[whch_evt].pea.sample_type & PERF_SAMPLE_IP) {
				uint64_t ip = *(buf_uint64_ptr(buf, off));
				off += sizeof(ip);
			}
			u32 pid=(u32)-1;
			u32 tid=pid, cpu=pid, res=pid;
			u64 time=0, addr, id, stream_id, period=0;
		 //	{ u32			pid, tid; } && PERF_SAMPLE_TID
			if (prf_obj.events[whch_evt].pea.sample_type & PERF_SAMPLE_TID) {
				pid = *(u32 *)(buf + off);
				off += sizeof(pid);
				tid = *(u32 *)(buf + off);
				off += sizeof(tid);
			}
		 //	{ u64			time;     } && PERF_SAMPLE_TIME
			if (prf_obj.events[whch_evt].pea.sample_type & PERF_SAMPLE_TIME) {
				time = *(u64 *)(buf + off);
				off += sizeof(time);
			}
		 //	{ u64			addr;     } && PERF_SAMPLE_ADDR
			if (prf_obj.events[whch_evt].pea.sample_type & PERF_SAMPLE_ADDR) {
				addr = *(u64 *)(buf + off);
				off += sizeof(addr);
			}
		 //	{ u64			id;	  } && PERF_SAMPLE_ID
			if (prf_obj.events[whch_evt].pea.sample_type & PERF_SAMPLE_ID) {
				id = *(u64 *)(buf + off);
				off += sizeof(id);
			}
		 //	{ u64			stream_id;} && PERF_SAMPLE_STREAM_ID
			if (prf_obj.events[whch_evt].pea.sample_type & PERF_SAMPLE_STREAM_ID) {
				stream_id = *(u64 *)(buf + off);
				off += sizeof(stream_id);
			}
		 //	{ u32			cpu, res; } && PERF_SAMPLE_CPU
			if (prf_obj.events[whch_evt].pea.sample_type & PERF_SAMPLE_CPU) {
				cpu = *(u32 *)(buf + off);
				off += sizeof(cpu);
				res = *(u32 *)(buf + off);
				off += sizeof(res);
			}
		 //	{ u64			period;   } && PERF_SAMPLE_PERIOD
			if (prf_obj.events[whch_evt].pea.sample_type & PERF_SAMPLE_PERIOD) {
				period = *(u64 *)(buf + off);
				off += sizeof(period);
			}
		 //	{ struct read_format	values;	  } && PERF_SAMPLE_READ
			if (prf_obj.events[whch_evt].pea.sample_type & PERF_SAMPLE_READ) {
				//sample_rec_fmt_len = prf_decode_read_format(decoded_sample_fmt, (uint64_t)pfa->attr.read_format);
				uint64_t nr=0;
				int grp=0, tm_ena=0, tm_run=0, id3=0;
				int len = prf_decode_read_format_len((uint64_t)prf_obj.events[whch_evt].pea.read_format, grp,
						tm_ena, tm_run, id3);
				if (!grp) {
					len++;
					len *= sizeof(u64);
					printf("PERF_SAMPLE_READ !grp sz= %d , nr= %d grp= %d, tm_ena= %d, tm_run= %d, id= %d, at %s %d\n",
						(int)len, (int)nr, grp, tm_ena, tm_run, id3, __FILE__, __LINE__);
				} else {
					nr = *(u64 *)(buf + off);
					len = (int)(sizeof(u64) * ((1 + tm_ena + tm_run) + nr * (1 + id3)));
					printf("PERF_SAMPLE_READ grp sz= %d , nr= %d grp= %d, tm_ena= %d, tm_run= %d, id= %d, at %s %d\n",
						(int)len, (int)nr, grp, tm_ena, tm_run, id3, __FILE__, __LINE__);
				}
				off += len;
			}
			if (prf_obj.events[whch_evt].pea.sample_type & PERF_SAMPLE_CALLCHAIN) {
				u64 nr = *(u64 *)(buf + off);
				off += (u64)sizeof(nr);
				off += nr*(u64)sizeof(u64);
				//u64 nxt = *(u64 *)(buf + off);
				//printf("callchain nr= %d, off= %d 0x%x, nxt= 0x%lx\n", (int)nr, (int)off, (int)off, nxt);
			}
			long raw_offset = -1;
			if (prf_obj.events[whch_evt].pea.sample_type & PERF_SAMPLE_RAW) {
				u32 size = *(u32 *)(buf + off);
				off += sizeof(u32);
				raw_offset = off;
				if (verbose)
					hex_dump_n_bytes_from_buf(buf+off, size, "evt raw_data:", __LINE__);
				off += size;
			}
			uint64_t other_mask = 
				PERF_SAMPLE_BRANCH_STACK| PERF_SAMPLE_REGS_USER| PERF_SAMPLE_STACK_USER| PERF_SAMPLE_WEIGHT| 
#ifdef PERF_SAMPLE_PHYS_ADDR
					PERF_SAMPLE_PHYS_ADDR |
#endif
				PERF_SAMPLE_DATA_SRC| PERF_SAMPLE_TRANSACTION| PERF_SAMPLE_REGS_INTR;
			if (prf_obj.events[whch_evt].pea.sample_type & other_mask) {
				printf("sorry, some bits are set in the PERF_SAMPLE_* mask which are not yet handled. Bye at %s %d\n", __FILE__, __LINE__);
				if (prf_obj.events[whch_evt].pea.sample_type & PERF_SAMPLE_BRANCH_STACK) {
					printf("PERF_SAMPLE_BRANCH_STACK is set and unhandled\n");
				}
				if (prf_obj.events[whch_evt].pea.sample_type & PERF_SAMPLE_REGS_USER) {
					printf("PERF_SAMPLE_REGS_USER is set and unhandled\n");
				}
				if (prf_obj.events[whch_evt].pea.sample_type & PERF_SAMPLE_STACK_USER) {
					printf("PERF_SAMPLE_STACK_USER is set and unhandled\n");
				}
				if (prf_obj.events[whch_evt].pea.sample_type & PERF_SAMPLE_WEIGHT) {
					printf("PERF_SAMPLE_WEIGHT is set and unhandled\n");
				}
				if (prf_obj.events[whch_evt].pea.sample_type & PERF_SAMPLE_DATA_SRC) {
					printf("PERF_SAMPLE_DATA_SRC is set and unhandled\n");
				}
				if (prf_obj.events[whch_evt].pea.sample_type & PERF_SAMPLE_TRANSACTION) {
					printf("PERF_SAMPLE_TRANSACTION is set and unhandled\n");
				}
				if (prf_obj.events[whch_evt].pea.sample_type & PERF_SAMPLE_REGS_INTR) {
					printf("PERF_SAMPLE_REGS_INTR is set and unhandled\n");
				}
#ifdef PERF_SAMPLE_PHYS_ADDR
				if (prf_obj.events[whch_evt].pea.sample_type & PERF_SAMPLE_PHYS_ADDR) {
					printf("PERF_SAMPLE_PHYS_ADDR is set and unhandled\n");
				}
#endif
				exit(1);
			}
			if (prf_obj.events[whch_evt].pea.type == PERF_TYPE_TRACEPOINT &&
				prf_obj.events[whch_evt].lst_ft_fmt_idx == -2) {
				prf_obj.events[whch_evt].lst_ft_fmt_idx = -1;
				for (uint32_t i=0; i < file_list.lst_ft_fmt_vec.size(); i++) {
					if ((file_list.lst_ft_fmt_vec[i].event == evt_nm &&
							file_list.lst_ft_fmt_vec[i].area == prf_obj.events[whch_evt].event_area) ||
							(file_list.lst_ft_fmt_vec[i].area+":"+file_list.lst_ft_fmt_vec[i].event) == evt_nm) {
						printf("got lst_ft_fmt: match on prf event= %s at %s %d\n", evt_nm.c_str(), __FILE__, __LINE__);
						prf_obj.events[whch_evt].lst_ft_fmt_idx = (int)i;
						break;
					}
				}
				if (prf_obj.events[whch_evt].lst_ft_fmt_idx == -1) {
					printf("missed lst_ft_fmt: for perf event= %s at %s %d\n", evt_nm.c_str(), __FILE__, __LINE__);
					exit(1);
				}
			}
			tm_accum(tmi, __LINE__);
			double tm = 1.e-9 * (double)time;
			if (orig_order == 0) {
				printf("1st timestamp= %" PRIu64 " at %s %d\n", time, __FILE__, __LINE__);
			}
			if (pid != (u32)-1 && tid != (u32)-1 && pid > 0 && tid == 0) {
				// why is this needed? There are lines like:
	// swapper  8451/0     [003] 15524.509388277:          1   sched:sched_switch:               prev_comm=spin.x prev_pid=8455 prev_prio=120 prev_state=x ==> next_comm=swapper/3 next_pid=0 next_prio=120
				// above we see a sched switch with comm='swapper', pid > 0, tid==0 but prev_comm/pid = spin.x 8455
				// This is about 0.1 secs before the thread is killed.d > 0, tid==0 but prev_comm/pid = spin.x 8455
				if (raw_offset != -1 && (evt_nm == "sched_switch" || evt_nm == "sched:sched_switch")) {
					printf("try using sched_switch data for missed pid= %d at %s %d\n", pid, __FILE__, __LINE__);
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
					s_sw = (sched_switch_str *)(buf + raw_offset);
					printf("prev_pid= %d, prev_comm= %s, next_pid= %d, next_comm= %s at %s %d\n",
						s_sw->prev_pid, s_sw->prev_comm, s_sw->next_pid, s_sw->next_comm, __FILE__, __LINE__);
					prf_add_comm(pid, s_sw->prev_pid, std::string(s_sw->prev_comm), prf_obj, tm);
					tid = (uint32_t)s_sw->prev_pid;
					//tid = sw_sw->prev_pid;
					int pid_indx = prf_obj.tid_2_comm_indxp1[s_sw->prev_pid];
					if (pid_indx == 0) {
						printf("still missed loookup of comm for pid= %d, tid= %d, evt_nm= %s at %s %d\n",
							s_sw->prev_pid, s_sw->prev_pid, evt_nm.c_str(), __FILE__, __LINE__);
						exit(1);
					}
				}
			}
			int indx = prf_obj.tid_2_comm_indxp1[tid];
			if (indx == 0) {
				indx = prf_obj.tid_2_comm_indxp1[pid];
				if (indx == 0) {
					printf("missed loookup of comm for pid= %d and tid= %d evt_nm= %s tm= %.9f at %s %d\n",
						pid, tid, evt_nm.c_str(), tm, __FILE__, __LINE__);
					//exit(1);
				}
				if (raw_offset != -1 && (evt_nm == "sched_switch" || evt_nm == "sched:sched_switch")) {
					printf("try using sched_switch data for missed pid= %d at %s %d\n", pid, __FILE__, __LINE__);
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
					s_sw = (sched_switch_str *)(buf + raw_offset);
					printf("prev_pid= %d, prev_comm= %s, next_pid= %d, next_comm= %s at %s %d\n",
						s_sw->prev_pid, s_sw->prev_comm, s_sw->next_pid, s_sw->next_comm, __FILE__, __LINE__);
					prf_add_comm(pid, s_sw->prev_pid, std::string(s_sw->prev_comm), prf_obj, tm);
					tid = (uint32_t)s_sw->prev_pid;
					int pid_indx = prf_obj.tid_2_comm_indxp1[s_sw->prev_pid];
					if (pid_indx == 0) {
						printf("still missed loookup of comm for pid= %d, tid= %d, evt_nm= %s at %s %d\n",
							s_sw->prev_pid, s_sw->prev_pid, evt_nm.c_str(), __FILE__, __LINE__);
						exit(1);
					}
				}
				indx = prf_obj.tid_2_comm_indxp1[pid];
				if (indx > 0) {
					prf_add_comm(pid, tid, prf_obj.comm[indx-1].comm, prf_obj, tm);
					indx = prf_obj.tid_2_comm_indxp1[tid];
					if (indx == 0) {
						printf("missed loookup2 of comm for pid= %d and tid= %d evt_nm= %s tm= %.9f at %s %d\n",
							pid, tid, evt_nm.c_str(), tm, __FILE__, __LINE__);
						exit(1);
					}
				}
			}
			tm_accum(tmi, __LINE__);
			static int smples = -1;
			prf_samples_str pss;
			if (indx > 0) {
				pss.comm   = prf_obj.comm[indx-1].comm;
			}
			pss.event  = evt_nm;
			pss.pid    = pid;
			pss.evt_idx = (uint32_t)whch_evt;
			pss.tid    = tid;
			pss.cpu    = cpu;
			pss.ts     = time;
			pss.orig_order = orig_order++;
			//printf("ts= %f at %s %d\n", tm, __FILE__, __LINE__);
			if (options.tm_clip_beg_valid == 1 && tm < options.tm_clip_beg) {
				break;
			}
			if (options.tm_clip_end_valid == 1 && tm > options.tm_clip_end) {
				break;
			}
			pss.period = period;
			if (raw_offset != -1) {
				pss.mm_off = pos_rec + raw_offset;
			}
			char num_buf[256];
			int slen = snprintf(num_buf, sizeof(num_buf), " %.9f: ", tm);
			if (slen == sizeof(num_buf)) {
				printf("overflow for tm= %.9f at %s %d\n", tm, __FILE__, __LINE__);
				exit(1);
			}
			pss.tm_str = std::string(num_buf);
			if (verbose > 0) 
				printf("hdr:[%d] %-16.16s %d/%d [%.3d]%s %" PRIu64 " %s:\n",
					++smples, (indx > 0 ?  prf_obj.comm[indx-1].comm.c_str() : ""), pid, tid, cpu, pss.tm_str.c_str(), period, evt_nm.c_str());
			prf_obj.samples.push_back(pss);
			double tm_beg1 = tm_accum(tmi, __LINE__);
			tm_cumu += tm_beg1 - tm_beg0;
			std::string ostr;
			prf_sample_to_string((int)(prf_obj.samples.size()-1), ostr, prf_obj);
			++smples;
			if (verbose > 0)
			{
				double tm_beg2 = dclock();
				printf("hdr:[%d] tm_cumu= %f, tm_elap= %f %s, off= %ld 0x%lx\n", smples, tm_cumu, tm_beg2 - tm_beg_in, ostr.c_str(), off, off);
			}
			tm_accum(tmi, __LINE__);

			break;
		}
		case PERF_RECORD_MMAP2:
		{
#pragma pack(push, 1)
			struct smpl_typ_mmap_str {
				u32			pid, tid;
				u64			addr;
				u64			len;
				u64			pgoff;
				u32			maj;
				u32			min;
				u64			ino;
				u64			ino_generation;
				u32			prot, flags;
				char		filename[];
				//struct sample_id	sample_id;
			} *smpl_typ_mmap;
#pragma pack(pop)
			smpl_typ_mmap = (struct smpl_typ_mmap_str *)rec;
			if (verbose > 0)
			printf("smpl_typ_mmap2: pid= %d, tid= %d, addr= 0x%" PRIx64 ", len= 0x%" PRIx64 ", pgoff= 0x%" PRIx64 ", maj= %d, min= %d, ino= %" PRId64 ", ino_gen= %" PRIu64 ", prot= %d, flags= 0x%x flnm= %s\n",
				smpl_typ_mmap->pid,
				smpl_typ_mmap->tid,
				smpl_typ_mmap->addr,
				smpl_typ_mmap->len,
				smpl_typ_mmap->pgoff,
				smpl_typ_mmap->maj,
				smpl_typ_mmap->min,
				smpl_typ_mmap->ino,
				smpl_typ_mmap->ino_generation,
				smpl_typ_mmap->prot,
				smpl_typ_mmap->flags,
				smpl_typ_mmap->filename);
			break;
		}
		case PERF_RECORD_MMAP:
		{
#pragma pack(push, 1)
			struct smpl_typ_mmap_str {
				u32			pid, tid;
				u64			addr;
				u64			len;
				u64			pgoff;
				char			filename[];
				//struct sample_id	sample_id;
			} *smpl_typ_mmap;
#pragma pack(pop)
			smpl_typ_mmap = (struct smpl_typ_mmap_str *)rec;
			if (verbose > 0)
			printf("smpl_typ_mmap: pid= %d, tid= %d, addr= 0x%" PRIx64 ", len= 0x%" PRIx64 ", pgoff= 0x%" PRIx64 ", flnm= %s\n",
				smpl_typ_mmap->pid,
				smpl_typ_mmap->tid,
				smpl_typ_mmap->addr,
				smpl_typ_mmap->len,
				smpl_typ_mmap->pgoff,
				smpl_typ_mmap->filename);
			break;
		}
		case PERF_RECORD_LOST:
		{
#pragma pack(push, 1)
			struct smpl_typ_lost_str {
				u64	id;
				u64	lost;
				//struct sample_id		sample_id;
			} *smpl_typ_lost;
#pragma pack(pop)
			smpl_typ_lost = (struct smpl_typ_lost_str *)rec;
			if (verbose > 0)
			printf("smpl_typ_log: id= %" PRIu64 ", lost= %" PRIu64 "\n",
				smpl_typ_lost->id, smpl_typ_lost->lost);
			break;
		}
		case PERF_RECORD_COMM:
		{
#pragma pack(push, 1)
			struct a {
				u32	pid, tid;
				char	comm[];
			} *smpl;
#pragma pack(pop)
			smpl = (struct a *)rec;
			prf_add_comm(smpl->pid, smpl->tid, std::string(smpl->comm), prf_obj, 0.0);
			if (verbose > 0)
			printf("smpl_typ_comm: pid= %d, tid= %d, comm= %s\n",
				smpl->pid, smpl->tid, smpl->comm);
			break;
		}
		case PERF_RECORD_EXIT:
		{
#pragma pack(push, 1)
			struct a {
				u32 pid, ppid, tid, ptid;
				u64 time;
			} *smpl;
#pragma pack(pop)
			smpl = (struct a *)rec;
			if (verbose > 0)
			printf("smpl_typ_exit: pid= %d, ppid= %d, tid= %d, ptid= %d, time= %" PRIu64 "\n",
				smpl->pid, smpl->ppid, smpl->tid,smpl->ptid, smpl->time);
			break;
		}
		case PERF_RECORD_THROTTLE:
		case PERF_RECORD_UNTHROTTLE:
		{
#pragma pack(push, 1)
			struct a {
				u64 time;
				u64 id;
				u64 stream_id;
			} *smpl;
#pragma pack(pop)
			smpl = (struct a *)rec;
			if (verbose > 0)
			printf("smpl_typ_%sthrottle: time= %" PRIu64 ", id= %" PRIu64 ", stream_id= %" PRIu64 "\n",
				(typ == PERF_RECORD_THROTTLE ? "" : "un"),
				smpl->time, smpl->id, smpl->stream_id);
			break;
		}
		case PERF_RECORD_FORK:
		{
#pragma pack(push, 1)
			struct a {
				u32 pid, ppid, tid, ptid;
				u64 time;
			} *smpl;
#pragma pack(pop)
			smpl = (struct a *)rec;
			if (verbose > 0)
			printf("smpl_typ_fork: pid= %d, ppid= %d, tid= %d, ptid= %d, time= %" PRIu64 "\n",
				smpl->pid, smpl->ppid, smpl->tid,smpl->ptid, smpl->time);
#if 0
			int indx = prf_obj.tid_2_comm_indxp1[smpl->tid];
			if (indx == 0) {
				indx = prf_obj.tid_2_comm_indxp1[tid];
				prf_add_comm(smpl->pid, smpl->tid, std::string(smpl->comm), prf_obj);
				printf("missed loookup of comm for pid= %d, tid= %d evt_nm= %s tm= %.9f at %s %d\n",
					pid, tid, evt_nm.c_str(), tm, __FILE__, __LINE__);
				exit(1);
			}
			pss.comm   = prf_obj.comm[indx-1].comm;
#endif
			break;
		}
		case PERF_RECORD_READ:
		{
#pragma pack(push, 1)
			struct a {
				u32 pid, tid;
				// TBD handle struct read_format
				//struct read_format		values;
			} *smpl;
#pragma pack(pop)
			smpl = (struct a *)rec;
			if (verbose > 0)
			printf("smpl_typ_read: pid= %d, tid= %d\n",
				smpl->pid, smpl->tid);
			break;
		}
		default:
		{
			printf("smpl_typ_unhandled: typ= %" PRId64 " at %s %d\n", typ, __FILE__, __LINE__);
		}
	}
	return 0;
}

static std::string prf_get_evt_name(uint64_t typ, uint64_t config, file_list_str &file_list)
{
	std::string nm;
	if (typ == PERF_TYPE_HARDWARE) {
		switch(config) {
		case PERF_COUNT_HW_CPU_CYCLES:
			return "cycles";
			break;
		case PERF_COUNT_HW_INSTRUCTIONS:
			return "instructions";
			break;
		case PERF_COUNT_HW_CACHE_REFERENCES:
			return "cache-references";
			break;
		case PERF_COUNT_HW_CACHE_MISSES:
			return "cache-misses";
			break;
		case PERF_COUNT_HW_BRANCH_INSTRUCTIONS:
			return "branches";
			break;
		case PERF_COUNT_HW_BRANCH_MISSES:
			return "branch-misses";
			break;
		case PERF_COUNT_HW_BUS_CYCLES:
			return "bus-cycles";
			break;
		case PERF_COUNT_HW_STALLED_CYCLES_FRONTEND:
			return "frontend-stalls";
			break;
		case PERF_COUNT_HW_STALLED_CYCLES_BACKEND:
			return "backend-stalls";
			break;
		case PERF_COUNT_HW_REF_CPU_CYCLES:
			return "ref-cycles";
			break;
		default:
			break;
		}
	} else if (typ == PERF_TYPE_SOFTWARE) {
		switch(config) {
		case PERF_COUNT_SW_CPU_CLOCK:
			return "cpu-clock";
			break;
		case PERF_COUNT_SW_TASK_CLOCK:
			return "task-clock";
			break;
		case PERF_COUNT_SW_PAGE_FAULTS:
			return "page-faults";
			break;
		case PERF_COUNT_SW_CONTEXT_SWITCHES:
			return "context-switches";
			break;
		case PERF_COUNT_SW_CPU_MIGRATIONS:
			return "cpu-migrations";
			break;
		case PERF_COUNT_SW_PAGE_FAULTS_MIN:
			return "minor-faults";
			break;
		case PERF_COUNT_SW_PAGE_FAULTS_MAJ:
			return "major-faults";
			break;
		case PERF_COUNT_SW_ALIGNMENT_FAULTS:
			return "alignment-faults";
			break;
		case PERF_COUNT_SW_EMULATION_FAULTS:
			return "emulation-faults";
			break;
		case PERF_COUNT_SW_DUMMY:
			return "dummy";
			break;
		default:
			break;
		}
	} else if (typ == PERF_TYPE_TRACEPOINT) {
		int indx = file_list.tp_id_2_event_indxp1[(int)config];
		if (indx == 0) {
			printf("missed trace event lookup for event id (config)= %" PRIu64 ", 0x%" PRIx64 " sz= %d idx= %d at %s %d\n",
					config, config, (int)file_list.tp_id_2_event_indxp1.size(),
					file_list.idx, __FILE__, __LINE__);
			exit(1);
		}
		indx--;
		return file_list.tp_events[indx].area + ":" + file_list.tp_events[indx].event;
	}
	return nm;
}

static std::string prf_prt_str(char *pfx, char *sbuf, int &offset)
{
	struct perf_header_string *pevt_hdr;
	pevt_hdr = (struct perf_header_string *)sbuf;
	int sz = (int)pevt_hdr->len;
	int str_off = sizeof(pevt_hdr->len);
	char *cp = sbuf+str_off;
	printf("%s sz= %d str= %s\n", pfx, pevt_hdr->len, cp);
	offset = sz + str_off;
	return std::string(cp);
}

static std::vector <std::string> prf_prt_str_lst(char *pfx, char *sbuf, int &offset)
{
	struct perf_header_string_list *pevt_hdr;
	pevt_hdr = (struct perf_header_string_list *)sbuf;
	int nr = (int)pevt_hdr->nr;
	int str_off = sizeof(pevt_hdr->nr);
	std::string str;
	std::vector <std::string> svec;
	for (int i=0; i < nr; i++) {
		char *cp = sbuf+str_off;
		int off2=0;
		std::string argv = prf_prt_str(pfx, cp, off2);
		str += (str.size() == 0 ? "" : " ") + argv;
		svec.push_back(argv);
		str_off += off2;
	}
	
	printf("%s str= %s\n", pfx, str.c_str());
	offset = str_off;
	return svec;
}


static int prf_prt_event_desc(char *pfx, char *sbuf, prf_obj_str &prf_obj)
{
	struct evt_desc_str { 
	       uint32_t nr; /* number of events */
	       uint32_t attr_size; /* size of each perf_event_attr */
	       //} events[nr]; /* Variable length records */
	       struct events_str { 
		      struct perf_event_attr attr;  /* size of attr_size */
		      uint32_t nr_ids;
		      struct perf_header_string event_string;
		      uint64_t *ids;
		      //uint64_t ids[nr_ids];
	       } events, *pevents; /* Variable length records */
	} *pevt_hdr;
#pragma pack(push, 1)
   struct events_str { 
	  struct perf_event_attr attr;  /* size of attr_size */
	  uint32_t nr_ids;
	  struct perf_header_string event_string;
	  uint64_t *ids;
   };
#pragma pack(pop)
	pevt_hdr = (struct evt_desc_str *)sbuf;
	int nr = (int)pevt_hdr->nr;
	int at_sz = (int)pevt_hdr->attr_size;
	int at_off = offsetof(struct evt_desc_str, events.attr);
	printf("event_desc nr= %d, at_sz= %d, sizoef(attr)= %d, at_off= %d, nr_ids[0]= %d\n",
		nr, at_sz, (int)sizeof(struct perf_event_attr), at_off, pevt_hdr->events.nr_ids);
	int str_off = at_off; 
//xyz
	for (int j=0; j < nr; j++) {
		char *cp = sbuf + str_off;
		struct events_str *pevents = (struct events_str *)cp;
		printf("%s event_desc[%d]: nr_ids= %u, sln= %d\n", pfx, j, pevents[0].nr_ids, pevents[0].event_string.len);
		int off2=0;
		std::string e_str = prf_prt_str(pfx, (char *)(&pevents[0].event_string), off2);
		int slen = at_sz + (int)sizeof(pevents[0].nr_ids) + off2;
		struct prf_event_desc_str eds;
		eds.nr_ids = pevents[0].nr_ids;
		eds.event_string = e_str;
		pevents[0].ids = (uint64_t *)(char *)(cp + slen);
		for (uint32_t k=0; k < eds.nr_ids; k++) {
			eds.ids.push_back(pevents[0].ids[k]);
		}
		eds.event_string = e_str;
		eds.attr = pevents[0].attr;
		prf_obj.features_event_desc.push_back(eds);
		slen += pevents[0].nr_ids*sizeof(uint64_t);
		str_off += slen;
	}
	return 0;
}

static int prf_prt_nrcpus(char *pfx, char *sbuf, prf_obj_str &prf_obj)
{
#pragma pack(push, 1)
	struct nr_cpus {
	       uint32_t nr_cpus_online;
	       uint32_t nr_cpus_available; /* CPUs not yet onlined */
	} *pevt_hdr;
#pragma pack(pop)
	pevt_hdr = (struct nr_cpus *)sbuf;
	prf_obj.features_nr_cpus_online    = (int)pevt_hdr->nr_cpus_online;
	prf_obj.features_nr_cpus_available = (int)pevt_hdr->nr_cpus_available;
	printf("%s cpus_online= %d, cpus_avail=%d\n", pfx, pevt_hdr->nr_cpus_online, pevt_hdr->nr_cpus_available);
	return 0;
}

static int prf_prt_total_mem(char *pfx, char *sbuf)
{
	uint64_t tot_mem;
	tot_mem = *(uint64_t *)sbuf;
	printf("%s total_mem= %" PRIu64 "\n", pfx, tot_mem);
	return 0;
}

static int prf_prt_numa_topology(char *pfx, char *sbuf, prf_obj_str &prf_obj)
{
#pragma pack(push, 1)
	struct numa_topo_hdr_str {
	       uint32_t nr;
	} *pevt_hdr;
#pragma pack(pop)
#pragma pack(push, 1)
   struct numa_topo_str {
	  uint32_t nodenr;
	  uint64_t mem_total;
	  uint64_t mem_free;
	  struct perf_header_string cpus;
   }; /* Variable length records */
#pragma pack(pop)
	pevt_hdr = (struct numa_topo_hdr_str *)sbuf;
	int nr = (int)pevt_hdr->nr;
	int str_off = sizeof(pevt_hdr->nr);
	printf("%s numa nodes= %d\n", pfx, nr);
	for (int i=0; i < nr; i++) {
		struct numa_str ns;
		struct numa_topo_str *nodes = (struct numa_topo_str *)(sbuf+str_off);
		int slen = sizeof(nodes->nodenr) + sizeof(nodes->mem_total) + sizeof(nodes->mem_free);
		char *cp = sbuf + str_off + slen;
		int off=0;
		std::string cpus = prf_prt_str(pfx, cp, off);
		printf("nodenr= %d, mem_tot= %" PRIu64 ", mem_free= %" PRIu64 ", cpus= %s\n",
			nodes->nodenr, nodes->mem_total, nodes->mem_free, cpus.c_str());
		ns.nodenr = nodes->nodenr;
		ns.mem_total = nodes->mem_total;
		ns.mem_free = nodes->mem_free;
		ns.cpus = cpus;
		prf_obj.features_numa_topology.push_back(ns);
		str_off += slen + off;
	}
	return 0;
}


static int prf_prt_pmu_mappings(char *pfx, char *sbuf, prf_obj_str &prf_obj)
{
#pragma pack(push, 1)
	struct pmu_hdr_str{
	       uint32_t nr;
	} *pevt_hdr;
#pragma pack(pop)
#pragma pack(push, 1)
	struct pmu_rstr {
		uint32_t pmu_type;
		struct perf_header_string pmu_name;
	}; /* Variable length records */
#pragma pack(pop)
	pevt_hdr = (struct pmu_hdr_str *)sbuf;
	int nr = (int)pevt_hdr->nr;
	int str_off = sizeof(pevt_hdr->nr);
	printf("%s pmus= %d\n", pfx, nr);
	for (int i=0; i < nr; i++) {
		struct pmu_str ps;
		struct pmu_rstr *pmu = (struct pmu_rstr *)(sbuf+str_off);
		int slen = sizeof(pmu->pmu_type);
		char *cp = sbuf + str_off + slen;
		int off=0;
		std::string pmu_name = prf_prt_str(pfx, cp, off);
		printf("pmu_type= %d, name= %s\n",
			pmu->pmu_type, pmu_name.c_str());
		ps.pmu_type = pmu->pmu_type;
		ps.pmu_name = pmu_name;
		prf_obj.features_pmu_mappings.push_back(ps);
		str_off += slen + off;
	}
	return 0;
}

static int prf_prt_group_desc(char *pfx, char *sbuf, prf_obj_str &prf_obj)
{
#pragma pack(push, 1)
	struct pmu_hdr_str{
	       uint32_t nr;
	} *pevt_hdr;
#pragma pack(pop)
#pragma pack(push, 1)
	struct grp_rstr {
		struct perf_header_string string;
		uint32_t leader_idx;
		uint32_t nr_members;
	}; /* Variable length records */
#pragma pack(pop)
	pevt_hdr = (struct pmu_hdr_str *)sbuf;
	int nr = (int)pevt_hdr->nr;
	int str_off = sizeof(pevt_hdr->nr);
	printf("%s event_grps= %d\n", pfx, nr);
	for (int i=0; i < nr; i++) {
		struct group_str gs;
		struct grp_rstr *grp = (struct grp_rstr *)(sbuf+str_off);
		char *cp = sbuf + str_off;
		int off = 0;
		std::string str = prf_prt_str(pfx, cp, off);
		cp = sbuf + str_off + off;
		gs.leader = str;
		gs.leader_idx = *(uint32_t *)cp;
		int slen = sizeof(grp->leader_idx);
		cp = sbuf + str_off + off + slen;
		gs.nr_members = *(uint32_t *)cp;
		printf("event_grps leader= %s, leader_indx= %d, nr_members= %d\n",
			gs.leader.c_str(), gs.leader_idx, gs.nr_members);
		prf_obj.features_group_desc.push_back(gs);
		str_off += slen + off + sizeof(gs.nr_members);
	}
	return 0;
}

static int prf_prt_cache(char *pfx, char *sbuf, prf_obj_str &prf_obj)
{
#pragma pack(push, 1)
	struct pmu_hdr_str{
	       uint32_t version;
	       uint32_t number_of_cache_levels;
	} *pevt_hdr;
#pragma pack(pop)
#pragma pack(push, 1)
	struct cache_rstr {
		uint32_t level, line_size, sets, ways;
		struct perf_header_string type, size, map;
	};
#pragma pack(pop)
	pevt_hdr = (struct pmu_hdr_str *)sbuf;
	int nr = (int)pevt_hdr->number_of_cache_levels;
	int str_off = sizeof(pevt_hdr->number_of_cache_levels) + sizeof(pevt_hdr->version);
	printf("%s cache_lvls= %d\n", pfx, nr);
	for (int i=0; i < nr; i++) {
		struct cache_str cs;
		struct cache_rstr *pcache = (struct cache_rstr *)(sbuf+str_off);
		cs.level = pcache->level;
		cs.line_size = pcache->line_size;
		cs.sets = pcache->sets;
		cs.ways = pcache->ways;
		str_off += 4 * sizeof(pcache->level);
		char *cp = sbuf + str_off;
		int off = 0;
		cs.type = prf_prt_str("cache_typ: ", cp, off);
		str_off += off;
		cp = sbuf + str_off;
		cs.size = prf_prt_str("cache_size: ", cp, off);
		str_off += off;
		cp = sbuf + str_off;
		cs.map  = prf_prt_str("cache_map: ", cp, off);
		str_off += off;
		cp = sbuf + str_off;
		printf("cacle[%d] lvl= %d, line_size= %d, sets= %d, ways= %d, type= %s, size= %s, map= %s\n",
			(int)prf_obj.features_cache.size(), cs.level, cs.line_size, cs.sets, cs.ways, cs.type.c_str(), cs.size.c_str(), cs.map.c_str());
		prf_obj.features_cache.push_back(cs);
	}
	return 0;
}

static int prf_prt_sample_time(char *pfx, char *sbuf, prf_obj_str &prf_obj)
{
	uint64_t tm_beg, tm_end;
	uint64_t *pevt_hdr = (uint64_t *)sbuf;
	tm_beg = pevt_hdr[0];
	tm_end = pevt_hdr[1];
	prf_obj.features_sample_time.push_back(tm_beg);
	prf_obj.features_sample_time.push_back(tm_end);
	printf("%s tm_beg= %" PRIu64 ", tm_end= %" PRIu64 "\n", pfx, tm_beg, tm_end);
	return 0;
}

int prf_read_data_bin(std::string flnm, int verbose, prf_obj_str &prf_obj, double tm_beg, file_list_str &file_list)
{
	std::ifstream file;
	long pos = 0;

	// map file to memory
	int mm_idx = (int)mm_vec.size();
	prf_obj.mm_idx = mm_idx;
	// does WholeFile work if file is > memory? Not sure. But I'm pretty sure it would be too much
	// data to plot (in oppat's current form of plotting every event)
	mm_vec.push_back(new MemoryMapped(flnm, MemoryMapped::WholeFile, MemoryMapped::SequentialScan));
	if (!(mm_vec[mm_idx]->isValid()))   {
		printf("File not found\n");
		return -2;
	}
	prf_obj.mm_buf = mm_vec[mm_idx]->getData();
	const unsigned char* mm_buf = prf_obj.mm_buf;
	if (mm_buf == NULL) {
		printf("memory map getData() failed at %s %d\n", __FILE__, __LINE__);
		exit(1);
	}
#if 0
	file.open (flnm, std::ios::in | std::ios::binary);
	if (!file.is_open()) {
		printf("messed up fopen of flnm= %s at %s %d\n", flnm.c_str(), __FILE__, __LINE__);
		exit(1);
	}
#endif
	pos = 0;
	//i = read_n_bytes(file, pos, 8, __LINE__);
	mm_read_n_bytes(mm_buf, pos, 8, __LINE__, buf, BUF_MAX);
	if (memcmp(buf, "PERFILE2", 8) != 0) {
		printf("missed PERFILE2 magic bytes 0-7 for file= %s at %s %d\n", flnm.c_str(), __FILE__, __LINE__);
		printf("bytes[0-7]= %*s at %s %d\n", 8, mm_buf, __FILE__, __LINE__);
		exit(1);
	}
	prf_obj.filename_bin = flnm;
	//read_n_bytes(file, pos, 8, __LINE__);
	mm_read_n_bytes(mm_buf, pos, 8, __LINE__, buf, BUF_MAX);
	uint64_t hdr_sz = *(buf_uint64_ptr(buf, 0));
	printf("hdr_sz from file= %" PRIu64 ", sizeof(perf_hdr)= %d\n", hdr_sz, (int)sizeof(perf_hdr));
	if (hdr_sz != sizeof(perf_hdr)) {
		printf("mismatch on perf_hdr size. not sure what to do here. Bye at %s %d\n", __FILE__, __LINE__);
		exit(1);
	}
	pos = 0;
	//file.seekg(0);
	//read_n_bytes_buf(file, pos, sizeof(perf_hdr), (char *)&perf_hdr, __LINE__);
	mm_read_n_bytes_buf(mm_buf, pos, sizeof(perf_hdr), (char *)&perf_hdr, __LINE__);
	if (verbose) {
		printf("perf_hdr.attr_size= %" PRIu64 "\n", perf_hdr.attr_size);
		printf("flags= 0x%" PRIx64 "\n", perf_hdr.flags);
		for (int i=0; i < 3; i++) {
			printf("flags1[%d]= 0x%" PRIx64 "\n", i, perf_hdr.flags1[i]);
		}
	}

#if 0
	std::vector <perf_file_section> opt_hdrs;
	for (i=0; i < 64; i++) {
		if ((1UL << i) & perf_hdr.flags) {
			perf_file_section pfs;
			read_n_bytes_buf(file, pos, sizeof(pfs), (char *)&pfs, __LINE__);
			printf("opt_hdrs[%d] offset= 0x%lx, size= %ld\n", (int)opt_hdrs.size(), pfs.offset, pfs.size);
			opt_hdrs.push_back(pfs);
			
		}
	}
#endif
	// read perf event attr section
	
	printf("struct perf_event_attr size= %d\n", (int)sizeof(perf_event_attr));
	printf("struct perf_file_section size= %d\n", (int)sizeof(perf_file_section));
	printf("perf_hdr.attrs.size= %" PRIu64 "\n", perf_hdr.attrs.size);
	char *buf_perf_attrs = (char *)malloc(perf_hdr.attrs.size);
	pos = (long)perf_hdr.attrs.offset;
	//file.seekg(pos);
	//read_n_bytes_buf(file, pos, perf_hdr.attrs.size, (char *)buf_perf_attrs, __LINE__);
	mm_read_n_bytes_buf(mm_buf, pos, (int)perf_hdr.attrs.size, (char *)buf_perf_attrs, __LINE__);
	struct perf_file_attr *pfa = (struct perf_file_attr *)buf_perf_attrs;
	int sample_rec_flds = 0, sample_rec_fmt_len=0;
	unsigned long len;
	bool has_ids = false;
	for (uint32_t i=0; i < (int)(perf_hdr.attrs.size/perf_hdr.attr_size); i++) {
		prf_events_str pes;
		pes.pea = pfa->attr;
		std::string decoded_sample_str, decoded_sample_fmt;
		sample_rec_flds    = prf_decode_sample_type(decoded_sample_str, (uint64_t)pfa->attr.sample_type);
		sample_rec_fmt_len = prf_decode_read_format(decoded_sample_fmt, (uint64_t)pfa->attr.read_format);
		std::string evt_name = prf_get_evt_name(pfa->attr.type, (uint64_t)pfa->attr.config, file_list);
		//std::string dec_st = decode_sample_type( (uint64_t)pfa->attr.sample_type);
		printf("event_attr[%d].type= %d, config= %" PRIu64 ", name= %s, sample_id_all= %" PRIu64 ", sample_type= 0x%" PRIx64 ", st_decode= %s, typ_flds= %d, fmt_flds= %d, fmt_str= %s\n",
			i, pfa->attr.type, 
			(uint64_t)pfa->attr.config,
			evt_name.c_str(),
			(uint64_t)pfa->attr.sample_id_all,
			(uint64_t)pfa->attr.sample_type, decoded_sample_str.c_str(), sample_rec_flds,
			sample_rec_fmt_len, decoded_sample_fmt.c_str());
		if (!pfa->attr.sample_id_all) {
			prf_obj.sample_id_all = 0;
		}
		pos = (long)pfa->id.offset;
		//file.seekg(pos);
		len = (unsigned long)pfa->id.size;
		pes.event_name = evt_name;
		prf_obj.events.push_back(pes);
		if (len < sizeof(buf)) {
			if (verbose)
				printf("pfa->id offset= 0x%lu, size= %ld\n", pos, len);
			//read_n_bytes(file, pos, len, __LINE__);
			mm_read_n_bytes(mm_buf, pos, (int)len, __LINE__, buf, BUF_MAX);
			for (uint32_t j=0; j < len/8; j++) {
				uint64_t id = *(buf_uint64_ptr(buf, (int)(j*8)));
				if (verbose)
					printf("id[%d]= %" PRIu64 "\n", j, id);
				prf_obj.events.back().ids.push_back(id);
				has_ids = true;
			}
		}
		if (verbose)
			printf("prf_obj.events[%d].ids.size()= %d at %s %d\n",
					(int)(prf_obj.events.size()-1), (int)(prf_obj.events.back().ids.size()), __FILE__, __LINE__);
		pfa++;
	}
	prf_obj.has_ids = has_ids;

	// option headers (after the data section)
	pos = (long)(perf_hdr.data.size + perf_hdr.data.offset);
	//file.seekg(pos);
	long pos_sv = pos;
#pragma pack(push, 1)
	struct feature_str {
		perf_file_section pfs;
		uint64_t indx;
	};
#pragma pack(pop)
		
	uint64_t sz_cur, sz_max=0;
#pragma pack(push, 1)
	perf_file_section opt_hdr;
#pragma pack(pop)
	std::vector <feature_str> features;
	for (int i=0; i < 64; i++) {
		if ((uint64_t)(1ULL << i) & perf_hdr.flags) {
			//read_n_bytes_buf(file, pos, sizeof(opt_hdr), (char *)&opt_hdr, __LINE__);
			mm_read_n_bytes_buf(mm_buf, pos, sizeof(opt_hdr), (char *)&opt_hdr, __LINE__);
			struct feature_str fs;
			fs.pfs = opt_hdr;
			fs.indx = (uint64_t)i;
			if (sz_max < opt_hdr.size) {
				sz_max = opt_hdr.size;
			}
			char *feat = "unknown";
			if (i < HEADER_LAST_FEATURE) {
				feat = feat_str[i];
			}
			printf("features[%d]: i= %d feat_str= %s offset= %" PRIu64 ", size= %" PRIu64 "\n", (int)features.size(), i, feat, opt_hdr.offset, opt_hdr.size);
			features.push_back(fs);
		}
	}
#pragma pack(push, 1)
	struct perf_event_header {
		uint32_t type;
		uint16_t misc;
		uint16_t size;
	} evt_hdr;
#pragma pack(pop)
	unsigned long recs= 0, sz_nxt= 0, sz_tot=0;


	//Some headers consist of a sequence of strings, which start with a 

	if (sz_max > 0) {
		char *feat_buf = (char *)malloc(sz_max);
		if (verbose) {
			printf("feat_buf sz_max= %d at %s %d\n", (int)sz_max, __FILE__, __LINE__);
			fflush(NULL);
		}
		for (uint32_t i=0; i < features.size(); i++) {
			//long pos_sz;
			pos    = (long) (perf_hdr.data.offset + perf_hdr.data.size + i * sizeof(opt_hdr));
			//file.seekg(pos);
			//read_n_bytes_buf(file, pos, sizeof(opt_hdr), (char *)&opt_hdr, __LINE__);
			mm_read_n_bytes_buf(mm_buf, pos, sizeof(opt_hdr), (char *)&opt_hdr, __LINE__);
			pos    = (long)features[i].pfs.offset;
			sz_cur = features[i].pfs.size;
			pos_sv = pos;
			char *feat = "unknown";
			if (features[i].indx < HEADER_LAST_FEATURE) {
				feat = feat_str[features[i].indx];
			} else {
				continue;
			}
			//file.seekg(pos);
			//read_n_bytes_buf(file, pos, sz_cur, feat_buf, __LINE__);
			if (verbose) {
				printf("bef feat[%d]: indx= %" PRIu64 " off= %ld, sz= %" PRIu64 ", pfs.off= %" PRIu64 " sz= %" PRIu64 "\n",
						i, features[i].indx, pos_sv, sz_cur, opt_hdr.offset, opt_hdr.size);
				fflush(NULL);
			}
			mm_read_n_bytes_buf(mm_buf, pos, (int)sz_cur, feat_buf, __LINE__);
			if (verbose ) {
				printf("aft feat[%d]: off= %ld, sz= %" PRIu64 ", pfs.off= %" PRIu64 " sz= %" PRIu64 "\n",
						i, pos_sv, sz_cur, opt_hdr.offset, opt_hdr.size);
				fflush(NULL);
			}
			int off=0;
			if (features[i].indx == HEADER_TRACING_DATA) {
				//printf("uint32= %u\n", *(uint32_t *)feat_buf);
				//struct perf_event_header	header;
				//pevt_hdr = (struct perf_event_header *)feat_buf;
				memcpy(&evt_hdr, feat_buf, sizeof(evt_hdr));
				printf("TRACING_DATA: evt[%ld] typ= %d, misc= 0x%x, size= %d, sz_cur= %" PRIu64 " sz_nxt= %lu, sz_tot= %lu, data.size= %" PRIu64 "\n",
				recs, evt_hdr.type, evt_hdr.misc, evt_hdr.size, sz_cur, sz_nxt, sz_tot, perf_hdr.data.size);
			}
			else if (features[i].indx == HEADER_BUILD_ID) {
				/*
				from https://lwn.net/Articles/644919/
The header consists of an sequence of build_id_event. The size of each record
is defined by header.size (see perf_event.h). Each event defines a ELF build id
for a executable file name for a pid. An ELF build id is a unique identifier
assigned by the linker to an executable.
				*/
#pragma pack(push, 1)
				struct build_id_event {
					struct perf_event_header header;
					pid_t			 pid;
					uint8_t			 build_id[24];
					char			 *filename;
					//char			 filename[header.size - offsetof(struct build_id_event, filename)];
				} *pevt_hdr;
#pragma pack(pop)
				int str_off = (sizeof(pevt_hdr->header) + sizeof(pid_t) + sizeof(pevt_hdr->build_id));
				sz_tot = 0;
				while (sz_tot < opt_hdr.size) {
					char *cp = feat_buf+sz_tot;
					pevt_hdr = (struct build_id_event *)(cp);
					sz_cur = pevt_hdr->header.size;
					int sz = (int)(sz_cur - str_off);
					sz_tot += (unsigned long)sz_cur;
					if (verbose > 0) {
						printf("evt[%ld] typ= %d, misc= 0x%x, size= %d, pid= %d, sz= %d str=",
							recs, pevt_hdr->header.type, pevt_hdr->header.misc, pevt_hdr->header.size, pevt_hdr->pid, sz);
						for(int j=0; j < sz; j++) {
							if (cp[str_off+j] == 0) {
								break;
							}
							printf("%c", cp[str_off+j]);
						}
						printf("\n");
					}
				}
			} else if (features[i].indx == HEADER_HOSTNAME) {
				prf_obj.features_hostname = prf_prt_str(feat, feat_buf, off);
			} else if (features[i].indx == HEADER_OSRELEASE) {
				prf_prt_str(feat, feat_buf, off);
			} else if (features[i].indx == HEADER_VERSION) {
				prf_prt_str(feat, feat_buf, off);
			} else if (features[i].indx == HEADER_ARCH) {
				prf_obj.features_arch = prf_prt_str(feat, feat_buf, off);
			} else if (features[i].indx == HEADER_NRCPUS) {
				prf_prt_nrcpus(feat, feat_buf, prf_obj);
			} else if (features[i].indx == HEADER_CPUDESC) {
				prf_obj.features_cpudesc = prf_prt_str(feat, feat_buf, off);
			} else if (features[i].indx == HEADER_CPUID) {
				prf_obj.features_cpuid = prf_prt_str(feat, feat_buf, off);
			} else if (features[i].indx == HEADER_TOTAL_MEM) {
				prf_prt_total_mem(feat, feat_buf);
			} else if (features[i].indx == HEADER_CMDLINE) {
				prf_obj.features_cmdline = prf_prt_str_lst(feat, feat_buf, off);
			} else if (features[i].indx == HEADER_EVENT_DESC) {
				prf_prt_event_desc(feat, feat_buf, prf_obj);
			} else if (features[i].indx == HEADER_CPU_TOPOLOGY) {
				printf("cores: %s\n", feat_buf);
				prf_obj.features_topology_cores = prf_prt_str_lst(feat, feat_buf, off);
				printf("threads: %s\n", feat_buf+off);
				prf_obj.features_topology_threads = prf_prt_str_lst(feat, feat_buf+off, off);
			} else if (features[i].indx == HEADER_NUMA_TOPOLOGY) {
				prf_prt_numa_topology(feat, feat_buf, prf_obj);
			} else if (features[i].indx == HEADER_PMU_MAPPINGS) {
				prf_prt_pmu_mappings(feat, feat_buf, prf_obj);
			} else if (features[i].indx == HEADER_GROUP_DESC) {
				prf_prt_group_desc(feat, feat_buf, prf_obj);
			} else if (features[i].indx == HEADER_STAT) {
				//This is merely a flag signifying that the data section contains data
				//recorded from perf stat record.) 
				// I have no idea what this flag means or how to cause it to be set.
				printf("%s, yes, data section has perf stat record data\n", feat);
			} else if (features[i].indx == HEADER_CACHE) {
				prf_prt_cache(feat, feat_buf, prf_obj);
			} else if (features[i].indx == HEADER_SAMPLE_TIME) {
				prf_prt_sample_time(feat, feat_buf, prf_obj);
			} else {
				printf("unhandled feature header: %" PRIu64 "\n", features[i].indx);
			}
		}
	}
		
	// data section
	recs= 0;
	sz_tot=0;
	pos = (long)perf_hdr.data.offset;
	//file.seekg(pos);
	fprintf(stderr, "before read data section, elap= %f at %s %d\n", dclock()-tm_beg, __FILE__, __LINE__);
	printf("data.offset= %ld, size= %" PRIu64 "\n", pos, perf_hdr.data.size);
	double tm_in_rd = 0, tm_in_dec=0;
	while (sz_tot < perf_hdr.data.size) {
		sz_cur = sizeof(evt_hdr);
		//read_n_bytes_buf(file, pos, sz_cur, (char *)&evt_hdr, __LINE__);
		double tm_rd0 = dclock();
		long pos_rec0 = pos;
		mm_read_n_bytes_buf(mm_buf, pos, (int)sz_cur, (char *)&evt_hdr, __LINE__);
		double tm_rd1 = dclock();
		tm_in_rd += tm_rd1 - tm_rd0;
		sz_tot += (unsigned long)sz_cur;
		sz_nxt = (unsigned long)(evt_hdr.size - sz_cur);
		if (verbose > 0)
			printf("evt[%ld] typ= %d, misc= 0x%x, size= %d, sz_cur= %" PRId64 " sz_nxt= %lu, sz_tot= %lu, pos= 0x%lx, data.size= %" PRIu64 "\n",
				recs, evt_hdr.type, evt_hdr.misc, evt_hdr.size, sz_cur, sz_nxt, sz_tot, pos_rec0, perf_hdr.data.size);
		if (sz_nxt > 0 && sz_nxt < sizeof(buf)) {
			//read_n_bytes(file, pos, sz_nxt, __LINE__);
			double tm_rd2 = dclock();
			long pos_rec = pos;
			mm_read_n_bytes(mm_buf, pos, sz_nxt, __LINE__, buf, BUF_MAX);
			double tm_rd3 = dclock();
			tm_in_rd += tm_rd3 - tm_rd2;
			prf_decode_perf_record(pos_rec, evt_hdr.type, buf, 1, prf_obj, tm_beg, file_list);
			double tm_rd4 = dclock();
			tm_in_dec += tm_rd4 - tm_rd3;
			sz_tot += sz_nxt;
		} else if (sz_nxt >= sizeof(buf)) {
			printf("got sz_nxt too big...... for buf at %s %d\n", __FILE__, __LINE__);
			exit(1);
		}
		recs++;
	}
	fprintf(stderr, "after read data section, elap= %f, tm_in_rd= %f, tm_in_decode= %f at %s %d\n", dclock() - tm_beg, tm_in_rd, tm_in_dec, __FILE__, __LINE__);
	if (verbose) {
		tm_print();
	}
	std::sort(prf_obj.samples.begin(), prf_obj.samples.end(), compareByTime);
	//file.close();
	return 0;
}

