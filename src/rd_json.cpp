/* Copyright (c) 2018 Patrick Fay
 *
 * License http://opensource.org/licenses/mit-license.php MIT License
 */


#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <cstddef>
#include <string>

#include "utils2.h"
#include "utils.h"

// from https://github.com/nlohmann/json
#include "json.hpp"

#define RD_JSON_CPP
#include "rd_json2.h"
#include "rd_json.h"

	

static std::vector <fld_typ_str> fld_typ_strs;

uint32_t hash_string(std::unordered_map<std::string, uint32_t> &hsh_str, std::vector <std::string> &vec_str, std::string str)
{
	uint32_t idx = hsh_str[str];
	if (idx == 0) {
		vec_str.push_back(str);
		idx = hsh_str[str] = (uint32_t)vec_str.size(); // so we are storing vec_str idx+1
	}
	return idx;
}
// for convenience
using json = nlohmann::json;


static void prt_line(int sz)
{
	int skip_spcs=0;
	for (int i=0; i <= sz; i++) {
		if ((i%10) == 0) {
			int num = i/10;
			printf("%d", i/10);
			if (num < 10) {
				skip_spcs = 1;
			} else if (num < 100) {
				skip_spcs = 2;
			} else if (num < 1000) {
				skip_spcs = 3;
			} else if (num < 10000) {
				skip_spcs = 4;
			}
		} 
		if (skip_spcs >= 1) {
			skip_spcs--;
		} else {
			printf(" ");
		}
	}
	printf("\n");
	for (int i=0; i <= sz; i++) {
		int num = i%10;
		printf("%d", num);
	}
	printf("\n");
}

static uint32_t ck_lkup_typ(const char *str, int verbose)
{
	uint32_t sz = fld_typ_strs.size();
	uint32_t flag = 0;
	for (uint32_t i=0; i < sz; i++) {
		if (strcmp(str, fld_typ_strs[i].str.c_str()) == 0) {
			//flag = (1 << i); // assumes that fld_typ_strs and fld_typ_enums are in the same order
			flag = fld_typ_strs[i].flag; // doesn't assume that fld_typ_strs and fld_typ_enums are in the same order
			//uint32_t tflg = fld_typ_enums[i];
			if (verbose)
				printf("fld[%d]: 0x%x str= %s\n", i, flag, str);
			break;
		}
	}
	if (!flag) {
		printf("error: got lkup_typ= '%s', not found in valid list. Valid list is:\n", str);
		for (uint32_t j=0; j < sz; j++) {
			printf("'%s'\n", fld_typ_strs[j].str.c_str());
		}
		exit(1);
	}
	return flag;
}

static std::vector<std::string> get_lkup_typ(std::string s, uint64_t &lkup_typ_flags, int verbose)
{
	std::vector<std::string> array;
	std::size_t pos = 0, found;
	while((found = s.find_first_of('|', pos)) != std::string::npos) {
		array.push_back(s.substr(pos, found - pos));
		pos = found+1;
	}
	array.push_back(s.substr(pos));
	uint64_t flags = 0;
	for (uint32_t i=0; i < array.size(); i++) {
		if (verbose)
			printf("typ[%d]= %s\n", i, array[i].c_str());
		flags |= ck_lkup_typ(array[i].c_str(), verbose);
	}
	lkup_typ_flags = flags;
	return array;
}

static bool is_action_oper_valid(std::string oper)
{
	if (oper == "drop_if_gt" || oper == "cap" || oper == "set" || oper == "mult" ||
		oper == "diff_ts_last_by_val" ||
		oper == "div" || oper == "add" || oper == "replace" || oper == "replace_any") {
		return true;
	}
	return false;
}

static int32_t ck_pixels_high(std::string json_file, std::string chart, std::string fld, int32_t pxls_high, int line)
{
	int32_t pixels_high_min = 100;
	int32_t pixels_high_max = 1500;
	if (pxls_high < pixels_high_min || pxls_high > pixels_high_max) {
		printf("for chart: '%s' found field \"%s\":%d in file %s. Value has to be >= %d and <= %d. called from line= %d. Bye at %s %d\n",
			chart.c_str(), fld.c_str(), pxls_high, json_file.c_str(), pixels_high_min, pixels_high_max, line, __FILE__, __LINE__);
		exit(1);
	}
	return pxls_high;
}

int ck_json(std::string str, std::string from_where, const char *file, int line, int verbose)
{
	uint32_t sz = (int)str.size();
	bool use_charts_default = true;
	bool worked=false;

	json j;
	try {
		j = json::parse(str);
		worked=true;
	} catch (json::parse_error& e) {
			// output exception information
			std::cout << "message: " << e.what() << '\n'
				  << "exception id: " << e.id << '\n'
				  << "byte position of error: " << e.byte << std::endl;
	}
	if (!worked) {
		printf("parse of json str %s failed called by %s %d. bye at %s %d\n",
				from_where.c_str(), file, line, __FILE__, __LINE__);
		fprintf(stderr, "parse of json str %s failed called by %s %d. bye at %s %d\n",
				from_where.c_str(), file, line, __FILE__, __LINE__);
		printf("Here is the string we tried to parse at %s %d:\n%s\n", __FILE__, __LINE__, str.c_str());
		prt_line(sz);

		exit(1);
	}
	return 0;
}

