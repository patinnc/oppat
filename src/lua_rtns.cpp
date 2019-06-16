/* Copyright (c) 2018 Patrick Fay
 *
 * License http://opensource.org/licenses/mit-license.php MIT License
 */

#include <sol.hpp>
#include <string>
#include <iostream>
#include <unordered_map>
#include <vector>
#include <cstddef>
#include <stdio.h>
#include <string.h>
#include "perf_event_subset.h"
#include "rd_json2.h"
#include "rd_json.h"
#include "printf.h"
#include "oppat.h"
#include "utils2.h"
#include "utils.h"
#define LUA_RTNS_CPP
#include "lua_rtns.h"

struct mylua_str {
	std::string filename;
	sol::state lua;
	sol::load_result loaded_chunk;
	sol::protected_function script_func;
};

#if 1
static int lua_push_sample(prf_obj_str &prf_obj, int verbose, int evt_idx, double ts)
{
	prf_samples_str pss;
	//pss.comm   = comm;
	//pss.event  = evt_nm;
	pss.evt_idx= evt_idx;
	prf_obj.events[evt_idx].evt_count++;
	pss.pid    = -1;
	pss.tid    = -1;
	pss.cpu    = 0;
	pss.ts     = ts;
	pss.period = 0;
	//if (verbose > 0)
	//	printf("hdr:[%d] %-16.16s %d/%d [%.3d]%s %s:\n",
	//	(int)prf_obj.samples.size(), pss.comm.c_str(), pss.pid, pss.pid, pss.cpu, pss.tm_str.c_str(), evt_nm.c_str());
	prf_obj.samples.push_back(pss);
	return 0;
}

static int lua_add_event(std::string evt_str, std::string area, prf_obj_str &prf_obj)
{
	prf_events_str pes;

	int evt_id = (int)prf_obj.events.size();
	pes.event_name = evt_str;
	pes.pea.type = 1000; // ummm... needs a number (like PERF_TYPE_TRACEPOINT) that doesn't conflict  with anything
	pes.pea.config = evt_id;
	if (area.size()) {
		pes.event_area = area;
	}
	pes.event_name = evt_str;
	//printf("slen= %d, tc event name= '%s', ID= %d, num_evts= %d\n", slen, evt_str, evt_id, (int)prf_obj.events.size());
	prf_add_ids(evt_id, prf_obj.events.size(), prf_obj);
	prf_obj.events.push_back(pes);
	return evt_id;
}

