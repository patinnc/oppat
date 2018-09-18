/* Copyright (c) 2018 Patrick Fay
 *
 * License http://opensource.org/licenses/mit-license.php MIT License
 */

#include "rd_json.h"

#pragma once
int etw_split_comm(const std::string &s, std::string &comm, std::string &pid, double ts);
int etw_parse_text(std::string flnm, prf_obj_str &prf_obj, double tm_beg_in, int verbose, std::vector <evt_str> &evt_tbl2);
double hires_clk(void);
