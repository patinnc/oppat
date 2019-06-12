/* Copyright (c) 2018 Patrick Fay
 *
 * License http://opensource.org/licenses/mit-license.php MIT License
 */
#include <regex>
#include <unordered_map>
#include "json.hpp"

enum {
	FILE_MODE_FIRST = 0x01,
	FILE_MODE_LAST  = 0x02,
	FILE_MODE_ALL   = 0x04,
	FILE_MODE_REPLACE_CUR_DIR_WITH_ROOT   = 0x08,
};


struct flnm_evt_str {
	std::string filename_bin, filename_text, evt_str;
	uint32_t prf_obj_idx, prf_evt_idx, evt_tbl_idx, evt_tbl_evt_idx, total,
			 file_tag_idx;
	bool has_callstacks;
	flnm_evt_str(): prf_obj_idx(-1), prf_evt_idx(-1), evt_tbl_idx(-1), evt_tbl_evt_idx(-1),
		total(0), file_tag_idx(-1), has_callstacks(false) {}
};

struct action_str {
	std::string oper;
	double val, val1;
	std::string str, str1;
	uint32_t regex_fld_idx;
	std::regex regx;
};
struct comm_tid_str {
	std::string comm, tid;
};

struct fld_str {
	std::string name, lkup, next_for_name, lkup_dlm_str;
	std::string diff_ts_with_ts_of_prev_by_var_using_fld;
	std::string lag_prev_by_var_using_fld;
	comm_tid_str mk_proc_from_comm_tid_flds;
	uint64_t flags;
	uint32_t got_factor, lag_by_var;
	int lst_ft_fmt_fld_idx, next_for_name_idx;
	std::vector <action_str> actions;
	std::vector <action_str> actions_stage2;
	fld_str(): flags(0), lag_by_var(0), lst_ft_fmt_fld_idx(-2), next_for_name_idx(-1) { }
};

struct ts_str {
	double ts, duration;
	ts_str(): ts(0.0), duration(0.0) {}
};
	

struct data_str {
	std::vector <ts_str> ts;
	std::vector <std::vector <double>> vals;
	std::vector <std::vector <uint32_t>> cpt_idx;
	std::vector <int> prf_sample_idx;
	std::vector <int> comm_pid_tid_idx;
};

struct lag_str {
	int fld_from, fld_to, by_var;
	lag_str(): fld_from(-1), fld_to(-1), by_var(0) {}
};

struct tot_line_opts_str {
	std::string xform, yval_fmt, yvar_fmt, desc;
	std::vector <std::string> scope;
};

struct chart_str {
	std::string title, var_name, by_var, chart_tag, chart_category, chart_type,
		y_fmt, by_val_ts, by_val_dura, y_label, tot_line,
		marker_type, marker_size, marker_connect, marker_text, marker_ymin;
	struct tot_line_opts_str tot_line_opts;
	bool use_chart;
	uint32_t var_idx, by_var_idx;
	int32_t pixels_high;
	uint64_t options;
	std::vector <std::string> options_strs;
	std::unordered_map <double, uint32_t> by_var_hsh;
	std::vector <double> by_var_vals, by_var_sub_tots;
	std::vector <double> dura_prev_ts;
	std::vector <double> next_ts;
	std::vector <double> next_val;
	std::vector <action_str> actions;
	std::vector <lag_str> lag_cfg;
	std::vector <std::vector <double>> lag_vec;
	std::vector <std::string> by_var_strs;
	chart_str(): use_chart(true), var_idx(-1), by_var_idx(-1), pixels_high(-1), options(0) {};
};

struct chart_cat_str {
	std::string name;
	int priority;
	chart_cat_str(): priority(0) {}
};

struct chart_defaults_str {
	int32_t pixels_high_default;
	int32_t drop_event_if_samples_exceed;
	int32_t dont_show_events_on_cpu_busy_if_samples_exceed;
	chart_defaults_str(): pixels_high_default(250), drop_event_if_samples_exceed(200000),
		dont_show_events_on_cpu_busy_if_samples_exceed(500000) {}
};

struct evt_derived_str {
	std::vector <std::string> evts_used;
	std::vector <std::string> evts_to_exclude;
	std::vector <std::string> evts_tags;
	std::vector <std::string> new_cols;
	std::string evt_trigger, lua_file, lua_rtn;
};

struct evt_aliases_str {
	std::string evt_name, arch;
	std::vector <std::string> aliases;
	uint32_t use_alias;
	evt_aliases_str(): use_alias(-1) {}
};

struct evt_str {
	std::string event_name, event_type, event_name_w_area, event_arch;
	struct evt_derived_str evt_derived;
	uint32_t prf_obj_idx, event_idx_in_file;
	std::unordered_map <std::string, uint32_t> hsh_str;
	std::vector <std::string> vec_str;
	std::vector <fld_str> flds;
	std::vector <chart_str> charts;
	struct data_str data;
	double ts_last, prf_ts_initial;
	int file_tag_idx;
	evt_str(): prf_obj_idx(-1), event_idx_in_file(-1), ts_last(-1.0), prf_ts_initial(-1.0), file_tag_idx(-1) {}
};

struct fld_typ_str {
	uint64_t flag;
	std::string str;
};

