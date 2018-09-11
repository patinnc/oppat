/* Copyright (c) 2018 Patrick Fay
 *
 * License http://opensource.org/licenses/mit-license.php MIT License
 */

#pragma once

void tkn_split(const std::string& s, std::string c, std::vector <std::string>& v);
void replace_substr(std::string &base_str, std::string lkfor_str, std::string replace_with_str, int verbose);
int search_for_file(std::string base_file, std::string &found_file, std::vector <std::string> &tried_filenames,
		const char *called_by_file, int called_by_line, int verbose);