int do_json_evt_chrts_defaults(std::string json_file, std::string str, int verbose)
{
	if (verbose > 0)
		std::cout << str << std::endl;
	uint32_t sz = (int)str.size();
	bool use_charts_default = true;
	bool worked=false;

	json j;
	try {
		j = json::parse(str);
		worked=true;
	} catch (json::parse_error& e) {
			// output exception information
			std::cout << "message: " << e.what() << '\n'
				  << "exception id: " << e.id << '\n'
				  << "byte position of error: " << e.byte << std::endl;
	}
	if (!worked) {
		printf("parse of json data file= %s failed. bye at %s %d\n", json_file.c_str(), __FILE__, __LINE__);
		printf("Here is the string we tried to parse at %s %d:\n%s\n", __FILE__, __LINE__, str.c_str());
		prt_line(sz);

		exit(1);
	}
	sz = j["ETW_events_to_skip"].size();
	for (uint32_t i=0; i < sz; i++) {
		try {
			std::string str = j["ETW_events_to_skip"][i];
			hash_string(ETW_events_to_skip_hash, ETW_events_to_skip_vec, str);
			if (verbose > 0) {
				printf("ETW_events_to_skip[%d]= '%s' hsh= %d, at %s %d\n",
						i, str.c_str(), ETW_events_to_skip_hash[str], __FILE__, __LINE__);
			}
		} catch (...) { }
	}
	sz = j["chart_category_priority"].size();
	for (uint32_t i=0; i < sz; i++) {
		try {
			chart_cat_str ch_cat;
			ch_cat.name     = j["chart_category_priority"][i]["name"];
			ch_cat.priority = j["chart_category_priority"][i]["priority"];
			if (verbose > 0) {
				printf("chart_category[%d] name= '%s', priority= %d at %s %d\n",
						i, ch_cat.name.c_str(), ch_cat.priority, __FILE__, __LINE__);
			}
			chart_category.push_back(ch_cat);
		} catch (...) { }
	}
	sz = j["event_array"].size();
	return sz;
}

