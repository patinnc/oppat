/* Copyright (c) 2018 Patrick Fay
 *
 * License http://opensource.org/licenses/mit-license.php MIT License
 */

#pragma once
int prf_read_data_bin(std::string flnm, int verbose, prf_obj_str &prf_obj, double tm_beg);
size_t prf_sample_to_string(int idx, std::string &ostr, prf_obj_str &prf_obj);
int prf_parse_text(std::string flnm, prf_obj_str &prf_obj, double tm_beg_in, int verbose);
