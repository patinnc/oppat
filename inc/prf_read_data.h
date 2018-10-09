/* Copyright (c) 2018 Patrick Fay
 *
 * License http://opensource.org/licenses/mit-license.php MIT License
 */

#pragma once

#include "rd_json.h"

#ifdef EXTERN_STR
#undef EXTERN_STR
#endif

#ifdef PRF_RD_DATA_CPP
#define EXTERN_STR
#else
#define EXTERN_STR extern
#endif

EXTERN_STR int prf_read_data_bin(std::string flnm, int verbose, prf_obj_str &prf_obj, double tm_beg, file_list_str &file_list);
EXTERN_STR size_t prf_sample_to_string(int idx, std::string &ostr, prf_obj_str &prf_obj);
EXTERN_STR int prf_parse_text(std::string flnm, prf_obj_str &prf_obj, double tm_beg_in, int verbose, std::vector <evt_str> &evt_tbl2);