uint32_t do_json(uint32_t want_evt_num, std::string lkfor_evt_name, std::string json_file, std::string str, std::vector <evt_str> &event_table, int verbose)
{
	if (verbose > 0)
		std::cout << str << std::endl;
	uint32_t sz = (int)str.size();
	bool use_charts_default = true;
	bool worked=false;

	if (want_evt_num >= 0 && want_evt_num != UINT32_M1 && lkfor_evt_name.size() > 0) {
		fprintf(stderr, "do_json: enter either want_evt_num or lkfor_event_name, but not both. Bye at %s %d\n", __FILE__, __LINE__);
		exit(1);
	}
	if (want_evt_num == UINT32_M1 && lkfor_evt_name.size() == 0) {
		fprintf(stderr, "do_json: enter either want_evt_num or lkfor_event_name. Bye at %s %d\n", __FILE__, __LINE__);
		exit(1);
	}
	json j;
	try {
		j = json::parse(str);
		worked=true;
	} catch (json::parse_error& e) {
		std::cout << "message: " << e.what() << '\n' << "exception id: " << e.id << '\n' << "byte position of error: " << e.byte << std::endl;
	}
	if (!worked) {
		printf("parse of json data file= %s failed. bye at %s %d\n", json_file.c_str(), __FILE__, __LINE__);
		printf("Here is the string we tried to parse at %s %d:\n%s\n", __FILE__, __LINE__, str.c_str());
		prt_line(sz);

		exit(1);
	}
	int32_t pixels_high_min = 100;
	int32_t pixels_high_max = 1500;
	int32_t pixels_high_default = 250;
	try {
		std::string fld = "pixels_high_default";
		int32_t pxls_high = j[fld];
		pixels_high_default = ck_pixels_high(json_file, "set defaults", fld, pxls_high, __LINE__);
	} catch (...) { };
	chart_defaults.pixels_high_default;
	try {
		std::string use_def = j["chart_use_default"];
		if (use_def == "y") {
			use_charts_default = true;
		} else {
			use_charts_default = false;
		}
	} catch (...) { };
	sz = j["event_array"].size();
	if (verbose > 0) {
		printf("sz= %d\n", sz);
	}
	try {
		for (uint32_t i=0; i < sz; i++) {
			if (want_evt_num != UINT32_M1 && i != want_evt_num) {
				continue;
			}
			if (verbose > 0) {
				std::cout << j["event_array"][i]["event"].dump() << std::endl;
				fflush(NULL);
			}
			std::string evt_nm  = j["event_array"][i]["event"]["evt_name"];
			std::string evt_typ = j["event_array"][i]["event"]["evt_type"];
			if (verbose > 0) {
				printf("evt_nm[%d]= '%s', typ= '%s'\n", i, evt_nm.c_str(), evt_typ.c_str());
			}
			if (lkfor_evt_name.size() > 0) {
				if (evt_nm == lkfor_evt_name) {
					if (verbose > 0) {
						printf("do_json: add evt_nm= %s at %s %d\n", evt_nm.c_str(), __FILE__, __LINE__);
					}
				} else {
					if (verbose > 0) {
						printf("do_json: skip evt_nm= %s at %s %d\n", evt_nm.c_str(), __FILE__, __LINE__);
					}
					continue;
				}
			}
			if (verbose > 0) {
				std::cout << j["event_array"][i]["event"]["flds"].dump() << std::endl;
			}
			struct evt_str es;
			es.event_name = evt_nm;
			es.event_type = evt_typ;
			event_table.push_back(es);
			uint32_t fsz = 0;
			try {
				fsz = j["event_array"][i]["event"]["evt_derived"]["evts_used"].size();
				for (uint32_t k=0; k < fsz; k++) {
					event_table.back().evt_derived.evts_used.push_back(j["event_array"][i]["event"]["evt_derived"]["evts_used"][k]);
				}
				fsz = j["event_array"][i]["event"]["evt_derived"]["new_cols"].size();
				for (uint32_t k=0; k < fsz; k++) {
					event_table.back().evt_derived.new_cols.push_back(j["event_array"][i]["event"]["evt_derived"]["new_cols"][k]);
				}
				event_table.back().evt_derived.evt_trigger = j["event_array"][i]["event"]["evt_derived"]["evt_trigger"];
				event_table.back().evt_derived.lua_file = j["event_array"][i]["event"]["evt_derived"]["lua_file"];
				event_table.back().evt_derived.lua_rtn  = j["event_array"][i]["event"]["evt_derived"]["lua_rtn"];
			} catch (...) { }
			//fflush(NULL);
			try {
				fsz = j["event_array"][i]["event"]["evt_flds"].size();
			} catch (...) { }
			
			bool got_diff_ts_stuff = false;
			bool  got_lag_2nd_by_var = false;
			for (uint32_t k=0; k < fsz; k++) {
				struct fld_str fs;
				std::string name     = j["event_array"][i]["event"]["evt_flds"][k]["name"];
				std::string lkup     = j["event_array"][i]["event"]["evt_flds"][k]["lkup"];
				std::string lkup_typ = j["event_array"][i]["event"]["evt_flds"][k]["lkup_typ"];
				std::string lkup_dlm_str;
				try {
					lkup_dlm_str = j["event_array"][i]["event"]["evt_flds"][k]["lkup_dlm_str"];
				} catch (...) { }
				std::string diff_ts_with_ts_of_prev_by_var_using_fld;
				try {
					diff_ts_with_ts_of_prev_by_var_using_fld = j["event_array"][i]["event"]["evt_flds"][k]["diff_ts_with_ts_of_prev_by_var_using_fld"];
					got_diff_ts_stuff = true;
				} catch (...) { }
				std::string lag_prev_by_var_using_fld;
				uint32_t lag_by_var=0;
				try {
					lag_prev_by_var_using_fld = j["event_array"][i]["event"]["evt_flds"][k]["lag_prev_by_var_using_fld"]["name"];
					lag_by_var = j["event_array"][i]["event"]["evt_flds"][k]["lag_prev_by_var_using_fld"]["by_var"];
				} catch (...) { }
				if (lag_prev_by_var_using_fld.size() > 0 && (lag_by_var != 0 && lag_by_var != 1)) {
					printf("for input json file event= %s, fld_name= %s: got lag_prev_by_var_using_fld= %s but lag_by_var must be 0 or 1. got lag_by_var= %d. Bye at %s %d\n",
							evt_nm.c_str(), name.c_str(), lag_prev_by_var_using_fld.c_str(), lag_by_var, __FILE__, __LINE__);
					exit(1);
				}
				std::string mk_proc_from_comm_tid_flds_1, mk_proc_from_comm_tid_flds_2;
				try {
					mk_proc_from_comm_tid_flds_1 = j["event_array"][i]["event"]["evt_flds"][k]["mk_proc_from_comm_tid_flds"]["comm"];
					mk_proc_from_comm_tid_flds_2 = j["event_array"][i]["event"]["evt_flds"][k]["mk_proc_from_comm_tid_flds"]["tid"];
				} catch (...) { }
				if (mk_proc_from_comm_tid_flds_1.size() > 0 && mk_proc_from_comm_tid_flds_2.size() == 0) {
					printf("for input json file event= %s, fld_name= %s: got mk_proc_from_comm_tid_flds.comm= %s but tid is missing. Bye at %s %d\n",
							evt_nm.c_str(), name.c_str(), mk_proc_from_comm_tid_flds_1.c_str(), __FILE__, __LINE__);
					exit(1);
				}
				std::string next_for_name;
				try {
					next_for_name = j["event_array"][i]["event"]["evt_flds"][k]["next_for_name"];
				} catch (...) { }
				std::vector <std::string> stg = {"actions", "actions_stage2"};
				for (uint32_t kk=0; kk < stg.size(); kk++) {
					try {
					uint32_t asz = j["event_array"][i]["event"]["evt_flds"][k][stg[kk]].size();
					if (verbose > 0) {
						printf("%s size= %d at %s %d\n", stg[kk].c_str(), asz, __FILE__, __LINE__);
					}
					for (uint32_t m=0; m < asz; m++) {
						action_str as;
						as.oper = j["event_array"][i]["event"]["evt_flds"][k][stg[kk]][m]["oper"];
						as.val  = j["event_array"][i]["event"]["evt_flds"][k][stg[kk]][m]["val"];
						if (stg[kk] == "actions") {
							fs.actions.push_back(as);
						} else {
							fs.actions_stage2.push_back(as);
						}
						if (verbose)
							printf("%s[%d] oper= %s, val= %f %g\n", stg[kk].c_str(), m, as.oper.c_str(), as.val, as.val);
						if (!is_action_oper_valid(as.oper)) {
							printf("invalid %s in chart json file: got oper= '%s' at %s %d\n", stg[kk].c_str(), as.oper.c_str(), __FILE__, __LINE__);
							exit(1);
						}
						try {
							as.val  = j["event_array"][i]["event"]["evt_flds"][k][stg[kk]][m]["val1"];
							if (stg[kk] == "actions") {
								fs.actions.back().val1 = as.val;
							} else {
								fs.actions_stage2.back().val1 = as.val;
							}
						} catch (...) { }
					}
					} catch (...) { }
				}
				if (verbose > 0) {
					printf("flds[%d] name= '%s', lkup= '%s', typ= '%s' actions.sz= %d, actions_stage2.sz= %d\n",
					k, name.c_str(), lkup.c_str(), lkup_typ.c_str(), (int)fs.actions.size(), (int)fs.actions_stage2.size());
				}
				uint64_t lkup_flags = 0;
				get_lkup_typ(lkup_typ, lkup_flags, verbose);
				fs.name  = name;
				fs.next_for_name = next_for_name;
				fs.lkup_dlm_str  = lkup_dlm_str;
				fs.diff_ts_with_ts_of_prev_by_var_using_fld  = diff_ts_with_ts_of_prev_by_var_using_fld;
				fs.lag_prev_by_var_using_fld  = lag_prev_by_var_using_fld;
				fs.lag_by_var  = lag_by_var;
				fs.mk_proc_from_comm_tid_flds.comm = mk_proc_from_comm_tid_flds_1;
				fs.mk_proc_from_comm_tid_flds.tid  = mk_proc_from_comm_tid_flds_2;
				fs.lkup  = lkup;
				fs.flags = lkup_flags;
				event_table.back().flds.push_back(fs);
			}
			if (!got_diff_ts_stuff && got_lag_2nd_by_var) {
				printf("for input json file event= %s, got lag_prev_by_var_using_fld and it's lag by_var=1 but you must have specified diff_ts_with_ts_of_prev_by_var_using_fld field too (in order to have set up the secondary by var). Bye at %s %d\n",
							event_table.back().event_name.c_str(), __FILE__, __LINE__);
					exit(1);
			}
			for (uint32_t k=0; k < fsz; k++) {
				uint64_t flg = event_table.back().flds[k].flags;
				if (event_table.back().flds[k].next_for_name.size() > 0) {
					event_table.back().flds[k].next_for_name_idx = -1;
					for (uint32_t m=0; m < fsz; m++) {
						if (event_table.back().flds[m].name.size() > 0 &&
							event_table.back().flds[k].next_for_name == 
							event_table.back().flds[m].name) {
							event_table.back().flds[k].next_for_name_idx = m;
							break;
						}
					}
					if (event_table.back().flds[k].next_for_name_idx == -1) {
						printf("missed match of \"next_for_name\":\"%s\" to any for of the \"name\" fields for event= %s at %s %d\n",
							event_table.back().flds[k].next_for_name.c_str(), evt_nm.c_str(), __FILE__, __LINE__);
						exit(1);
					}
				}
				if (event_table.back().flds[k].lag_by_var == 1) {
					int got_var1 = 0;
					for (uint32_t m=0; m < fsz; m++) {
						uint64_t flg2 = event_table.back().flds[m].flags;
						if (flg2 & FLD_TYP_BY_VAR1) {
							got_var1++;
						}
					}
					if (got_var1 == 0) {
						printf("Error: Got lag_prev_by_var_using_fld with by_var:1 but didn't find any field with flag with TYP_BY_VAR1.\n");
						printf("This is for evt_fld= %s, event= %s. Bye at %s %d\n",
							event_table.back().flds[k].name.c_str(), 
							event_table.back().event_name.c_str(), __FILE__, __LINE__);
						exit(1);
					}
					if (got_var1 > 1) {
						printf("Error: Got lag_prev_by_var_using_fld with by_var:1 but found more than on evt_flds with flag TYP_BY_VAR1.\n");
						printf("This is for evt_fld= %s, event= %s. Bye at %s %d\n",
							event_table.back().flds[k].name.c_str(), 
							event_table.back().event_name.c_str(), __FILE__, __LINE__);
						exit(1);
					}
				}
			}
			uint32_t csz = j["event_array"][i]["event"]["charts"].size();
			if (verbose > 0) {
				printf("charts.size()= %d\n", (int)csz);
			}
			for (uint32_t k=0; k < csz; k++) {
				struct chart_str cs;
				bool use_chart = use_charts_default;
				try {
					std::string use_chrt = j["event_array"][i]["event"]["charts"][k]["use_chart"];
					if (use_chrt == "y") {
						use_chart = true;
					} else if (use_chrt == "d") {
						use_chart = use_charts_default;
					} else {
						use_chart = false;
					}
				} catch (...) { }
				cs.use_chart = use_chart;
				std::string y_fmt, by_val_ts, by_val_dura;
				try {
					y_fmt = j["event_array"][i]["event"]["charts"][k]["y_fmt"];
				} catch (...) { }
				cs.y_fmt = y_fmt;
				try {
					by_val_ts = j["event_array"][i]["event"]["charts"][k]["by_val_ts"];
				} catch (...) { }
				cs.by_val_ts = by_val_ts;
				try {
					by_val_dura = j["event_array"][i]["event"]["charts"][k]["by_val_dura"];
				} catch (...) { }
				cs.by_val_dura = by_val_dura;
				cs.title    = j["event_array"][i]["event"]["charts"][k]["title"];
				cs.var_name = j["event_array"][i]["event"]["charts"][k]["var_name"];
				try {
					cs.by_var   = j["event_array"][i]["event"]["charts"][k]["by_var"];
				} catch (...) { }
				cs.pixels_high = -1;
				try {
					std::string fld = "pixels_high";
					int32_t pxls_high = j["event_array"][i]["event"]["charts"][k][fld];
					//cs.pixels_high = ck_pixels_high(json_file, cs.title, fld, pxls_high, __LINE__);
					cs.pixels_high = pxls_high;
				} catch (...) { }
				for (uint32_t m=0; m < event_table.back().flds.size(); m++) {
					uint64_t flg = event_table.back().flds[m].flags;
					if ((flg & FLD_TYP_BY_VAR0) && event_table.back().flds[m].name != cs.by_var) {
						printf("Error: you have evt_fld flag TYP_VAR_BY_VAR0 on field name= %s but you can only use this flag on the field used in the chart 'by_var'= %s.\n", 
							event_table.back().flds[m].name.c_str(), cs.by_var.c_str());
						printf("this is for event= %s, chart= %s. Bye at %s %d\n",
								event_table.back().event_name.c_str(), cs.title.c_str(), __FILE__, __LINE__);
						exit(1);
					}
				}
				std::string chrt_actn = "actions";
				try {
				json jact = j["event_array"][i]["event"]["charts"][k][chrt_actn];
				//uint32_t asz = j["event_array"][i]["event"]["charts"][k][chrt_actn].size();
				uint32_t asz = jact.size();
				for (uint32_t m=0; m < asz; m++) {
					action_str as;
					as.oper = jact[m]["oper"];
					as.val  = jact[m]["val"];
					cs.actions.push_back(as);
					if (verbose > 0) {
						printf("oper= %s, val= %f %g\n", as.oper.c_str(), as.val, as.val);
					}
					if (!is_action_oper_valid(as.oper)) {
						printf("invalid in chart json file: got oper= '%s' at %s %d\n", as.oper.c_str(), __FILE__, __LINE__);
						exit(1);
					}
					try {
						as.val  = jact[m]["val1"];
						cs.actions.back().val1 = as.val;
					} catch (...) { }
				}
				} catch (...) { }
				try {
					cs.chart_tag = j["event_array"][i]["event"]["charts"][k]["chart_tag"];
				} catch (...) { }
				try {
					cs.chart_category = j["event_array"][i]["event"]["charts"][k]["category"];
				} catch (...) { }
				try {
					cs.chart_type = j["event_array"][i]["event"]["charts"][k]["chart_type"];
				} catch (...) { }
				if (verbose > 0) {
					printf("charts[%d] title= '%s', var_name= '%s', by_var= '%s', tag='%s', type= '%s'\n",
						k, cs.title.c_str(), cs.var_name.c_str(), cs.by_var.c_str(),
						cs.chart_tag.c_str(),
						cs.chart_type.c_str());
				}
				int got_name=-1, got_by_var=-1;
				for (uint32_t m=0; m < fsz; m++) {
					if (cs.var_name == event_table.back().flds[m].name) { 
						got_name = m;
					}
					if (cs.by_var == event_table.back().flds[m].name) { 
						got_by_var = m;
					}
				}
				if (got_name == -1) {
					printf("for event: '%s' with chart title '%s' var_name is '%s' but that fld name is not in the evt_flds list. Bye at %s %d\n",
						evt_nm.c_str(), cs.title.c_str(), cs.var_name.c_str(), __FILE__, __LINE__);
					exit(1);
				}
				cs.var_idx = got_name;
				if (got_by_var == -1 && cs.var_name.size() > 0 && cs.by_var.size() > 0) {
					printf("for event: '%s' with chart title '%s' by_var is '%s' but that fld name is not in the evt_flds list. Bye at %s %d\n",
						evt_nm.c_str(), cs.title.c_str(), cs.by_var.c_str(), __FILE__, __LINE__);
					exit(1);
				}
				cs.by_var_idx = got_by_var;
				event_table.back().charts.push_back(cs);
			}
		}
	} catch (json::exception& e) {
		// output exception information
		std::cout << "message: " << e.what() << '\n'
			  << "exception id: " << e.id << ", at " << __FILE__ << " " << __LINE__ << std::endl;
	};
	fflush(NULL);
	//std::cout << j.dump() << std::endl;
	//std::cout << j.dump(4) << std::endl;
	
#if 0
	for (json::iterator it = j["event_array"].begin(); it != j["event_array"].end(); ++it) {
		//std::cout << it.key() << " : " << it.value() << "\n";
		//std::cout << it.key()  << "\n";
	}
#endif
	
	//std::unordered_map<const char*, double> c_umap
	
	return 0;
}