#endif
int lua_read_data(std::string data_filename, std::string data2_filename, std::string wait_filename,
		prf_obj_str &prf_obj, std::string lua_file_in, std::string lua_rtn_in, int verbose)
{
	std::cout << "=== opening a state ===" << std::endl;
	int lua_idx = 0;

	std::vector <mylua_str> lua_st;
	lua_st.resize(lua_idx+1);
	// open some common libraries
	//lua_st[lua_idx].lua.open_libraries(sol::lib::base, sol::lib::package);
	lua_st[lua_idx].lua.open_libraries();

	std::cout << std::endl;


/* OPTION 3 */
	// error handling based on https://github.com/ThePhD/sol2/issues/386
	// This is a lower-level, more explicit way to load code
	// This explicitly loads the code, giving you access to any errors
	// plus the load status
	// then, it turns the loaded code into a sol::protected_function
	// which is then called so that the code can run
	// you can then check that too, for any errors
	// The two previous approaches are recommended
	std::string ds = std::string(DIR_SEP);
	std::string root_dir = std::string(get_root_dir_of_exe());
	std::string lua_file = "test_01.lua";
	std::string lua_rtn  = "do_tst";
	if (lua_file_in.size() > 0) {
		lua_file = lua_file_in;
	}
	if (lua_rtn_in.size() > 0) {
		lua_rtn = lua_rtn_in;
	}
	std::string base_filename = "src_lua"+ ds + lua_file;
	std::string filename = root_dir + ds + base_filename;
	printf("try lua file %s at %s %d\n", filename.c_str(), __FILE__, __LINE__);
	int rc = ck_filename_exists(filename.c_str(), __FILE__, __LINE__, 0);
	if (rc != 0) {
		std::string f2 = root_dir + ds + ".." + ds + base_filename;
		rc = ck_filename_exists(f2.c_str(), __FILE__, __LINE__, 0);
		if (rc != 0) {
			printf("failed to find lua file with base file name= %s\nlooked for %s and %s at %s %d\n",
					base_filename.c_str(), filename.c_str(), f2.c_str(), __FILE__, __LINE__);
			exit(1);
		}
		filename = f2;
		printf("found lua file %s at %s %d\n", filename.c_str(), __FILE__, __LINE__);
	}
	lua_file = filename;
	lua_st[lua_idx].loaded_chunk = lua_st[lua_idx].lua.load_file(lua_file);
	//c_assert(!loaded_chunk.valid());
	if (!lua_st[lua_idx].loaded_chunk.valid()) {
		sol::error err = lua_st[lua_idx].loaded_chunk;
		sol::load_status status = lua_st[lua_idx].loaded_chunk.status();
		std::cout << "Something went horribly wrong loading the code: " << sol::to_string(status) << " error" << "\n\t" << err.what() << std::endl;
		printf("error on check of lua file '%s'. bye at %s %d\n", lua_file.c_str(), __FILE__, __LINE__);
		exit(1);
	}
	lua_st[lua_idx].loaded_chunk();
	lua_st[lua_idx].script_func = lua_st[lua_idx].lua[lua_rtn];
	if (!lua_st[lua_idx].script_func) {
		printf("didn't find lua function named '%s' in lua file %s. Bye at %s %d\n",
			lua_rtn.c_str(), lua_file.c_str(), __FILE__, __LINE__);
		exit(1);
	}
	//sol::protected_function problematic_woof = lua_st[lua_idx].lua["do_tst"];
	lua_st[lua_idx].script_func.error_handler = lua_st[lua_idx].lua["got_problems"];
	//problematic_woof.error_handler = lua_st[lua_idx].lua["got_problems"];
	sol::protected_function_result result = lua_st[lua_idx].script_func(data_filename, data2_filename, wait_filename, verbose);
	if (!result.valid()) {
		sol::error err = result;
		sol::call_status status = result.status();
		std::cout << "Something went horribly wrong running the code: " << sol::to_string(status) << " error" << "\n\t" << err.what() << std::endl;
		printf("error running lua file '%s'. bye at %s %d\n", lua_file.c_str(), __FILE__, __LINE__);
		exit(1);
	}
	int events = lua_st[lua_idx].lua["data_shape"]["events"];
	std::vector <std::string> event_nms;
	for (int ev=1; ev <= events; ev++) {
		std::string evt_nm, evt_area;
		evt_nm   = lua_st[lua_idx].lua["data_shape"]["event_name"][ev];
		evt_area = lua_st[lua_idx].lua["data_shape"]["event_area"];
		int evt_idx = lua_add_event(evt_nm, evt_area, prf_obj);
		event_nms.push_back(evt_nm);
		printf("got lua event names[%d]= %s at %s %d\n", ev-1, evt_nm.c_str(), __FILE__, __LINE__);
	}
	int rows = lua_st[lua_idx].lua["data_shape"]["rows"];
	int cols = lua_st[lua_idx].lua["data_shape"]["cols"];
	if (verbose) {
		printf("lua data_shape rows= %d cols= %d at %s %d\n", rows, cols, __FILE__, __LINE__);
	}
	//std::vector <int> col_typ;
	std::vector <std::vector <int>> col_typ;
	std::vector <std::vector <std::string>> col_names;
	std::string str;
	std::vector <int> ts_idx;
	std::vector <int> dura_idx;
	std::vector <int> evt_nm_idx;
	ts_idx.resize(events, -1);
	dura_idx.resize(events, -1);
	evt_nm_idx.resize(events, -1);
	for (int ev=1; ev <= events; ev++) {
		std::vector <std::string> cols_vec;
		std::vector <int> cols_typ;
		for (int j=0; j < cols; j++) {
			str = lua_st[lua_idx].lua["data_shape"]["col_names"][ev][j+1];
			cols_vec.push_back(str);
			if (str == "_TIMESTAMP_") {
				if (ts_idx[ev-1] != -1) {
					printf("lua field has _TIMESTAMP_ more than once. Bye at %s %d\n", __FILE__, __LINE__);
					exit(1);
				}
				ts_idx[ev-1] = j;
			} else if (str == "_DURATION_") {
				if (dura_idx[ev-1] != -1) {
					printf("lua field has _DURATION_ more than once. Bye at %s %d\n", __FILE__, __LINE__);
					exit(1);
				}
				dura_idx[ev-1] = j;
			} else if (str == "event") {
				evt_nm_idx[ev-1] = j;
			}
			if (verbose)
				printf("lua data_shape[\"col_name\"][%d][%d]= '%s' at %s %d\n", ev, j+1, str.c_str(), __FILE__, __LINE__);
			str = lua_st[lua_idx].lua["data_shape"]["col_types"][ev][j+1];
			if (str == "str") {
				cols_typ.push_back((int)fte_enum::FLD_TYP_STR);
			} else if (str == "dbl") {
				cols_typ.push_back((int)fte_enum::FLD_TYP_DBL);
			} else if (str == "int") {
				cols_typ.push_back((int)fte_enum::FLD_TYP_INT);
			} else {
				printf("invalid col_type[%d]= '%s' at %s %d\n", j, str.c_str(), __FILE__, __LINE__);
				exit(1);
			}
			if (verbose)
				printf("lua data_shape[\"col_types\"][%d][%d]= '%s' at %s %d\n", ev, j+1, str.c_str(), __FILE__, __LINE__);
		}
		col_names.push_back(cols_vec);
		col_typ.push_back(cols_typ);
		if (evt_nm_idx[ev-1] == -1) {
			printf("missed a column named 'event' in the lua data. This col IDs the event name. Must have it. Bye at %s %d\n", __FILE__, __LINE__);
			exit(1);
		}
	}

	prf_obj.features_nr_cpus_online = 1; // the power stats are system wide (I think... not sure how multi socket shows up)
	prf_obj.features_nr_cpus_available = 1;
	prf_obj.lua_data.events = event_nms;
	prf_obj.lua_data.col_names = col_names;
	prf_obj.lua_data.col_typ = col_typ;
	prf_obj.lua_data.timestamp_idx = ts_idx;
	prf_obj.lua_data.duration_idx = dura_idx;
	if (verbose)
		printf("begin lua data_table rows= %d, cols= %d\n", rows, cols);
	bool need_ts0 = true, need_tm_end = true;
	prf_obj.tm_beg = 0.0;
	prf_obj.tm_end = 0.0;

	//verbose = 1; // set to 1 to enable printing the lua data_table
	lua_data_str lds;
	int read_rows = 0;
	for (int i=1; i <= rows; i++) {
		std::vector <lua_data_val_str> ldvs_vec;
		read_rows++;
		uint32_t evt_idx = -1;
		for (uint32_t k=0; k < event_nms.size(); k++) {
			for (uint32_t j=0; j < col_names[k].size(); j++) {
				std::string col = col_names[k][j];
				if (col == "event") {
					if (event_nms[k] == lua_st[lua_idx].lua["data_table"][i][col]) {
						evt_idx = k;
						break;
					}
				}
			}
			if (evt_idx != (uint32_t)-1) {
				break;
			}
		}
		if (evt_idx == (uint32_t)-1) {
			printf("missed event lookup row= %d for field at %s %d\n", i, __FILE__, __LINE__);
			for (uint32_t k=0; k < event_nms.size(); k++) {
				std::string nm = event_nms[k];
				if (verbose) {
					printf("got event_nms[%d]= %s at %s %d\n", k, nm.c_str(), __FILE__, __LINE__);
					fflush(NULL);
				}
				for (uint32_t j=0; j < col_names[k].size(); j++) {
					std::string col = col_names[k][j];
					if (verbose) {
						printf("got col_names[%d][%d]= %s at %s %d\n", k, j, col.c_str(), __FILE__, __LINE__);
						fflush(NULL);
					}
					if (col == "event") {
						std::string nm2 = lua_st[lua_idx].lua["data_table"][i][col];
						printf("got nm2= %s at %s %d\n", nm2.c_str(), __FILE__, __LINE__);
						//fflush(NULL);
					}
					/*
					if (col == "event") {
						std::string ck = lua_st[lua_idx].lua["data_table"][i][col];
						//printf("got event_nms[%d]= %s cmp lua_st[%d].lua['data_table'][%d][%s]= %s at %s %d\n", 
						//	k, event_nms[k].c_str(), lua_idx, i, col.c_str(), ck.c_str(), __FILE__, __LINE__);
						printf("got event_nms= %s cmp ck= %s at %s %d\n", 
							event_nms[k].c_str(), ck.c_str(), __FILE__, __LINE__);
						fflush(NULL);
					}
					*/
				}
			}
			exit(1);
		}
		double dura = 0.0;
		for (uint32_t j=0; j < col_names[evt_idx].size(); j++) {
			lua_data_val_str ldvs;
			std::string col = col_names[evt_idx][j];
			if (col_typ[evt_idx][j] & (int)fte_enum::FLD_TYP_STR) {
				std::string str = lua_st[lua_idx].lua["data_table"][i][col];
				if (verbose)
					printf(", %s=%s, ", col.c_str(), str.c_str());
				ldvs.str = str;
			} else if (col_typ[evt_idx][j] & (int)fte_enum::FLD_TYP_DBL) {
				double x = lua_st[lua_idx].lua["data_table"][i][col];
				if (verbose)
					printf(", %s=%.9f, ", col.c_str(), x);
				ldvs.dval = x;
			} else if (col_typ[evt_idx][j] & (int)fte_enum::FLD_TYP_INT) {
				double x = lua_st[lua_idx].lua["data_table"][i][col];
				if (verbose)
					printf(", %s=%.0f, ", col.c_str(), x);
				ldvs.dval = x;
				ldvs.ival = x;
			}
			ldvs_vec.push_back(ldvs);
		}
		if (verbose)
			printf("\n");
		double tsn = ldvs_vec[ts_idx[evt_idx]].dval;
#if 1
		if ((options.tm_clip_beg_valid == CLIP_LVL_1 && tsn < options.tm_clip_beg) ||
			(options.tm_clip_end_valid == CLIP_LVL_1 && (tsn-dura) > options.tm_clip_end)) {
			continue;
		}
#endif
		dura = ldvs_vec[dura_idx[evt_idx]].dval;
		double ts_beg = ldvs_vec[ts_idx[evt_idx]].dval - dura;
		if (need_ts0 || ts_beg < prf_obj.tm_beg) {
			prf_obj.tm_beg = ts_beg;
			printf("LUA prf_obj.tm_beg= %f at %s %d\n", prf_obj.tm_beg, __FILE__, __LINE__);
			need_ts0 = false;
		}
		if (need_tm_end) {
			prf_obj.tm_end = ldvs_vec[ts_idx[evt_idx]].dval;
		}
		if (prf_obj.tm_end < ldvs_vec[ts_idx[evt_idx]].dval) {
			prf_obj.tm_end = ldvs_vec[ts_idx[evt_idx]].dval;
		}
		prf_obj.lua_data.data_rows.push_back(ldvs_vec);
		lua_push_sample(prf_obj, verbose, evt_idx, 1e9*(ldvs_vec[ts_idx[evt_idx]].dval - dura));
	}
	printf("end lua data_table rows= %d, cols= %d read_rows= %d, prf_obj.tm_end= %f\n", rows, cols, read_rows, prf_obj.tm_end);

	//std::sort(prf_obj.samples.begin(), prf_obj.samples.end(), compareByTime);
	std::cout << std::endl;


	//printf("bye at %s %d\n", __FILE__, __LINE__);
	//exit(1);
	return 0;
}

