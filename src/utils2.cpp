/* Copyright (c) 2018 Patrick Fay
 *
 * License http://opensource.org/licenses/mit-license.php MIT License
 */

#ifdef _WIN32
#include <windows.h>
#endif
#ifdef __linux__
#include <sys/time.h>
#include <limits.h>
#define _strdup strdup
#endif

#include <string.h>

#include <time.h>
#include <stdio.h>
#include <chrono>
#include <iostream>
#include <string>
#include <chrono>
#include <vector>
#include "utils.h"

#include <stdint.h>
#define UTILS2_CPP
#include "utils2.h"

static void my_str_trim(const std::string& str_in, std::string &str_out, const std::string& space_chars = " \t")
{
	size_t beg = str_in.find_first_not_of(space_chars);
	if (beg == std::string::npos) {
		str_out = "";
		return;
	}
	size_t end = str_in.find_last_not_of(space_chars);
	str_out = str_in.substr(beg, end-beg+1);
	return;
}



void tkn_split(const std::string& s, std::string c, std::vector <std::string>& v)
{
	//based on https://www.safaribooksonline.com/library/view/c-cookbook/0596007612/ch04s07.html
	// does not require license. see https://www.safaribooksonline.com/library/view/c-cookbook/0596007612/pr02s03.html
	/*
Using Code Examples

This book is designed to help you get your job done. In general, you may use the code in this book in your programs and documentation. You do not need to contact us for permission unless you're reproducing a significant portion of the code. For example, writing a program that uses several chunks of code from this book does not require permission. Selling or distributing a CD-ROM of examples from O'Reilly books does require permission. Answering a question by citing this book and quoting example code does not require permission. Incorporating a significant amount of example code from this book into your product's documentation does require permission.

We appreciate, but do not require, attribution. An attribution usually includes the title, author, publisher, and ISBN. For example: "C++ Cookbook by D. Ryan Stephens, Christopher Diggins, Jonathan Turkanis, and Jeff Cogswell. Copyright 2006 O'Reilly Media, Inc., 0-596-00761-2."

	 */
	v.resize(0);
	std::string::size_type i = 0;
	std::string::size_type j = s.find(c);
	std::string::size_type clen = c.length();
	std::string tkn;

	while (j != std::string::npos) {
		my_str_trim(s.substr(i, j-i), tkn, " ");
		v.push_back(tkn);
		j += clen;
		i = j;
		j = s.find(c, j);

		if (j == std::string::npos) {
			my_str_trim(s.substr(i, s.length()), tkn, " ");
			v.push_back(tkn);
		}
	}
	return;
}

void replace_substr(std::string &base_str, std::string lkfor_str, std::string replace_with_str, int verbose)
{
	if (base_str.size() == 0) {
		return;
	}
	size_t pos = base_str.find(lkfor_str);
	if (pos != std::string::npos) {
		if (verbose)
			printf("ckfor bef cur_tag in str= %s at %s %d\n", base_str.c_str(), __FILE__, __LINE__);
		while(pos != std::string::npos) {
			base_str.replace(pos, lkfor_str.size(), replace_with_str);
			pos = base_str.find(lkfor_str, pos+replace_with_str.size());
		}
		if (verbose)
			printf("ckfor aft replace in str= %s at %s %d\n", base_str.c_str(), __FILE__, __LINE__);
	}
}


/* search for oppat files
 * 0) try base_file name in cur dir
 * 1) try oppat/bin/base_file
 * 2) try oppat/bin/../input_files/base_file
 * 3) try oppat/bin/../base_file   # for files like lua_src or web
 * put each tried name int tried_filenames
 * return 0 if found it. put found name in found_file
 *        not zero if found
 */
int search_for_file(std::string base_file, std::string &found_file, std::vector <std::string> &tried_filenames,
		const char *called_by_file, int called_by_line, int verbose)
{
	int rc;
	std::string root_dir, ds, flnm;
	found_file.clear();
	tried_filenames.resize(0);
	std::vector <std::string> search_typ = {"cur_dir", "bin_dir", "input_files", "../bin_dir"};
	for (uint32_t i=0; i < search_typ.size(); i++) {
		if (i == 0) {
			flnm = base_file;
		} else if (i == 1) {
			root_dir = std::string(get_root_dir_of_exe());
			ds = std::string(DIR_SEP);
			flnm = root_dir + ds + base_file;
		} else if (i == 2) {
			flnm = root_dir + ds + ".." + ds + "input_files" + ds + base_file;
		} else if (i == 3) {
			flnm = root_dir + ds + ".." + ds + base_file;
		}
		tried_filenames.push_back(flnm);
		if (verbose)
			printf("look for file %s using search type= %s: filename %s. Called from %s %d. cked at %s %d\n",
					base_file.c_str(), search_typ[i].c_str(), flnm.c_str(), called_by_file, called_by_line, __FILE__, __LINE__);
		rc = ck_filename_exists(flnm.c_str(), called_by_file, called_by_line, verbose);
		if (rc == 0) {
			found_file = flnm;
			if (verbose)
				printf("found %s file as: %s at %s %d\n", base_file.c_str(), flnm.c_str(), __FILE__, __LINE__);
			return 0;
		}
	}
	return rc;
}