static void bld_fld_typ(void)
{
	fld_typ_strs.push_back({FLD_TYP_INT,           "TYP_INT"});
	fld_typ_strs.push_back({FLD_TYP_DBL,           "TYP_DBL"});
	fld_typ_strs.push_back({FLD_TYP_STR,           "TYP_STR"});
	fld_typ_strs.push_back({FLD_TYP_SYS_CPU,       "TYP_SYS_CPU"});
	fld_typ_strs.push_back({FLD_TYP_TM_CHG_BY_CPU, "TYP_TM_CHG_BY_CPU"});
	fld_typ_strs.push_back({FLD_TYP_PERIOD,        "TYP_PERIOD"});
	fld_typ_strs.push_back({FLD_TYP_PID,           "TYP_PID"});
	fld_typ_strs.push_back({FLD_TYP_TID,           "TYP_TID"});
	fld_typ_strs.push_back({FLD_TYP_TIMESTAMP,     "TYP_TIMESTAMP"});
	fld_typ_strs.push_back({FLD_TYP_COMM,          "TYP_COMM"});
	fld_typ_strs.push_back({FLD_TYP_COMM_PID,      "TYP_COMM_PID"});
	fld_typ_strs.push_back({FLD_TYP_COMM_PID_TID,  "TYP_COMM_PID_TID"});
	fld_typ_strs.push_back({FLD_TYP_EXCL_PID_0,    "TYP_EXCL_PID_0"});
	fld_typ_strs.push_back({FLD_TYP_DURATION_BEF,  "TYP_DURATION_BEF"});
	fld_typ_strs.push_back({FLD_TYP_DIV_BY_INTERVAL,  "TYP_DIV_BY_INTERVAL"});
	fld_typ_strs.push_back({FLD_TYP_TRC_BIN,       "TYP_TRC_BIN"});
	fld_typ_strs.push_back({FLD_TYP_STATE_AFTER,   "TYP_STATE_AFTER"});
	fld_typ_strs.push_back({FLD_TYP_DURATION_PREV_TS_SAME_BY_VAL,   "TYP_DURATION_PREV_TS_SAME_BY_VAL"});
	fld_typ_strs.push_back({FLD_TYP_DIV_BY_INTERVAL2,  "TYP_DIV_BY_INTERVAL2"});
	fld_typ_strs.push_back({FLD_TYP_TRC_FLD_PFX,   "TYP_TRC_FLD_PFX"});
	fld_typ_strs.push_back({FLD_TYP_ETW_COMM_PID,  "TYP_ETW_COMM_PID"});
	fld_typ_strs.push_back({FLD_TYP_HEX_IN,        "TYP_HEX_IN"});
	fld_typ_strs.push_back({FLD_TYP_BY_VAR0,       "TYP_BY_VAR0"});
	fld_typ_strs.push_back({FLD_TYP_BY_VAR1,       "TYP_BY_VAR1"});
	fld_typ_strs.push_back({FLD_TYP_ETW_COMM_PID2, "TYP_ETW_COMM_PID2"});
	fld_typ_strs.push_back({FLD_TYP_COMM_PID_TID2, "TYP_COMM_PID_TID2"});
	fld_typ_strs.push_back({FLD_TYP_TID2,          "TYP_TID2"});
	fld_typ_strs.push_back({FLD_TYP_CSW_STATE,     "TYP_CSW_STATE"});
	fld_typ_strs.push_back({FLD_TYP_CSW_REASON,    "TYP_CSW_REASON"});
	fld_typ_strs.push_back({FLD_TYP_LAG,           "TYP_LAG"});
	fld_typ_strs.push_back({FLD_TYP_NEW_VAL,       "TYP_NEW_VAL"});
}

