/* Copyright (c) 2018 Patrick Fay
 *
 * License http://opensource.org/licenses/mit-license.php MIT License
 */

#pragma once
void etw_split_comm(const std::string &s, std::string &comm, std::string &pid);
int etw_parse_text(std::string flnm, prf_obj_str &prf_obj, double tm_beg_in, int verbose);
double hires_clk(void);
