/* Copyright (c) 2018 Patrick Fay
 *
 * License http://opensource.org/licenses/mit-license.php MIT License
 */

#pragma once

enum {
	CLIP_NONE,
	CLIP_LVL_1,
	CLIP_LVL_2,
};

enum {
	SHOW_JSON_NO=0,
	SHOW_JSON_PER_FILE,
	SHOW_JSON_ALL,
};

struct path_file_str {
	std::string path, file;
};

struct options_str {
	int verbose, help, file_mode, show_json, web_port;
	std::string chart_file, data_file;
	std::string perf_bin, perf_txt;
	std::string tc_bin, tc_txt;
	std::string lua_energy, lua_energy2, lua_wait;
	std::string etw_txt;
	std::string replay_filename;
	std::string web_file;
	std::string cpu_diagram;
	std::vector <path_file_str> file_list;
	std::vector <std::string> follow_proc;
	std::vector <std::string> root_data_dirs;
	std::vector <std::string> file_tag_vec;
	std::vector <std::string> phase, phase_end;
	std::vector <std::string> ph_step_int;
	std::vector <std::string> by_phase;
	std::vector <std::string> ph_image;
	std::vector <std::string> img_wxh_pxls;
	std::string j2x;
	double tm_clip_beg, tm_clip_end;
	int tm_clip_beg_valid, tm_clip_end_valid;
	int clip_mode;
	int marker_beg_num, marker_end_num;
	bool load_replay_file, web_file_quit;
	options_str(): verbose(0), help(0), file_mode(-1), show_json(SHOW_JSON_NO), web_port(8081),
		tm_clip_beg(-1.0), tm_clip_end(-1.0), clip_mode(CLIP_NONE),
		tm_clip_beg_valid(CLIP_NONE), tm_clip_end_valid(CLIP_NONE),
		marker_beg_num(-1), marker_end_num(-1),
   		load_replay_file(false), web_file_quit(false) {}
};

struct numa_str {
	uint32_t nodenr;
	uint64_t mem_total; // in KBytes
	uint64_t mem_free;  // in KBytes
	std::string cpus;
};

struct pmu_str {
	uint32_t pmu_type;
	std::string pmu_name;
};

struct group_str {
	std::string leader;
	uint32_t leader_idx;
	uint32_t nr_members;
};

struct cache_str {
	uint32_t level, line_size, sets, ways;
	std::string type, size, map;
};
struct prf_comm_str {
	uint32_t pid, tid;
	std::string comm;
	double tm_defined;
	prf_comm_str(): pid(-1), tid(-1), tm_defined(0.0) {}
};

struct pe_group_str {
	uint64_t grp, tm_ena, tm_run;
	struct pe_vals_str {
		uint64_t val, id, off;
		pe_vals_str(): val(0), id(-1) {}
	};
	std::vector <pe_vals_str> pe_vals;
	pe_group_str(): grp(-1), tm_ena(0), tm_run(0) {}
};

struct prf_events_str {
	struct perf_event_attr pea;
	std::vector <uint64_t> ids;
	std::vector <std::string> etw_cols, new_cols;
	std::unordered_map<std::string, uint32_t> etw_col_hsh;
	std::vector <std::string> etw_col_vec;
	std::string event_area;
	std::string event_name;
	std::string event_name_w_area;
	int lst_ft_fmt_idx, evt_count;
	struct pe_group_str pe_grp;
	prf_events_str(): lst_ft_fmt_idx(-2), evt_count(0) {}
};
struct prf_callstack_str {
	std::string mod, rtn;
};

struct lua_data_val_str {
	std::string str;
	double dval;
	int ival;
};
struct lua_data_str {
	std::vector <std::vector <std::string>> col_names;
	std::vector <std::vector <int>> col_typ;
	std::vector <std::string> events;
	std::vector <std::vector <lua_data_val_str>> data_rows;
	std::vector <int> timestamp_idx, duration_idx;
};

struct gen_div_det_str {
	uint64_t ts[2], ts_prev[2], val[2];
	double dval;
	int32_t paired;
	gen_div_det_str(): paired(-1), dval(0.0), ts{0, 0}, ts_prev{0, 0} {}
};

struct gen_div_str {
	uint32_t col_val_idx, col_emt_idx, col_dur_idx, col_area_idx, col_num_idx, col_den_idx;
	std::vector <gen_div_det_str> det;
	std::unordered_map<std::string, uint32_t> hsh_str;
	std::vector <std::string> vec_str;
	gen_div_str(): col_val_idx(-1), col_emt_idx(-1), col_dur_idx(-1), col_area_idx(-1), col_num_idx(-1), col_den_idx(-1) {}
};

struct evts_derived_str {
	uint32_t evt_tbl2_idx, trigger_idx, evt_new_idx;
	struct gen_div_str gen_div;
	std::vector <uint32_t> evts_used;
	std::vector <std::string> evts_tags;
	std::vector <std::string> new_cols;
	std::vector <std::string> new_vals;
};

