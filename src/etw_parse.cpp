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

void etw_split_comm(const std::string &s, std::string &comm, std::string &pid)
{
	std::string::size_type pos;
	pos = s.find(" (");
	if (pos == std::string::npos) {
		printf("didn't find ' (' in string '%s' in ETW file at %s %d\n", s.c_str(), __FILE__, __LINE__);
		exit(1);
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
}

static std::vector <std::string> WinSAT_SystemConfig;

int add_threads(std::vector <std::string> cols, prf_obj_str &prf_obj, uint32_t t_idx, double ts, int verbose)
{
	uint32_t pids = 0;
	uint32_t pid_col = prf_obj.events[t_idx].etw_col_hsh["Process Name ( PID)"] - 1;
	uint32_t tid_col = prf_obj.events[t_idx].etw_col_hsh["ThreadID"] - 1;
	//printf("T-Start cols: pid= %d, tid= %d\n", pid_col, tid_col);
	std::string comm, pid, tid;
	for (uint32_t i=0; i < prf_obj.etw_evts_set[t_idx].size(); i++) {
		uint32_t data_idx = prf_obj.etw_evts_set[t_idx][i].data_idx;
		uint64_t ts       = prf_obj.etw_evts_set[t_idx][i].ts;
		etw_split_comm(prf_obj.etw_data[data_idx][pid_col], comm, pid);
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

int etw_parse_text(std::string flnm, prf_obj_str &prf_obj, double tm_beg_in, int verbose)
{
	std::ifstream file;
	//long pos = 0;
	std::string line;
	int lines_comments = 0, lines_samples = 0, lines_callstack = 0, lines_null=0;
	int samples = -1, s_idx = -1;

	struct nms_str {
		std::string str;
		std::vector <std::string> cols;
		int count, len, ext_strs;
	};

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
	std::vector <std::string> tkns;
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
	uint32_t thr_st0 = prf_obj.etw_evts_hsh["T-DCStart"];
	uint32_t thr_st1 = prf_obj.etw_evts_hsh["T-Start"];
	double tm_beg2 = dclock();
	while(!file.eof()){
		//read data from file
		std::getline (file, line);
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
			if (evt_idx == UINT32_M1) {
				printf("skipping etw line= '%s' at %s %d\n", line.c_str(), __FILE__, __LINE__);
				continue;
				//exit(1);
			}
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
			if (options.tm_clip_end_valid && tm > options.tm_clip_end) {
				continue;
			}
			if (evt_idx == thr_st0 || evt_idx == thr_st1) {
				add_threads(tkns, prf_obj, evt_idx, (double)es.ts, verbose);
			}
			if (options.tm_clip_beg_valid && tm < options.tm_clip_beg) {
				continue;
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
#if 0
			if (sysconfig_idx != evt_idx) {
				last_ts = es.ts;
			} else {
				es.ts = last_ts;
			}
#endif
			es.ts -= first_ts;
			prf_obj.etw_evts_set[evt_idx].push_back(es);
			prf_obj.etw_data.push_back(tkns);
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
	int csw_sz = (csw_idx >=0 ? (int)prf_obj.etw_evts_set[csw_idx].size() : 0);
	printf("parsed file in %f secs, prf_obj.etw_evts_set.size()= %d csw_sz= %d at %s %d\n",
			tm_end2-tm_beg2, (int)prf_obj.etw_evts_set.size(), csw_sz, __FILE__, __LINE__);
	if (csw_idx < 0) {
		printf("didn't get cswitch event, bye at %s %d\n", __FILE__, __LINE__);
		exit(1);
	}
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
	uint32_t SampledProfile_cols = prf_obj.events[SampledProfile_idx].etw_cols.size();
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
#if 0
	uint32_t pids = 0;
	std::vector <std::string> t_arr = {"T-DCStart", "T-Start"};
	for (uint32_t j=0; j < t_arr.size(); j++) {
		uint32_t t_idx  = prf_obj.etw_evts_hsh[t_arr[j]] -1;
		if (t_idx == UINT32_M1) {
			printf("Can't find '%s' event in ETW trace file %s. Maybe need LOADER? Bye at %s %d\n", t_arr[j].c_str(), flnm.c_str(), __FILE__, __LINE__);
			exit(1);
		}
		uint32_t pid_col = prf_obj.events[t_idx].etw_col_hsh["Process Name ( PID)"] - 1;
		uint32_t tid_col = prf_obj.events[t_idx].etw_col_hsh["ThreadID"] - 1;
		printf("T-Start cols: pid= %d, tid= %d\n", pid_col, tid_col);
		std::string comm, pid, tid;
		for (uint32_t i=0; i < prf_obj.etw_evts_set[t_idx].size(); i++) {
			uint32_t data_idx = prf_obj.etw_evts_set[t_idx][i].data_idx;
			uint64_t ts       = prf_obj.etw_evts_set[t_idx][i].ts;
			etw_split_comm(prf_obj.etw_data[data_idx][pid_col], comm, pid);
			tid = prf_obj.etw_data[data_idx][tid_col];
			uint32_t ipid, itid;
			ipid = atoi(pid.c_str());
			itid = atoi(tid.c_str());
			double tm = 1.0e-6 * (double)ts + 1.0e-7 * (double)start_time;
			prf_add_comm(ipid, itid, comm, prf_obj, tm);
			//hash_comm_pid_tid(comm_pid_tid_hash, comm_pid_tid_vec, comm, ipid, itid);
			if (verbose > 0) 
				printf("tid[%d]: %s %s, comm= '%s', pid= %s, tm= %f\n", pids, prf_obj.etw_data[data_idx][pid_col].c_str(),
					tid.c_str(), comm.c_str(), pid.c_str(), tm);
			pids++;
		}
	}
#endif
	return 0;
	printf("bye at %s %d\n", __FILE__, __LINE__);
	exit(1);
	std::string unknown_mod;
	while(!file.eof()){
		//read data from file
		std::getline (file, line);
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
			samples++;
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
			int mtch=-1;
			for (int i=i_beg; i < sz; i++) {
				if (line.find(prf_obj.samples[i].tm_str) != std::string::npos) {
					s_idx = i;
					store_callstack_idx = i;
					mtch = i;
					if (extra_str.size() > 0) {
						prf_obj.samples[i].args.push_back(extra_str);
					}
					prf_obj.samples[i].evt_idx = evt;
					//printf("ck tm[%d]= %s, line= %s\n", i, prf_obj.samples[i].tm_str.c_str(), line.c_str());
					break;
				}
			}
			if (mtch == -1) {
				std::string tm;
				if (s_idx >= 0 && s_idx < (int)prf_obj.samples.size()) {
					tm = prf_obj.samples[s_idx].tm_str;
				}
				printf("missed samples %d, s_idx= %d, i_beg= %d, tm[%d]= '%s', line= %s\n",
					samples, s_idx, i_beg, s_idx, tm.c_str(), line.c_str());
				exit(1);
			}
			if (s_idx == -1) {
				s_idx = -2;
			}
		}
	}
	file.close();
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