std::string rd_json(std::string flnm)
{
	std::ifstream file;
	//long pos = 0;
	bld_fld_typ();
	std::string line;
	file.open (flnm.c_str(), std::ios::in);
	if (!file.is_open()) {
		printf("messed up fopen of flnm= %s at %s %d\n", flnm.c_str(), __FILE__, __LINE__);
		exit(1);
	}
	std::string tot_str;
	while(!file.eof()){
		std::getline (file, line);
		//uint32_t sz = line.size();
		tot_str += line;
	}
	file.close();
	return tot_str;
}

int parse_file_list_json(std::string json_file, std::string str, std::vector <file_list_str> &file_list,
		std::vector <std::string> use_file_tag, int file_mode, std::string root_data_dir, int verbose)
{
	if (verbose > 0)
		std::cout << str << std::endl;
	uint32_t sz = (int)str.size();
	bool use_charts_default = true;
	bool worked=false;

	json j;
	try {
		j = json::parse(str);
		worked=true;
	} catch (json::parse_error& e) {
			// output exception information
			std::cout << "message: " << e.what() << '\n'
				  << "exception id: " << e.id << '\n'
				  << "byte position of error: " << e.byte << std::endl;
	}
	if (!worked) {
		printf("parse of json data file= %s failed. bye at %s %d\n", json_file.c_str(), __FILE__, __LINE__);
		printf("Below is the string we tried to parse at %s %d:\n%s\n", __FILE__, __LINE__, str.c_str());

		prt_line(sz);
		exit(1);
	}
#if 0
	try {
		std::string use_def = j["file_use_default"];
		if (use_def == "y") {
			use_charts_default = true;
		} else {
			use_charts_default = false;
		}
	} catch (...) { }
#endif
	sz = j["file_list"].size();
	std::string lkfor_cur_tag = "%cur_tag%";
	std::string lkfor_root_dir = "%root_dir%";

	std::string first_last_tag;
	std::string root_dir, root_dir_in, root_dir_replace;
	std::string cur_dir;
	std::string cur_tag;
	uint32_t idx_of_first_file_with_last_tag= UINT32_M1;
	if (root_data_dir.size() > 0) {
		root_dir = root_data_dir;
		root_dir_in = root_data_dir;
	}

	if ((file_mode & FILE_MODE_FIRST) && (file_mode & FILE_MODE_LAST)) {
		printf("file_mode can't be both FILE_MODE_FIRST and FILE_MODE_LAST. json file= %s. Bye at %s %d\n",
				json_file.c_str(), __FILE__, __LINE__);
		exit(1);
	}
	if ((file_mode & FILE_MODE_ALL) && (file_mode & (FILE_MODE_FIRST|FILE_MODE_LAST))) {
		printf("file_mode can't be FILE_MODE_ALL and (FILE_MODE_FIRST|FILE_MODE_LAST). json file= %s. Bye at %s %d\n",
				json_file.c_str(), __FILE__, __LINE__);
		exit(1);
	}
	if ((file_mode & FILE_MODE_REPLACE_CUR_DIR_WITH_ROOT) && root_data_dir.size() == 0) {
		printf("If you use FILE_MODE_REPLACE_CUR_DIR_WITH_ROOT then root_data_dir must be passed into rtn. json file= %s. Bye at %s %d\n",
				json_file.c_str(), __FILE__, __LINE__);
		exit(1);
	}

	printf("begin processing json file %s at %s %d\n", json_file.c_str(), __FILE__, __LINE__);
	for (uint32_t i=0; i < sz; i++) {
		if (verbose)
			printf("ckinput: doing line[%d]\n", i);
		try {
			std::string ctag = j["file_list"][i]["cur_tag"];
			if (verbose)
				printf("got line[%d] cur_tag= %s at %s %d\n", i, ctag.c_str(), __FILE__, __LINE__);
			cur_tag = ctag;
			continue;
		} catch (...) { }
		try {
			std::string ctag = j["file_list"][i]["root_dir"];
			if (verbose)
				printf("got line[%d] root_dir= %s at %s %d\n", i, ctag.c_str(), __FILE__, __LINE__);
			root_dir = ctag;
			if (root_data_dir.size() > 0) {
				root_dir = root_data_dir;
			}
			if (root_dir.size() > 0) {
			   if (!(root_dir.back() == '/' || root_dir.back() == '\\')) {
				   root_dir += "/";
			   }
			}
			continue;
		} catch (...) { }
		try {
			std::string ctag = j["file_list"][i]["cur_dir"];
			if (verbose)
				printf("got line[%d] cur_dir= %s at %s %d\n", i, ctag.c_str(), __FILE__, __LINE__);
			cur_dir = ctag;
			if ((file_mode & FILE_MODE_REPLACE_CUR_DIR_WITH_ROOT) && root_data_dir.size() > 0) {
				printf("root_data_dir= '%s' at %s %d\n", root_data_dir.c_str(), __FILE__, __LINE__);
				cur_dir = root_data_dir;
			}
			if (cur_dir.size() > 0) {
			   if (!(cur_dir.back() == '/' || cur_dir.back() == '\\')) {
				   cur_dir += "/";
			   }
			}
			if (!(file_mode & FILE_MODE_REPLACE_CUR_DIR_WITH_ROOT) && root_dir.size() > 0) {
				replace_substr(cur_dir, lkfor_root_dir, root_dir, verbose);
			}
			printf("cur_dir= '%s' at %s %d\n", cur_dir.c_str(), __FILE__, __LINE__);
			continue;
		} catch (...) { }
		std::vector <std::string> flds= {"bin_file", "txt_file", "wait_file", "type", "tag", "lua_file", "lua_rtn"};
		std::vector <std::string> vals;
		std::string lua_file, lua_rtn;
		uint32_t file_typ = UINT32_M1;
		vals.resize(flds.size());
		for (uint32_t fld=0; fld < flds.size(); fld++) {
			try {
				vals[fld] = j["file_list"][i][flds[fld]];
				if (verbose)
					printf("line[%d] \"%s\":\"%s\"\n", fld, flds[fld].c_str(), vals[fld].c_str());
				if (cur_tag.size() > 0) {
					replace_substr(vals[fld], lkfor_cur_tag, cur_tag, verbose);
				}
				if (flds[fld] == "type") {
					if (vals[fld].size() == 0) {
						printf("every line must have a \"type\" field. Error for file_list entry %d in file %s. Bye at %s %d\n",
							i, json_file.c_str(), __FILE__, __LINE__);
						exit(1);
					}
					std::vector <std::string> typ_strs = {"PERF", "TRACE_CMD", "LUA", "ETW"};
					std::vector <uint32_t> typ_vals    = {FILE_TYP_PERF, FILE_TYP_TRC_CMD, FILE_TYP_LUA, FILE_TYP_ETW};
					for (uint32_t kk=0; kk < typ_strs.size(); kk++) {
						if (vals[fld] == typ_strs[kk]) {
							file_typ = typ_vals[kk];
							break;
						}
					}
					if (file_typ == UINT32_M1) {
						printf("every line must have a valid \"type\" field. Entry %d has type:'%s'. Error in file %s. Bye at %s %d\n",
							i, vals[fld].c_str(), json_file.c_str(), __FILE__, __LINE__);
						printf("Valid types are:\n");
						for (uint32_t kk=0; kk < typ_strs.size(); kk++) {
							printf("%s\n", typ_strs[kk].c_str());
						}
						exit(1);
					}
				}
			} catch (...) { }
		}
		file_list_str fls;
		std::string use_dir = cur_dir;
		if (vals[0].size() > 0) {
			fls.file_bin  = use_dir+vals[0];
		}
		if (vals[1].size() > 0) {
			fls.file_txt  = use_dir+vals[1];
		}
		if (vals[2].size() > 0) {
			fls.wait_txt  = use_dir+vals[2];
		}
		fls.typ       = file_typ;
		fls.file_tag  = vals[4];
		if (vals[5].size() > 0) {
			fls.lua_file  = vals[5];
		}
		if (vals[6].size() > 0) {
			fls.lua_rtn   = vals[6];
		}
		if (use_file_tag.size() > 0) {
			for (uint32_t ft=0; ft < use_file_tag.size(); ft++) {
				if (use_file_tag[ft] == fls.file_tag) {
					file_list.push_back(fls);
				}
			}
			continue;
		}
		if (file_mode & FILE_MODE_ALL) {
			file_list.push_back(fls);
		}
		if (file_mode & FILE_MODE_FIRST) {
			if (first_last_tag.size() == 0) {
				first_last_tag = fls.file_tag;
			}
			if (first_last_tag == fls.file_tag) {
				file_list.push_back(fls);
			} else {
				break;
			}
		}
		if (file_mode & FILE_MODE_LAST) {
			if (first_last_tag != fls.file_tag) {
				first_last_tag = fls.file_tag;
				printf("got new 'last' file_tag= %s at %s %d\n", first_last_tag.c_str(), __FILE__, __LINE__);
				idx_of_first_file_with_last_tag= file_list.size();
			}
			file_list.push_back(fls);
		}
	}
	if (file_mode & FILE_MODE_LAST) {
		if (idx_of_first_file_with_last_tag == UINT32_M1) {
			printf("something wrong with your logic here. bye at %s %d\n", __FILE__, __LINE__);
			exit(1);
		}
		file_list.erase(file_list.begin(), file_list.begin()+idx_of_first_file_with_last_tag);
	}
	fflush(NULL);
	return 0;
}

int read_file_list_json(std::string flnm, std::vector <file_list_str> &file_list, std::vector<std::string> use_file_tag,
	int file_mode, std::string root_data_dir, int verbose)
{
	std::ifstream file;
	//long pos = 0;
	std::string line;
	file.open (flnm.c_str(), std::ios::in);
	if (!file.is_open()) {
		printf("messed up fopen of flnm= %s at %s %d\n", flnm.c_str(), __FILE__, __LINE__);
		exit(1);
	}
	std::string tot_str;
	while(!file.eof()){
		std::getline (file, line);
		//uint32_t sz = line.size();
		tot_str += line;
	}
	file.close();
	if (tot_str.size() == 0) {
		printf("hmm... got no data from input file list filename= %s. Bye at %s %d\n", flnm.c_str(), __FILE__, __LINE__);
		exit(1);
	}
	return parse_file_list_json(flnm, tot_str, file_list, use_file_tag, file_mode, root_data_dir, verbose);
}