struct prf_samples_str {
	std::string comm, event, tm_str, extra_str;
	uint32_t evt_idx, pid, tid, cpu;
	int fe_idx, orig_order, line_num;
	long mm_off;
	uint64_t ts, period, tm_run;
	std::vector <std::string> args, new_vals;
	std::vector <prf_callstack_str> callstack;
	prf_samples_str(): evt_idx(-1), pid(-1), tid(-1), cpu(-1), fe_idx(-1),
		orig_order(-1), line_num(0), mm_off(-1), ts(0), period(0), tm_run(0) {}
};

struct prf_event_desc_str { 
	struct perf_event_attr attr;  /* size of attr_size */
	uint32_t nr_ids;
	std::string event_string;
	std::vector <uint64_t> ids;
};

struct etw_str {
	uint32_t txt_idx, data_idx, cs_idx_beg, cs_idx_end;
	uint64_t ts;
	etw_str(): txt_idx(-1), data_idx(-1), cs_idx_beg(-1), cs_idx_end(-1), ts(0) {}
};

#include "MemoryMapped.h"

struct prf_obj_str {
	std::string filename_bin, filename_text;
	int file_type;
	const unsigned char *mm_buf;
	int mm_idx;
	
	std::string features_hostname;
	std::string features_cpuid;
	std::string features_cpudesc;
	std::string features_arch;
	std::vector <std::string> features_cmdline;
	std::vector <std::string> features_topology_cores;
	std::vector <std::string> features_topology_threads;
	std::vector <int> map_cpu_2_core;
	lua_data_str lua_data;
	int features_nr_cpus_online;
	int features_nr_cpus_available;
	std::vector <numa_str> features_numa_topology;
	std::vector <pmu_str> features_pmu_mappings;
	std::vector <group_str> features_group_desc;
	std::vector <cache_str> features_cache;

	std::vector <uint64_t> features_sample_time;

	std::vector <prf_comm_str> comm;
	std::unordered_map<uint32_t, int> tid_2_comm_indxp1;

	std::unordered_map<uint32_t, uint32_t> ids_2_evt_indxp1;
	std::vector <uint32_t> ids_vec;

	std::vector <prf_events_str> events;
	std::vector <prf_event_desc_str> features_event_desc;

	std::unordered_map<std::string, uint32_t> etw_evts_hsh;
	std::vector <std::string> etw_evts_vec;
	std::vector <std::vector <etw_str>> etw_evts_set;
	std::vector <std::vector <std::string>> etw_data;
	double tm_beg, tm_end, tm_beg_offset_due_to_clip;

	std::vector <prf_samples_str> samples;
	int sample_id_all;
	int file_tag_idx;
	bool has_ids, has_tm_run;
	uint32_t def_sample_flags;
	prf_obj_str(): file_type(-1), mm_buf(0), mm_idx(-1), tm_beg(0), tm_end(0),
		tm_beg_offset_due_to_clip(0.0), sample_id_all(1),
		file_tag_idx(-1), has_ids(false), has_tm_run(false), def_sample_flags(0) {};
};

#ifdef EXTERN2
#undef EXTERN2
#endif
#ifdef OPPAT_CPP
#define EXTERN2 
#else
#define EXTERN2 extern
#endif
#pragma once

EXTERN2 std::vector <MemoryMapped *> mm_vec;
EXTERN2 struct options_str options;

void prf_add_ids(uint32_t id, int evt_idx, prf_obj_str &prf_obj);
void prf_add_comm(uint32_t pid, uint32_t tid, std::string comm, prf_obj_str &prf_obj, double tm);
uint32_t hash_comm_pid_tid(std::unordered_map<std::string, int> &hsh_str, std::vector <comm_pid_tid_str> &vec_str, std::string comm, int pid, int tid);
uint32_t hash_uint32(std::unordered_map <uint32_t, uint32_t> &hsh_u32, std::vector <uint32_t> &vec_u32, uint32_t lkup, uint32_t val);
uint64_t *buf_uint64_ptr(char *buf, int off);
uint16_t *buf_uint16_ptr(char *buf, int off);
uint32_t *buf_uint32_ptr(char *buf, int off);
int32_t  *buf_int32_ptr(char *buf, int off);
long     *buf_long_ptr(char *buf, int off);
int hex_dump_n_bytes_from_buf(char *buf_in, long sz, std::string pref, int line);
int hex_dump_bytes(std::ifstream &file, long &pos_in, long sz, std::string pref, int line);
int mm_read_n_bytes(const unsigned char *mm_buf, long &pos, int n, int line, char *buf, int buf_sz);
int mm_read_n_bytes_buf(const unsigned char *mm_buf, long &pos, int n, char *nw_buf, int line);
int mm_read_till_null(const unsigned char *mm_buf, long &pos, int line, char *buf, int buf_sz);
bool compareByTime(const prf_samples_str &a, const prf_samples_str &b);
//EXTERN2 std::vector <prf_samples_str> prf_samples;