static std::unordered_map<std::string, uint32_t> lua_state_hash;
static std::vector <std::string> lua_state_vec;
static std::vector <mylua_str *> lua_states;

double lua_derived_evt(std::string lua_file, std::string lua_rtn, std::string &evt_nm,
		std::vector <std::string> &data_cols, std::vector <std::string> &data_vals,
		std::vector <std::string> &new_cols, std::vector <std::string> &new_vals, int verbose)
{
	int lua_idx = (int)hash_string(lua_state_hash, lua_state_vec, lua_file) - 1;
	if (lua_idx >= lua_states.size()) {
		printf("create lua state for lua_idx= %d lua_file= %s at %s %d\n", lua_idx, lua_file.c_str(), __FILE__, __LINE__);
		mylua_str* mls;
		mls = new mylua_str;
		lua_states.push_back(mls);
		//lua_states.resize(lua_idx+1);
		// open some common libraries
		lua_states[lua_idx]->lua.open_libraries();

		// error handling based on https://github.com/ThePhD/sol2/issues/386
		// This is a lower-level, more explicit way to load code
		// This explicitly loads the code, giving you access to any errors
		// plus the load status
		// then, it turns the loaded code into a sol::protected_function
		// which is then called so that the code can run
		// you can then check that too, for any errors
		// The two previous approaches are recommended
		std::string ds = std::string(DIR_SEP);
		std::string root_dir = std::string(get_root_dir_of_exe());
		std::string base_filename = "src_lua"+ ds + lua_file;
		std::string filename = root_dir + ds + base_filename;
		printf("try lua file %s at %s %d\n", filename.c_str(), __FILE__, __LINE__);
		int rc = ck_filename_exists(filename.c_str(), __FILE__, __LINE__, 0);
		if (rc != 0) {
			std::string f2 = root_dir + ds + ".." + ds + base_filename;
			rc = ck_filename_exists(f2.c_str(), __FILE__, __LINE__, 0);
			if (rc != 0) {
				printf("failed to find lua file with base file name= %s\nlooked for %s and %s at %s %d\n",
						base_filename.c_str(), filename.c_str(), f2.c_str(), __FILE__, __LINE__);
				exit(1);
			}
			filename = f2;
			printf("found lua file %s at %s %d\n", filename.c_str(), __FILE__, __LINE__);
		}
		lua_states[lua_idx]->loaded_chunk = lua_states[lua_idx]->lua.load_file(filename);
		//c_assert(!loaded_chunk.valid());
		if (!lua_states[lua_idx]->loaded_chunk.valid()) {
			sol::error err = lua_states[lua_idx]->loaded_chunk;
			sol::load_status status = lua_states[lua_idx]->loaded_chunk.status();
			std::cout << "Something went horribly wrong loading the code: " << sol::to_string(status) << " error" << "\n\t" << err.what() << std::endl;
			printf("error on check of lua file '%s'. bye at %s %d\n", lua_file.c_str(), __FILE__, __LINE__);
			exit(1);
		}
		lua_states[lua_idx]->loaded_chunk();
		std::string err_hndlr = "got_problems";
		lua_states[lua_idx]->script_func.error_handler = lua_states[lua_idx]->lua[err_hndlr];
		if (!lua_states[lua_idx]->script_func.error_handler) {
			printf("didn't find LUA script named '%s' in file %s. Bye at %s %d\n",
				err_hndlr.c_str(), lua_file.c_str(), __FILE__, __LINE__);
			exit(1);
		}
		lua_states[lua_idx]->lua["data_cols"] = lua_states[lua_idx]->lua.create_table_with("1", "1");
		lua_states[lua_idx]->lua["data_vals"] = lua_states[lua_idx]->lua.create_table_with("1", "1");
		lua_states[lua_idx]->lua["new_cols"] = lua_states[lua_idx]->lua.create_table_with("1", "1");
		lua_states[lua_idx]->lua["new_vals"] = lua_states[lua_idx]->lua.create_table_with("1", "1");
		//lua["some_table"] = lua.create_table_with("value", 24);
	}
	lua_states[lua_idx]->script_func = lua_states[lua_idx]->lua[lua_rtn];
	if (!lua_states[lua_idx]->script_func) {
		printf("didn't find LUA script named '%s' in file %s. Bye at %s %d\n",
			lua_rtn.c_str(), lua_file.c_str(), __FILE__, __LINE__);
		exit(1);
	}


	// load the input data and headers
	for (uint32_t i=0; i < data_cols.size(); i++) {
		lua_states[lua_idx]->lua["data_cols"][i+1] = data_cols[i];
	}
	for (uint32_t i=0; i < new_cols.size(); i++) {
		lua_states[lua_idx]->lua["new_cols"][i+1] = new_cols[i];
		if (verbose > 0) {
			printf("new_cols[%d]= %s at %s %d\n", i+1, new_cols[i].c_str(), __FILE__, __LINE__);
		}
	}
	for (uint32_t i=0; i < data_vals.size(); i++) {
		lua_states[lua_idx]->lua["data_vals"][i+1] = data_vals[i];
	}
	sol::protected_function_result result = lua_states[lua_idx]->script_func(verbose);
	if (!result.valid()) {
		sol::error err = result;
		sol::call_status status = result.status();
		std::cout << "Something went horribly wrong running the code: " << sol::to_string(status) << " error" << "\n\t" << err.what() << std::endl;
		printf("error running lua file '%s'. bye at %s %d\n", lua_file.c_str(), __FILE__, __LINE__);
		exit(1);
	}
	if (new_cols.size() != new_vals.size()) {
		fprintf(stderr, "expected new_cols.sz= %d to be == new_vals.sz= %d. Bye at %s %d\n",
				(int)new_cols.size(), (int)new_vals.size(), __FILE__, __LINE__);
		exit(1);
	}
	int rc = 0;
	for (uint32_t i=0; i < new_cols.size(); i++) {
		new_vals[i] = lua_states[lua_idx]->lua["new_vals"][i+1];
		if (new_cols[i] == "__EMIT__" && new_vals[i] == "1") {
			rc = 1;
		}
		if (verbose > 0) {
			printf("lua new_val[%d]= %s at %s %d\n", i, new_vals[i].c_str(), __FILE__, __LINE__);
		}
	}
	return rc;
}

