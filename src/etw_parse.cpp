/* Copyright (c) 2018 Patrick Fay
 *
 * License http://opensource.org/licenses/mit-license.php MIT License
 */

#ifdef _WIN32
#include <windows.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <cstddef>
#include <unordered_map>
#include <chrono>
#include <time.h>
#include <inttypes.h>
#include "utils2.h"
#include "utils.h"
#if 1
#include "perf_event_subset.h"
#else
#include <linux/perf_event.h>
#endif
#include "rd_json2.h"
#include "rd_json.h"
#include "printf.h"
#include "oppat.h"
#include "pugixml.hpp"
#include "lua_rtns.h"
#include "etw_parse.h"

void parse_etw_cfg_xml(std::string &cfg_str, prf_obj_str &prf_obj, int verbose)
{
	pugi::xml_document doc;
	const char *source = cfg_str.c_str();
	pugi::xml_parse_result result = doc.load_string(source);
	pugi::xml_node tools = doc.child("SystemConfig").child("Processor");

	if (result)
	{
		std::cout << "XML parsed without errors, attr value: [" << doc.child("node").attribute("attr").value() << "]\n\n";
		if (verbose) {
			std::cout << "source XML: [" << source << "] << std::endl";
		}
	}
	else
	{
		std::cout << "XML [" << source << "] parsed with errors, attr value: [" << doc.child("node").attribute("attr").value() << "]\n";
		std::cout << "Error description: " << result.description() << "\n";
		std::cout << "Error offset: " << result.offset << " (error at [..." << (source + result.offset) << "]\n\n";
		std::cout << "Below is XML:" << std::endl << source << std::endl;
	}
	for (pugi::xml_node tool = tools.child("Instance"); tool; tool = tool.next_sibling("Instance"))
	{
		std::cout << "Processor " << tool.child_value("ProcessorName") << "\n";
		int num_cpus = atoi(tool.child_value("NumCPUs"));
		printf("cfg num_cpus= %d at %s %d\n", num_cpus, __FILE__, __LINE__);
		prf_obj.features_nr_cpus_online = num_cpus;
		prf_obj.features_nr_cpus_available = num_cpus;
	}
}

static int etw_add_event(int idx, std::string evt_str, prf_obj_str &prf_obj, std::vector <std::string> cols)
{
	int evt_id= idx;
	prf_events_str pes;
	pes.event_name = evt_str;
	pes.event_name_w_area = evt_str;
	pes.etw_cols = cols;
	pes.pea.type = PERF_TYPE_ETW;
	pes.pea.config = (uint64_t)evt_id;
	//pes.event_area = std::string(area);
	printf("etw event name= '%s', ID= %d, num_evts= %d\n", evt_str.c_str(), evt_id, (int)prf_obj.events.size());
	prf_add_ids((uint32_t)evt_id, (int)prf_obj.events.size(), prf_obj);
	for (uint32_t i=0; i < cols.size(); i++) {
		hash_string(pes.etw_col_hsh, pes.etw_col_vec, cols[i]);
	}
	//uint32_t ck_idx, ck_evt;
	//ck_idx = prf_obj.ids_2_evt_indxp1[evt_id];
	//ck_evt = prf_obj.ids_vec[ck_idx-1];
	//printf("hash ck_evt= %d, evt_idx= %d at %s %d\n", ck_evt, (int)prf_obj.events.size(), __FILE__, __LINE__);
	prf_obj.events.push_back(pes);
	return 1;
}

int etw_split_comm(const std::string &s, std::string &comm, std::string &pid, double ts)
{
	std::string::size_type pos;
	pos = s.find(" (");
	if (pos == std::string::npos) {
		printf("didn't find ' (' in string '%s' in ETW file. ts= %.0f at %s %d\n", s.c_str(), ts, __FILE__, __LINE__);
		return -1;
	}
	comm = s.substr(0, pos);
	if (comm.size() > 3 && comm.front() == '"' && comm.back() == '"') {
		//printf("etw_comm bef= %s at %s %d\n", comm.c_str(), __FILE__, __LINE__);
		comm = comm.substr(1, comm.size()-2);
		//printf("etw_comm aft= %s at %s %d\n", comm.c_str(), __FILE__, __LINE__);
	}
	std::string rest = s.substr(pos+2);
	pos = rest.find_first_not_of(" ");
	if (pos == std::string::npos) {
		pid = rest.substr(0, rest.length()-1);
	} else {
		pid = rest.substr(pos, rest.length()-1-pos);
	}
	return 0;
}

