/* Copyright (c) 2018 Patrick Fay
 *
 * License http://opensource.org/licenses/mit-license.php MIT License
 */

#pragma once

int tc_read_data_bin(std::string flnm, int verbose, prf_obj_str &prf_obj, double tm_beg_in, prf_obj_str *prf_obj_prev);
int tp_read_event_formats(std::string flnm, int verbose);
int tc_parse_text(std::string flnm, prf_obj_str &prf_obj, double tm_beg_in, int verbose);