double lua_derived_tc_prf(uint32_t der_evt_idx, std::string lua_file, std::string lua_rtn, std::string &evt_nm,
		struct prf_samples_str &samples,
		std::vector <std::string> &new_cols, std::vector <std::string> &new_vals,
		uint32_t &emit_var, std::string evt_tag, int verbose)
{
	int lua_idx = (int)hash_string(lua_state_hash, lua_state_vec, lua_file) - 1;
	static uint32_t emit_idx = UINT32_M1;
	if (lua_idx >= lua_states.size()) {
		printf("create lua state for lua_idx= %d lua_file= %s at %s %d\n", lua_idx, lua_file.c_str(), __FILE__, __LINE__);
		mylua_str* mls;
		mls = new mylua_str;
		lua_states.push_back(mls);
		//lua_states.resize(lua_idx+1);
		// open some common libraries
		lua_states[lua_idx]->lua.open_libraries();

		// error handling based on https://github.com/ThePhD/sol2/issues/386
		// This is a lower-level, more explicit way to load code
		// This explicitly loads the code, giving you access to any errors
		// plus the load status
		// then, it turns the loaded code into a sol::protected_function
		// which is then called so that the code can run
		// you can then check that too, for any errors
		// The two previous approaches are recommended
		std::string ds = std::string(DIR_SEP);
		std::string root_dir = std::string(get_root_dir_of_exe());
		std::string base_filename = "src_lua"+ ds + lua_file;
		std::string filename = root_dir + ds + base_filename;
		printf("try lua file %s at %s %d\n", filename.c_str(), __FILE__, __LINE__);
		int rc = ck_filename_exists(filename.c_str(), __FILE__, __LINE__, 0);
		if (rc != 0) {
			std::string f2 = root_dir + ds + ".." + ds + base_filename;
			rc = ck_filename_exists(f2.c_str(), __FILE__, __LINE__, 0);
			if (rc != 0) {
				printf("failed to find lua file with base file name= %s\nlooked for %s and %s at %s %d\n",
						base_filename.c_str(), filename.c_str(), f2.c_str(), __FILE__, __LINE__);
				exit(1);
			}
			filename = f2;
			printf("found lua file %s at %s %d\n", filename.c_str(), __FILE__, __LINE__);
		}
		lua_states[lua_idx]->loaded_chunk = lua_states[lua_idx]->lua.load_file(filename);
		if (!lua_states[lua_idx]->loaded_chunk.valid()) {
			sol::error err = lua_states[lua_idx]->loaded_chunk;
			sol::load_status status = lua_states[lua_idx]->loaded_chunk.status();
			std::cout << "Something went horribly wrong loading the code: " << sol::to_string(status) << " error" << "\n\t" << err.what() << std::endl;
			printf("error on check of lua file '%s'. bye at %s %d\n", lua_file.c_str(), __FILE__, __LINE__);
			exit(1);
		}
		lua_states[lua_idx]->loaded_chunk();
		std::string err_hndlr = "got_problems";
		lua_states[lua_idx]->script_func.error_handler = lua_states[lua_idx]->lua[err_hndlr];
		if (!lua_states[lua_idx]->script_func.error_handler) {
			printf("didn't find LUA script named '%s' in file %s. Bye at %s %d\n",
				err_hndlr.c_str(), lua_file.c_str(), __FILE__, __LINE__);
			exit(1);
		}
		lua_states[lua_idx]->lua["data_cols"] = lua_states[lua_idx]->lua.create_table_with("1", "1");
		lua_states[lua_idx]->lua["data_vals"] = lua_states[lua_idx]->lua.create_table_with("1", "1");
		lua_states[lua_idx]->lua["new_cols"] = lua_states[lua_idx]->lua.create_table_with("1", "1");
		lua_states[lua_idx]->lua["new_vals"] = lua_states[lua_idx]->lua.create_table_with("1", "1");
		//sol::table my_table(my_lua_state, sol::create);
	}
#if 0
	auto ck_it3 = lua_states[lua_idx]->lua["data_cols"];
	if (ck_it3.valid()) {
		printf("lua data_cols table exists at %s %d\n", __FILE__, __LINE__);
	}
#endif
	auto ck_it = lua_states[lua_idx]->lua["data_cols"][der_evt_idx];
	if (!ck_it.valid()) {
		printf("lua creating data_cols[%d] at %s %d\n", der_evt_idx, __FILE__, __LINE__);
		lua_states[lua_idx]->lua["data_cols"][der_evt_idx] = lua_states[lua_idx]->lua.create_table_with("1", "1");
		lua_states[lua_idx]->lua["data_vals"][der_evt_idx] = lua_states[lua_idx]->lua.create_table_with("1", "1");
		lua_states[lua_idx]->lua["new_cols"][der_evt_idx] = lua_states[lua_idx]->lua.create_table_with("1", "1");
		lua_states[lua_idx]->lua["new_vals"][der_evt_idx] = lua_states[lua_idx]->lua.create_table_with("1", "1");
#if 0
		auto ck_it2 = lua_states[lua_idx]->lua["data_cols"][der_evt_idx];
		if (ck_it2.valid()) {
			printf("lua created data_cols[%d] at %s %d\n", der_evt_idx, __FILE__, __LINE__);
		}
#endif
		int i = 0;
		// data_cols are fixed table. The table data_cols[1] = 'event'
		lua_states[lua_idx]->lua["data_cols"][der_evt_idx][++i] = "event";
		lua_states[lua_idx]->lua["data_cols"][der_evt_idx][++i] = "ts";
		lua_states[lua_idx]->lua["data_cols"][der_evt_idx][++i] = "extra_str";
		lua_states[lua_idx]->lua["data_cols"][der_evt_idx][++i] = "comm";
		lua_states[lua_idx]->lua["data_cols"][der_evt_idx][++i] = "pid";
		lua_states[lua_idx]->lua["data_cols"][der_evt_idx][++i] = "tid";
		lua_states[lua_idx]->lua["data_cols"][der_evt_idx][++i] = "cpu";
		lua_states[lua_idx]->lua["data_cols"][der_evt_idx][++i] = "period";
		lua_states[lua_idx]->lua["data_cols"][der_evt_idx][++i] = "evt_tag";
	}
	for (int i=0; i < (int)new_cols.size(); i++) {
		lua_states[lua_idx]->lua["new_cols"][der_evt_idx][i+1] = new_cols[i];
		if (new_cols[i] == "__EMIT__") {
			emit_idx = i;
		}
	}
	lua_states[lua_idx]->script_func = lua_states[lua_idx]->lua[lua_rtn];
	if (!lua_states[lua_idx]->script_func) {
		printf("didn't find LUA script named '%s' in file %s. Bye at %s %d\n",
			lua_rtn.c_str(), lua_file.c_str(), __FILE__, __LINE__);
		exit(1);
	}
	std::string comm, event, tm_str;
	uint32_t evt_idx, pid, tid, cpu;
	uint64_t ts, period;
	std::vector <std::string> args;

	int i = 0;
	// the data_vals table indices number must agree with data_cols indices.
	lua_states[lua_idx]->lua["data_vals"][der_evt_idx][++i] = samples.event;
	lua_states[lua_idx]->lua["data_vals"][der_evt_idx][++i] = samples.ts;
	lua_states[lua_idx]->lua["data_vals"][der_evt_idx][++i] = samples.extra_str;
	lua_states[lua_idx]->lua["data_vals"][der_evt_idx][++i] = samples.comm;
	lua_states[lua_idx]->lua["data_vals"][der_evt_idx][++i] = samples.pid;
	lua_states[lua_idx]->lua["data_vals"][der_evt_idx][++i] = samples.tid;
	lua_states[lua_idx]->lua["data_vals"][der_evt_idx][++i] = samples.cpu;
	lua_states[lua_idx]->lua["data_vals"][der_evt_idx][++i] = samples.period;
	lua_states[lua_idx]->lua["data_vals"][der_evt_idx][++i] = evt_tag;

	//printf("lua extra_str= %s at %s %d\n", samples.extra_str.c_str(), __FILE__, __LINE__);

	// load the input data and headers
	sol::protected_function_result result = lua_states[lua_idx]->script_func(verbose, der_evt_idx);
	if (!result.valid()) {
		sol::error err = result;
		sol::call_status status = result.status();
		std::cout << "Something went horribly wrong running the code: " << sol::to_string(status) << " error" << "\n\t" << err.what() << std::endl;
		printf("error running lua file '%s'. bye at %s %d\n", lua_file.c_str(), __FILE__, __LINE__);
		exit(1);
	}
	if (new_cols.size() != new_vals.size()) {
		fprintf(stderr, "expected new_cols.sz= %d to be == new_vals.sz= %d. Bye at %s %d\n",
				(int)new_cols.size(), (int)new_vals.size(), __FILE__, __LINE__);
		exit(1);
	}
	emit_var = 0;
	for (uint32_t i=0; i < new_cols.size(); i++) {
		new_vals[i] = lua_states[lua_idx]->lua["new_vals"][der_evt_idx][i+1];
		//printf("new_vals[%d]= %s , hdr= %s at %s %d\n", i, new_vals[i].c_str(), new_cols[i].c_str(), __FILE__, __LINE__);
		if (verbose > 0) {
			printf("lua new_val[%d]= %s at %s %d\n", i, new_vals[i].c_str(), __FILE__, __LINE__);
		}
		if (i == emit_idx && new_vals[i] == "1") {
			emit_var = 1;
		}
	}
	return 0;
}