static std::vector <std::string> WinSAT_SystemConfig;

int add_threads(std::vector <std::string> &cols, prf_obj_str &prf_obj, uint32_t t_idx, double ts, int verbose)
{
	uint32_t pids = 0;
	uint32_t pid_col = prf_obj.events[t_idx].etw_col_hsh["Process Name ( PID)"] - 1;
	uint32_t tid_col = prf_obj.events[t_idx].etw_col_hsh["ThreadID"] - 1;
	//printf("T-Start cols: pid= %d, tid= %d\n", pid_col, tid_col);
	std::string comm, pid, tid;
	for (uint32_t i=0; i < prf_obj.etw_evts_set[t_idx].size(); i++) {
		uint32_t data_idx = prf_obj.etw_evts_set[t_idx][i].data_idx;
		uint64_t ts       = prf_obj.etw_evts_set[t_idx][i].ts;
		etw_split_comm(prf_obj.etw_data[data_idx][pid_col], comm, pid, ts);
		tid = prf_obj.etw_data[data_idx][tid_col];
		uint32_t ipid, itid;
		ipid = atoi(pid.c_str());
		itid = atoi(tid.c_str());
		double tm = 1.0e-6 * ts + prf_obj.tm_beg;
		prf_add_comm(ipid, itid, comm, prf_obj, tm);
		//hash_comm_pid_tid(comm_pid_tid_hash, comm_pid_tid_vec, comm, ipid, itid);
		if (verbose > 0) 
			printf("tid[%d]: %s %s, comm= '%s', pid= %s, tm= %f\n", pids, prf_obj.etw_data[data_idx][pid_col].c_str(),
				tid.c_str(), comm.c_str(), pid.c_str(), tm);
		pids++;
	}
	return pids;
}

struct nms_str {
	std::string str;
	std::vector <std::string> cols;
	int count, len, ext_strs;
};

static uint32_t add_evt_and_cols(prf_obj_str &prf_obj, std::vector <std::string> tkns, std::vector <nms_str> &nms)
{
	hash_string(prf_obj.etw_evts_hsh, prf_obj.etw_evts_vec, tkns[0]);
	std::vector <etw_str> es;
	prf_obj.etw_evts_set.push_back(es);
	struct nms_str ns;
	ns.str = tkns[0];
	ns.cols = tkns;
	ns.count = 0;
	ns.ext_strs = 0;
	ns.len = ns.str.size();
	etw_add_event((int)nms.size(), tkns[0], prf_obj, tkns);
	nms.push_back(ns);
	return nms.size() - 1;
}