enum class copt_enum : const uint64_t {
	DROP_1ST						= (1ULL << 0),
	OVERLAPPING_RANGES_WITHIN_AREA	= (1ULL << 1),
	TOT_LINE_ADD_VALUES_IN_INTERVAL = (1ULL << 2),
	TOT_LINE_LEGEND_WEIGHT_BY_DURA  = (1ULL << 3),
	TOT_LINE_BUCKET_BY_END_OF_SAMPLE= (1ULL << 4),
	SHOW_EVEN_IF_ALL_ZERO           = (1ULL << 5),
};

enum class fte_enum : const uint64_t {
	FLD_TYP_INT           = (1ULL << 0),
	FLD_TYP_DBL           = (1ULL << 1),
	FLD_TYP_STR           = (1ULL << 2),
	FLD_TYP_SYS_CPU       = (1ULL << 3),
	FLD_TYP_TM_CHG_BY_CPU = (1ULL << 4),
	FLD_TYP_PERIOD        = (1ULL << 5),
	FLD_TYP_PID           = (1ULL << 6),
	FLD_TYP_TID           = (1ULL << 7),
	FLD_TYP_TIMESTAMP     = (1ULL << 8),
	FLD_TYP_COMM          = (1ULL << 9),
	FLD_TYP_COMM_PID      = (1ULL << 10),
	FLD_TYP_COMM_PID_TID  = (1ULL << 11),
	FLD_TYP_EXCL_PID_0    = (1ULL << 12),
	FLD_TYP_DURATION_BEF  = (1ULL << 13),
	FLD_TYP_DIV_BY_INTERVAL = (1ULL << 14),
	FLD_TYP_TRC_BIN       = (1ULL << 15),
	FLD_TYP_STATE_AFTER   = (1ULL << 16),
	FLD_TYP_DURATION_PREV_TS_SAME_BY_VAL = (1ULL << 17),
	FLD_TYP_DIV_BY_INTERVAL2 = (1ULL << 18),
	FLD_TYP_TRC_FLD_PFX   = (1ULL << 19),
	FLD_TYP_ETW_COMM_PID  = (1ULL << 20),
	FLD_TYP_HEX_IN        = (1ULL << 21),
	FLD_TYP_BY_VAR0       = (1ULL << 22),
	FLD_TYP_BY_VAR1       = (1ULL << 23),
	FLD_TYP_ETW_COMM_PID2 = (1ULL << 24),
	FLD_TYP_COMM_PID_TID2 = (1ULL << 25),
	FLD_TYP_TID2          = (1ULL << 26),
	FLD_TYP_CSW_STATE     = (1ULL << 27),
	FLD_TYP_CSW_REASON    = (1ULL << 28),
	FLD_TYP_LAG           = (1ULL << 29),
	FLD_TYP_NEW_VAL       = (1ULL << 30),
	FLD_TYP_ADD_2_EXTRA   = (1ULL << 31),
	FLD_TYP_TM_RUN        = (1ULL << 32),
};

enum {
	FILE_TYP_PERF=1,
	FILE_TYP_TRC_CMD,
	FILE_TYP_LUA,
	FILE_TYP_ETW,
};

struct lst_fld_str {
	std::string name, typ;
	uint32_t size, offset, sgned, arr_sz, common;
};

struct lst_ft_per_fld_str {
	uint32_t flags;
	std::string prefix, fld_nm, typ;
};

struct lst_ft_fmt_str {
	int id;
	std::string area, event, fmt;
	std::vector <lst_ft_per_fld_str> per_fld;
	std::vector <lst_fld_str> fields;
};

struct tp_event_str {
	std::string area, event;
	int id;
};

struct file_list_str {
	std::string file_bin, file_txt, wait_txt, file_tag, lua_file, lua_rtn, perf_event_list_dump, path, options, use_line;
	std::vector <lst_ft_fmt_str> lst_ft_fmt_vec;
	std::vector <tp_event_str> tp_events;
	std::unordered_map<int, int> tp_id_2_event_indxp1;
	int typ, grp, idx;
	file_list_str(): typ(-1), grp(-1), idx(-1), use_line("y") {}
};

#pragma once

#ifdef EXTERN_STR
#undef EXTERN_STR
#endif

#ifdef RD_JSON_CPP
#define EXTERN_STR
#else
#define EXTERN_STR extern
#endif

EXTERN_STR chart_defaults_str chart_defaults;
EXTERN_STR std::vector <evt_aliases_str> evt_aliases_vec;

EXTERN_STR std::vector <chart_cat_str> chart_category;
EXTERN_STR int read_file_list_json(std::string flnm, std::vector <file_list_str> &file_list, std::vector <std::string> use_file_tag,
	int read_file_mode, std::string root_data_dir, int verbose);
EXTERN_STR std::string rd_json(std::string flnm);
EXTERN_STR uint32_t do_json(uint32_t want_evt_num, std::string lkfor_evt_nm, std::string json_file, std::string &str,
	std::vector <evt_str> &event_table,  std::string features_cpuid, int verbose);
EXTERN_STR int do_json_evt_chrts_defaults(std::string json_file, std::string str, int verbose);
EXTERN_STR std::unordered_map<std::string, uint32_t> ETW_events_to_skip_hash;
EXTERN_STR std::vector <std::string> ETW_events_to_skip_vec;

EXTERN_STR uint32_t hash_string(std::unordered_map<std::string, uint32_t> &hsh_str, std::vector <std::string> &vec_str, std::string str);
EXTERN_STR int ck_json(std::string &str, std::string from_where, bool just_parse, nlohmann::json &jobj, const char *file, int line, int verbose);


