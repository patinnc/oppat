/* Copyright (c) 2018 Patrick Fay
 *
 * License http://opensource.org/licenses/mit-license.php MIT License
 */

#pragma once

#ifdef EXTERN_STR
#undef EXTERN_STR
#endif

#ifdef TC_RD_DATA_CPP
#define EXTERN_STR
#else
#define EXTERN_STR extern
#endif

EXTERN_STR int tc_read_data_bin(std::string flnm, int verbose, prf_obj_str &prf_obj, double tm_beg_in, prf_obj_str *prf_obj_prev);
EXTERN_STR int tp_read_event_formats(std::string flnm, int verbose);
EXTERN_STR int tc_parse_text(std::string flnm, prf_obj_str &prf_obj, double tm_beg_in, int verbose, std::vector <evt_str> &evt_tbl2);
EXTERN_STR uint32_t ck_for_match_on_event_name(std::string evt, prf_obj_str &prf_obj, int verbose);
EXTERN_STR void ck_derived_evts(prf_obj_str &prf_obj, std::vector <evt_str> &evt_tbl2, int verbose);
EXTERN_STR void ck_if_evt_used_in_derived_evt(int mtch, prf_obj_str &prf_obj, int verbose, std::vector <evt_str> &evt_tbl2,
				std::vector <prf_samples_str> &samples, std::string &extra_str);
