#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <cstddef>
#include <string>
#include <regex>
#include <inttypes.h>

#include "utils2.h"
#include "utils.h"

// from https://github.com/nlohmann/json
#include "json.hpp"

//#define RD_JSON_CPP
//#include "rd_json2.h"
//#include "rd_json.h"

#include "xlsxwriter.h"
#include "xls.h"

using json = nlohmann::json;

static int tst_xls(void)
{

    /* Create a new workbook and add a worksheet. */
    lxw_workbook  *workbook  = workbook_new("demo.xlsx");
    lxw_worksheet *worksheet = workbook_add_worksheet(workbook, NULL);

    /* Add a format. */
    lxw_format *format = workbook_add_format(workbook);

    /* Set the bold property for the format */
    format_set_bold(format);

    /* Change the column width for clarity. */
    worksheet_set_column(worksheet, 0, 0, 20, NULL);

    /* Write some simple text. */
    worksheet_write_string(worksheet, 0, 0, "Hello", NULL);

    /* Text with formatting. */
    worksheet_write_string(worksheet, 1, 0, "World", format);

    /* Write some numbers. */
    worksheet_write_number(worksheet, 2, 0, 123,     NULL);
    worksheet_write_number(worksheet, 3, 0, 123.456, NULL);

    /* Insert an image. */
    //worksheet_insert_image(worksheet, 1, 2, "examples/logo.png");

    workbook_close(workbook);

    return 0;
}

int json_2_xls(std::string json_text, std::string filename, std::string from_where, const char *file, int line, int verbose)
{
	json j;
	bool worked = false;
	printf("got json_text.size()= %d at %s %d\n", (int)json_text.size(), __FILE__, __LINE__);
	try {
		j = json::parse(json_text);
		worked=true;
	} catch (json::parse_error& e) {
			// output exception information
			std::cout << "message: " << e.what() << '\n'
				  << "exception id: " << e.id << '\n'
				  << "byte position of error: " << e.byte << std::endl;
	}
	if (!worked) {
		printf("parse of json str %s failed called by %s %d. bye at %s %d\n",
				from_where.c_str(), file, line, __FILE__, __LINE__);
		fprintf(stderr, "parse of json str %s failed called by %s %d. bye at %s %d\n",
				from_where.c_str(), file, line, __FILE__, __LINE__);
		printf("Here is the string we tried to parse at %s %d:\n%s\n", __FILE__, __LINE__, json_text.c_str());
		std::cout << json_text << std::endl;
		return 1;
	}

	std::string flnm = filename;
	if (flnm.size() < 6 || flnm.substr(flnm.size()-5, flnm.size()) != ".xlsx") {
		flnm += ".xlsx";
	}
    /* Create a new workbook and add a worksheet. */
    lxw_workbook  *workbook  = workbook_new(flnm.c_str());
    lxw_worksheet *worksheet = workbook_add_worksheet(workbook, NULL);

    /* Add a format. */
    lxw_format *format = workbook_add_format(workbook);

    /* Set the bold property for the format */
    format_set_bold(format);

    /* Change the column width for clarity. */
    //worksheet_set_column(worksheet, 0, 0, 20, NULL);

    /* Write some simple text. */
    //worksheet_write_string(worksheet, 0, 0, "Hello", NULL);

    /* Text with formatting. */
    //worksheet_write_string(worksheet, 1, 0, "World", format);

    /* Write some numbers. */
    //worksheet_write_number(worksheet, 2, 0, 123,     NULL);
    //worksheet_write_number(worksheet, 3, 0, 123.456, NULL);

    /* Insert an image. */
    //worksheet_insert_image(worksheet, 1, 2, "examples/logo.png");


	uint32_t sz;
	json t_str;
	t_str = j["txt"];
	sz = t_str.size();
	std::vector <json> evt_new;
	printf("xls lines of data= %d at %s %d\n", sz, __FILE__, __LINE__);
	// line 0 desc
	// line 1 core #
	// line 2 hdr
	// line 3 hdr+core
	// line 4 vals
	int row_vals = 4;
	for (uint32_t i=0; i < sz; i++) {
		try {
    		if (i == 0) {
				worksheet_write_string(worksheet, 3, 0, "loop", NULL);
				worksheet_write_string(worksheet, 3, 1, "phase.beg", NULL);
				worksheet_write_string(worksheet, 3, 2, "phase.end", NULL);
				worksheet_write_string(worksheet, 3, 3, "T.beg", NULL);
				worksheet_write_string(worksheet, 3, 4, "T.end", NULL);
				worksheet_write_string(worksheet, 3, 5, "row", NULL);
			}
    //worksheet_write_string(worksheet, 0, 0, "Hello", NULL);
    //worksheet_write_number(worksheet, 2, 0, 123,     NULL);
			int lp = t_str[i]["lp"];
			std::string ph0 = t_str[i]["phase0"];
			std::string ph1 = t_str[i]["phase1"];
			double x0 = t_str[i]["t.abs_beg"];
			double x1 = t_str[i]["t.abs_end"];
    		worksheet_write_number(worksheet, i+row_vals, 0, lp,     NULL);
			worksheet_write_string(worksheet, i+row_vals, 1, ph0.c_str(), NULL);
			worksheet_write_string(worksheet, i+row_vals, 2, ph1.c_str(), NULL);
    		worksheet_write_number(worksheet, i+row_vals, 3, x0,     NULL);
    		worksheet_write_number(worksheet, i+row_vals, 4, x1,     NULL);
    		worksheet_write_number(worksheet, i+row_vals, 5, i,      NULL);
			int tcol = 6;

			json kv_arr = t_str[i]["key_val_arr"];
			json yvar_arr;
			//printf("lp= %d, ph0= %s, ph1= %s, x0= %f, x1= %f at %s %d\n", lp, ph0.c_str(), ph1.c_str(), x0, x1, __FILE__, __LINE__);
			std::string key, dsc;
			for (uint32_t k=0; k < kv_arr.size(); k++) {
				printf("ky= %s\n", key.c_str());
				if (i == 0) {
					key = kv_arr[k]["key"];
					dsc = kv_arr[k]["desc"];
				}
				json v_arr = kv_arr[k]["vals"];
				for (uint32_t v=0; v < v_arr.size(); v++) {
					if (i == 0) {
						worksheet_write_string(worksheet, 0, tcol+v, dsc.c_str(), NULL);
						worksheet_write_string(worksheet, 2, tcol+v, key.c_str(), NULL);
						if (v == 0) {
							yvar_arr = kv_arr[k]["yvar"];
						}
						std::string yvar = yvar_arr[v];
						worksheet_write_string(worksheet, 1, tcol+v, yvar.c_str(), NULL);
						std::string hdr_yv = key + " " + yvar;
						worksheet_write_string(worksheet, 3, tcol+v, hdr_yv.c_str(), NULL);
					}
					double val = v_arr[v];
					printf("v[%d]= %f\n", v, val);
    				worksheet_write_number(worksheet, i+row_vals, tcol+v, val,     NULL);
				}
				tcol += v_arr.size();
			}
		} catch (...) { }
	}

    workbook_close(workbook);

	return 0;
}
