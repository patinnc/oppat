/* Copyright (c) 2018 Patrick Fay
 *
 * License http://opensource.org/licenses/mit-license.php MIT License
 */

#pragma once

#ifdef EXTERN2
#undef EXTERN2
#endif
#ifdef LUA_RTNS_CPP
#define EXTERN2 
#else
#define EXTERN2 extern
#endif
EXTERN2 int lua_read_data(std::string data_filename, std::string data2_filename, std::string wait_filename, prf_obj_str &prf_obj, int verbose);