int etw_parse_text(std::string flnm, prf_obj_str &prf_obj, double tm_beg_in, int verbose, std::vector <evt_str> &evt_tbl2)
{
	std::ifstream file;
	//long pos = 0;
	std::string line;
	int lines_comments = 0, lines_samples = 0, lines_callstack = 0, lines_null=0;
	int samples = -1, s_idx = -1;


	std::vector <nms_str> nms;

#if 0
	for (uint32_t i=0; i < prf_obj.features_event_desc.size(); i++) {
		struct nms_str ns;
		ns.str = " " + prf_obj.features_event_desc[i].event_string + ": ";
		ns.count = 0;
		ns.ext_strs = 0;
		ns.len = ns.str.size();
		nms.push_back(ns);
	}
#endif

	prf_obj.filename_text = flnm;

	int store_callstack_idx = -1;
	double tm_beg = dclock();
	uint32_t line_num = UINT32_M1;
	file.open (flnm.c_str(), std::ios::in);
	if (!file.is_open()) {
		printf("messed up fopen of flnm= %s at %s %d\n", flnm.c_str(), __FILE__, __LINE__);
		exit(1);
	}
	bool doing_header = true;
	std::getline (file, line);
	line_num++;
	bool pop_char = false;
	if (line == "BeginHeader") {
		printf("got BeginHeader at %s %d\n", __FILE__, __LINE__);
	} else {
		if ( line.size() && line[line.size()-1] == '\r' ) {
			line.pop_back();
			pop_char = true;
		}
		if (line != "BeginHeader") {
		printf("expected 1st line of ETW text file '%s' to be string 'BeginHeader'.\ngot str= '%s'. Bye at %s %d\n",
			flnm.c_str(), line.c_str(), __FILE__, __LINE__);
		exit(1);
		}
	}
	printf("got %d events in ETW txt file at %s %d\n", (int)nms.size(), __FILE__, __LINE__);
	std::vector <std::string> tkns, tkns2;
#if 0
	std::unordered_map<std::string, uint32_t> evts_hsh;
	std::vector <std::string> evts_vec;
	struct etw_str {
		uint32_t txt_idx, data_idx;
		uint64_t ts;
		etw_str(): txt_idx(-1), data_idx(-1), ts(0) {}
	};
	std::vector <std::vector <etw_str>> evts_set;
#endif
	uint32_t csw = 0, csw2=0, csw_idx=-1;
	while(!file.eof()){
	    std::getline (file, line);
		if (pop_char && line.size()) {
			line.pop_back();
		}
		line_num++;
		tkn_split(line, ", ", tkns);
#if 0
		printf("line= '%s'\n", line.c_str());
		for (uint32_t i=0; i < tkns.size(); i++) {
			printf("tkn[%d]= '%s'\n", i, tkns[i].c_str());
		}
#endif
		if (line == "EndHeader") {
			doing_header = false;
			break;
		}
		// ck for duplicates
		uint32_t hsh_ck = prf_obj.etw_evts_hsh[tkns[0]];
		if (hsh_ck != 0) {
			continue;
		}
		add_evt_and_cols(prf_obj, tkns, nms);
#if 0
		hash_string(prf_obj.etw_evts_hsh, prf_obj.etw_evts_vec, tkns[0]);
		std::vector <etw_str> es;
		prf_obj.etw_evts_set.push_back(es);
		struct nms_str ns;
		ns.str = tkns[0];
		ns.cols = tkns;
		ns.count = 0;
		ns.ext_strs = 0;
		ns.len = ns.str.size();
		etw_add_event((int)nms.size(), tkns[0], prf_obj, tkns);
		nms.push_back(ns);
#endif
	}

	struct evts_derived_str {
		uint32_t evt_tbl2_idx, trigger_idx, evt_new_idx;
		std::vector <uint32_t> evts_used;
		std::vector <std::string> new_cols;
		std::vector <std::string> new_vals;
	};

	std::vector <evts_derived_str> evts_derived;

	for (uint32_t i=0; i < evt_tbl2.size(); i++) {
		if (evt_tbl2[i].event_type == "ETW" && evt_tbl2[i].evt_derived.evts_used.size() > 0) {
			bool okay= true;
			evts_derived_str eds;
			eds.evts_used.resize(0);
			uint32_t hsh_ck;
			for (uint32_t j=0; j < evt_tbl2[i].evt_derived.evts_used.size(); j++) {
				hsh_ck = prf_obj.etw_evts_hsh[evt_tbl2[i].evt_derived.evts_used[j]];
				if (hsh_ck == 0) {
					okay= false;
					break;
				}
				eds.evts_used.push_back(hsh_ck - 1);
			}
			std::string trigger = evt_tbl2[i].evt_derived.evt_trigger;
			uint32_t trgr_idx = UINT32_M1;
			if (okay) {
				if (trigger != "__EMIT__") {
					hsh_ck = prf_obj.etw_evts_hsh[trigger];
					if (hsh_ck == 0) {
						okay = false;
					}
				} else {
					trgr_idx = UINT32_M2;
				}
			}
			if (okay) {
				std::string new_nm  = evt_tbl2[i].event_name;
				std::vector <std::string> tkns;
				if (trgr_idx != UINT32_M2) {
					hsh_ck = prf_obj.etw_evts_hsh[trigger] - 1;
					tkns = prf_obj.events[hsh_ck].etw_cols;
					tkns[0] = new_nm;
					for (uint32_t j=0; j < evt_tbl2[i].evt_derived.new_cols.size(); j++) {
						tkns.push_back(evt_tbl2[i].evt_derived.new_cols[j]);
					}
				} else {
					hsh_ck = prf_obj.etw_evts_hsh[evt_tbl2[i].evt_derived.evts_used[0]] - 1;
					tkns = prf_obj.events[hsh_ck].etw_cols;
					tkns[0] = new_nm;
					for (uint32_t j=0; j < evt_tbl2[i].evt_derived.new_cols.size(); j++) {
						tkns.push_back(evt_tbl2[i].evt_derived.new_cols[j]);
					}
				}
				uint32_t new_idx = add_evt_and_cols(prf_obj, tkns, nms);
				eds.evt_tbl2_idx = i;
				if (trgr_idx != UINT32_M2) {
					eds.trigger_idx = hsh_ck;
				} else {
					eds.trigger_idx = trgr_idx;
				}
				eds.evt_new_idx = new_idx;
				evts_derived.push_back(eds);
				for (uint32_t j=0; j < evt_tbl2[i].evt_derived.new_cols.size(); j++) {
					evts_derived.back().new_cols.push_back(evt_tbl2[i].evt_derived.new_cols[j]);
				}
				printf("trigger= %s, tkns[0]= %s, new_nm= %s trg_idx= %d, new_idx= %d evts_der.back().used.sz= %d at %s %d\n",
					trigger.c_str(), tkns[0].c_str(), new_nm.c_str(), hsh_ck, new_idx,
					(int)evts_derived.back().evts_used.size(), __FILE__, __LINE__);
				for (uint32_t j=0; j < evts_derived.back().evts_used.size(); j++) {
					hsh_ck = prf_obj.etw_evts_hsh[evt_tbl2[i].evt_derived.evts_used[j]] - 1;
					std::string nm = prf_obj.events[hsh_ck].etw_cols[0];
					printf("evt_used[%d]= %s at %s %d\n", j, nm.c_str(), __FILE__, __LINE__);
				}
			}
		}
	}
	if (doing_header) {
		printf("missed 'EndHeader' string in ETW text file '%s'. Bye at %s %d\n", flnm.c_str(), __FILE__, __LINE__);
		exit(1);
	}
	std::getline (file, line);
	if (pop_char && line.size()) {
		line.pop_back();
	}
	line_num++;
	tkn_split(line, ", ", tkns);
	std::string lkfor = "OS Version:", lkfor2, lkfor3;
	if (tkns.size() == 0 || tkns[0].find(lkfor) == std::string::npos) {
		printf("missed '%s' string in ETW text file '%s'. Bye at %s %d\n", lkfor.c_str(), flnm.c_str(), __FILE__, __LINE__);
		exit(1);
	}
	uint64_t start_time=0;
	uint32_t ptr_sz = 0;
	std::string trace_file;
	lkfor = "Trace Start: ";
	lkfor2 = "PointerSize: ";
	lkfor3 = "Trace Name: ";
	for (uint32_t i=0; i < tkns.size(); i++) {
		if (start_time == 0 && tkns[i].find(lkfor) != std::string::npos) {
			start_time = atoll(tkns[i].substr(lkfor.size()).c_str());
		}
		if (ptr_sz == 0 && tkns[i].find(lkfor2) != std::string::npos) {
			ptr_sz = atoi(tkns[i].substr(lkfor2.size()).c_str());
		}
		if (trace_file.size() == 0 && tkns[i].find(lkfor3) != std::string::npos) {
			trace_file = tkns[i].substr(lkfor3.size());
		}
		if (verbose)
			printf("tkn[%d]= '%s'\n", i, tkns[i].c_str());
	}
	if (start_time == 0) {
		printf("missed '%s' string in ETW text file '%s', line= '%s'.\nBye at %s %d\n",
			lkfor.c_str(), flnm.c_str(), line.c_str(), __FILE__, __LINE__);
		exit(1);
	}
	printf("got %d events in ETW txt file. start_time= %" PRId64 ", ptr_sz= %d, trace_bin_file= %s at %s %d\n",
			(int)prf_obj.events.size(), start_time, ptr_sz, trace_file.c_str(), __FILE__, __LINE__);
//OS Version: 10.0.17134, Trace Size: 13952KB, Events Lost: 0, Buffers lost: 0, Trace Start: 131777972134992100, Trace Length: 3 sec, PointerSize: 8, Trace Name: mytrace.etl
	prf_obj.filename_bin = trace_file;
	prf_obj.tm_beg = 1.0e-7 * (double)start_time; // start_time is a FILETIME: 100 ns units

	//std::vector <std::vector <std::string>> etw_data;
	uint32_t evt_idx, evt_idx_prv=(uint32_t)-2;
	const std::string lkfor_err = "Error: ";
	const uint32_t err_sz = lkfor_err.size();
	uint64_t last_ts=0, first_ts = 0;
	bool first_ts_valid = false;
	uint32_t stk_idx = prf_obj.etw_evts_hsh["Stack"] - 1;
	uint32_t sysconfig_idx = prf_obj.etw_evts_hsh["WinSAT_SystemConfig"] - 1;
	uint32_t thr_st0 = prf_obj.etw_evts_hsh["T-DCStart"] - 1;
	uint32_t thr_st1 = prf_obj.etw_evts_hsh["T-Start"] - 1;
	double tm_beg2 = dclock();
	while(!file.eof()) {
		//read data from file
		std::getline (file, line);
		if (line.size() < 2) {
			continue;
		}
		if (pop_char && line.size()) {
			line.pop_back();
		}
		line_num++;
		tkn_split(line, ", ", tkns);
		if (tkns.size() > 1) {
#if 0
			if (tkns[0].size() >= err_sz) {
				if (tkns[0].substr(0, err_sz) == lkfor_err) {
					printf("skipping etw line= '%s' at %s %d\n", line.c_str(), __FILE__, __LINE__);
					continue;
				}
			}
#endif
			evt_idx = prf_obj.etw_evts_hsh[tkns[0]] - 1;
#if 1
			if (evt_idx == UINT32_M1) {
				printf("skipping etw line= '%s' at %s %d\n", line.c_str(), __FILE__, __LINE__);
				continue;
				//exit(1);
			}
#endif
			etw_str es;
			es.ts = atoll(tkns[1].c_str());
			double tm = prf_obj.tm_beg + 1.0e-6 * (double)es.ts;
			//printf("etw ts= %f, tm_beg= %f, tm_end= %f at %s %d\n",
			//	   tm, options.tm_clip_beg, options.tm_clip_end, __FILE__, __LINE__);
			if (sysconfig_idx == evt_idx) {
				if (WinSAT_SystemConfig.size() > 0) {
					printf("need code to handle more than 1 WinSAT_SystemConfig event. Bye at %s %d\n", __FILE__, __LINE__);
					exit(1);
				}
				WinSAT_SystemConfig = tkns;
				continue;
			}
#if 1
			if (options.tm_clip_end_valid == CLIP_LVL_1 && tm > options.tm_clip_end) {
				continue;
			}
#endif
			if (evt_idx == thr_st0 || evt_idx == thr_st1) {
				add_threads(tkns, prf_obj, evt_idx, (double)es.ts, verbose);
			}
#if 1
			if (options.tm_clip_beg_valid == CLIP_LVL_1 && tm < options.tm_clip_beg) {
				continue;
			}
#endif
			if (ETW_events_to_skip_hash[tkns[0]] != 0) {
				//printf("skipping ETW event: %s line= %d at %s %d\n", tkns[0].c_str(), line_num, __FILE__, __LINE__);
				continue;
			}
			if (evt_idx >= prf_obj.etw_evts_set.size()) {
				printf("evt_idx= %d >= prf_obj.etw_evts_set.size()= %d at %s %d\n",
					evt_idx, (int)prf_obj.etw_evts_set.size(), __FILE__, __LINE__);
				exit(1);
			}
			if (evt_idx == stk_idx && evt_idx_prv != (uint32_t)-2) {
				uint32_t stk_sz = prf_obj.etw_data.size();
				if (prf_obj.etw_evts_set[evt_idx_prv].back().cs_idx_beg == UINT32_M1) {
					prf_obj.etw_evts_set[evt_idx_prv].back().cs_idx_beg = stk_sz;
				}
				prf_obj.etw_evts_set[evt_idx_prv].back().cs_idx_end = stk_sz;
				//printf("evt_idx= %d, prv= %d, evt_prv= %s, stk_beg= %d at %s %d\n",
					//evt_idx, evt_idx_prv, prf_obj.etw_evts_vec[evt_idx_prv].c_str(), stk_sz, __FILE__, __LINE__);
			}
			if (tkns[0] == "CSwitch") {
				if (csw == 0) {
					printf("CSwitch evt_idx= %d\n", evt_idx);
					csw_idx = evt_idx;
				}
				csw2 = prf_obj.etw_evts_set[evt_idx].size();
				csw++;
			}
			es.txt_idx = line_num;
			es.data_idx = prf_obj.etw_data.size();
			if (!first_ts_valid) {
				first_ts = es.ts;
				first_ts_valid = true;
			}
			last_ts = es.ts;
			es.ts -= first_ts;
			prf_obj.etw_evts_set[evt_idx].push_back(es);
			prf_obj.etw_data.push_back(tkns);
			bool prt = false, got_it = false;
			for (uint32_t j=0; j < evts_derived.size(); j++) {
				uint32_t tbl2_idx = evts_derived[j].evt_tbl2_idx;
				uint32_t trig_idx = evts_derived[j].trigger_idx;
				uint32_t new_idx = evts_derived[j].evt_new_idx;
				std::vector <std::string> &cols = prf_obj.events[evt_idx].etw_cols;
				for (uint32_t k=0; k < evts_derived[j].evts_used.size(); k++) {
					if (evts_derived[j].evts_used[k] == evt_idx) {
						uint32_t new_sz = evts_derived[j].new_cols.size();
						uint32_t old_sz = tkns.size();
						tkns2.resize(old_sz);
						//for (uint32_t m=0; m < old_sz; m++) {
						//	tkns2.push_back(tkns[m]);
						//}
						tkns2 = tkns;
						tkns2.resize(old_sz+new_sz);
						got_it = true;
						//printf("got evts_used= %s ts %s, at %s %d\n", tkns[0].c_str(), tkns[1].c_str(), __FILE__, __LINE__);
						std::string lua_file = evt_tbl2[tbl2_idx].evt_derived.lua_file;
						std::string lua_rtn  = evt_tbl2[tbl2_idx].evt_derived.lua_rtn;
						std::vector <std::string> new_vals;
						new_vals.resize(new_sz);

						int trig = lua_derived_evt(lua_file, lua_rtn, cols[0], cols, tkns, evts_derived[j].new_cols, new_vals, verbose);
						if (trig_idx == evt_idx || (trig_idx == UINT32_M2 &&  trig == 1)) {
							tkns2[0] = prf_obj.events[new_idx].etw_cols[0];
							if (verbose > 0)
								printf("trigger: new_idx= %d, etw_cols.zs= %d, tkns2.sz= %d, at %s %d\n",
									new_idx, (int)prf_obj.events[evt_idx].etw_cols.size(), (int)tkns2.size(), __FILE__, __LINE__);
							if (tkns2.size() < (old_sz+new_sz)) {
								printf("ummm. dude. bye at %s %d\n", __FILE__, __LINE__);
								exit(1);
							}
							for (uint32_t kk=0; kk < new_sz; kk++) {
								tkns2[old_sz + kk] = new_vals[kk];
							}
							es.data_idx = prf_obj.etw_data.size();
							prf_obj.etw_evts_set[new_idx].push_back(es);
							prf_obj.etw_data.push_back(tkns2);
							if (verbose > 0)
								printf("current evt is trigger event at %s %d\n", __FILE__, __LINE__);
						}
						//run_heapchk("etw_parse:", __FILE__, __LINE__, 1);
						break;
					}
				}
				/*
				if (got_it) {
					// if we break here then, if an event is used in more than 1 derived event, the 2+ der_evt won't see the event
					break;
				}
				*/
			}
			if (evt_idx != stk_idx) {
				evt_idx_prv = evt_idx;
			}
		}
	}
	double tm_end2 = dclock();
	prf_obj.tm_end = prf_obj.tm_beg + 1.0e-6 * (double)last_ts; // tm offset is usecs;
	prf_obj.tm_beg_offset_due_to_clip = 1.0e-6 * (double)first_ts;
	prf_obj.tm_beg += 1.0e-6 * (double)first_ts;
	printf("last_ts - first_ts = %f secs at %s %d\n", 1.0e-6 * (double)(last_ts - first_ts), __FILE__, __LINE__);
	if (verbose) {
		printf("csw= %d, csw2= %d at %s %d\n", csw, csw2, __FILE__, __LINE__);
		fflush(NULL);
	}
	if (csw_idx == UINT32_M1) {
		printf("didn't get cswitch event, bye at %s %d\n", __FILE__, __LINE__);
		exit(1);
	}
	int csw_sz = (csw_idx != UINT32_M1 ? (int)prf_obj.etw_evts_set[csw_idx].size() : 0);
	printf("parsed file in %f secs, prf_obj.etw_evts_set.size()= %d csw_sz= %d at %s %d\n",
			tm_end2-tm_beg2, (int)prf_obj.etw_evts_set.size(), csw_sz, __FILE__, __LINE__);
	fflush(NULL);
	if (WinSAT_SystemConfig.size() == 0) {
		printf("sorry but I need event WinSAT_SystemConfig in the ETW trace text file '%s'. bye at %s %d\n",
				flnm.c_str(), __FILE__, __LINE__);
		exit(1);
	}
	uint32_t cols = WinSAT_SystemConfig.size();
	if (cols < 4) {
		printf("expected 4 fields for WinSAT_SystemConfig event at %s %d\n", __FILE__, __LINE__);
		exit(1);
	}
	parse_etw_cfg_xml(WinSAT_SystemConfig[3], prf_obj, verbose);

	uint32_t SampledProfile_idx = prf_obj.etw_evts_hsh["SampledProfile"] -1;
	uint32_t SampledProfile_cols = UINT32_M1;
	if (SampledProfile_idx != UINT32_M1) {
		SampledProfile_cols = prf_obj.events[SampledProfile_idx].etw_cols.size();
	}
	for (uint32_t i=0; i < prf_obj.etw_evts_set.size(); i++) {
		double tot_cols = 0.0, count = 0.0;
		for (uint32_t j=0; j < prf_obj.etw_evts_set[i].size(); j++) {
			uint32_t data_idx = prf_obj.etw_evts_set[i][j].data_idx;
			uint32_t cols = prf_obj.etw_data[data_idx].size();
			tot_cols += cols;
			count += 1.0;
			if (i == SampledProfile_idx && cols != SampledProfile_cols) {
				printf("SampledProfile cols= %d vs exp cols= %d for line %d at %s %d\n",
						cols, SampledProfile_cols,
						prf_obj.etw_evts_set[i][j].txt_idx, __FILE__, __LINE__);
			}
		}
		double avg = (count > 0.0 ? tot_cols / count : 0.0);
		avg = avg / (double)(prf_obj.events[i].etw_cols.size());
		if (count > 0 && avg != 1.0) {
			printf("evt[%d] avg cols= %f count= %f evt= %s sz= %d, at %s %d\n",
					i, avg, count, prf_obj.events[i].event_name.c_str(), (int)prf_obj.etw_evts_set[i].size(), __FILE__, __LINE__);
		}
	}
	return 0;
}
