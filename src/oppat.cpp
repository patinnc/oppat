/* Copyright (c) 2018 Patrick Fay
 *
 * License http://opensource.org/licenses/mit-license.php MIT License
 */

#define NOMINMAX
#ifdef _WIN32
#include <windows.h>
#define pid_t int
#endif


#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <cstddef>
#include <inttypes.h>
#include <algorithm>
#include <unordered_map>
#include <thread>
#include <functional>
#include <math.h>
#ifdef __linux__
#include <unistd.h>
#endif
#include <sstream>
#include <iomanip>
#include <regex>
#include <signal.h>
#include "perf_event_subset.h"
#include "rd_json.h"
#include "rd_json2.h"
#include "printf.h"
#define OPPAT_CPP
#include "oppat.h"
#include "prf_read_data.h"
#include "etw_parse.h"
#include "lua_rtns.h"
#include "utils2.h"
#include "utils.h"
#include "ck_nesting.h"
#include "web_api.h"
#include "MemoryMapped.h"
#include "mygetopt.h"
#include "dump.hpp"
#include "base64.h"
#include "zlib.h"
#include "tc_read_data.h"
#include "xls.h"

#pragma comment(lib, "Setupapi.lib")
#pragma comment(lib, "Advapi32.lib")
#pragma comment(lib, "User32.lib")


static int ck_bin_txt_file_opts(std::string opt_str, std::string &file_in, bool exit_on_not_found)
{
	if (file_in.size() > 0) {
		std::string flnm = file_in;
		int rc = ck_filename_exists(flnm.c_str(), __FILE__, __LINE__, options.verbose);
		if (rc != 0 && options.root_data_dirs.size() > 0) {
			flnm = options.root_data_dirs[0] + DIR_SEP + flnm;
			rc = ck_filename_exists(flnm.c_str(), __FILE__, __LINE__, options.verbose);
		}
		if (rc != 0 && exit_on_not_found) {
			fprintf(stderr, "can't find file '--%s %s'. By at %s %d\n", opt_str.c_str(), flnm.c_str(), __FILE__, __LINE__);
			exit(1);
		}
		if (rc == 0) {
			file_in = flnm;
		}
	}
	return 0;
}

static int load_long_opt_val(const char *opt_name, const char *ck_str, std::string &sv_to_str, const char *optarg)
{
	int slen0 = strlen(opt_name);
	int slen1 = strlen(ck_str);
	int slen = slen0;
	if (slen < slen1) {
		slen = slen1;
	}
	//printf("opt_name= '%s', ck_str= '%s' at %s %d\n", opt_name, ck_str, __FILE__, __LINE__);
	if (strcmp(opt_name, ck_str) == 0 || strncmp(opt_name, ck_str, slen) == 0) {
		if (!optarg) {
			fprintf(stderr, "you must enter an arg for the --%s option. bye at %s %d\n",
				opt_name,	__FILE__, __LINE__);
			exit(1);
		}
		sv_to_str = std::string(optarg);
		printf("got --%s %s at %s %d\n", opt_name, optarg, __FILE__, __LINE__);
		return 1;
	}
	return 0;
}


static uint32_t ck_if_root_points_to_file_list(std::vector <std::string> root_dir, std::string def_flnm, std::vector <path_file_str> &vec)
{
	std::string flnm = def_flnm;
	if (def_flnm.size() == 0) {
		printf("ummm... how can the default filename be len=0. Bye at %s %d\n", __FILE__, __LINE__);
		exit(1);
	}
	int rc = ck_filename_exists(def_flnm.c_str(), __FILE__, __LINE__, options.verbose);
	if (rc == 0) {
		path_file_str pfs;
		pfs.path = "";
		pfs.file = def_flnm;
		vec.push_back(pfs);
		return vec.size();
	} else if (root_dir.size() > 0) {
		for (uint32_t i=0; i < root_dir.size(); i++) {
			flnm = root_dir[i] + DIR_SEP + def_flnm;
			rc = ck_filename_exists(flnm.c_str(), __FILE__, __LINE__, options.verbose);
			if (rc == 0) {
				path_file_str pfs;
				pfs.path = root_dir[i];
				pfs.file = def_flnm;
				vec.push_back(pfs);
				printf("got file_list.json filename: %s at %s %d\n", flnm.c_str(), __FILE__, __LINE__);
			}
		}
	}
	return vec.size();
}

static int ck_if_root_points_to_data_file(std::vector <std::string> root_dir, std::string &flnm_in, std::string def_flnm)
{
	std::string flnm = flnm_in;
	int rc;
	if (flnm.size() == 0) {
		flnm = def_flnm;
		rc = ck_filename_exists(flnm.c_str(), __FILE__, __LINE__, options.verbose);
		if (rc == 0) {
			flnm_in = flnm;
		} else if (root_dir.size() > 0) {
			flnm = root_dir[0] + DIR_SEP + flnm;
			rc = ck_filename_exists(flnm.c_str(), __FILE__, __LINE__, options.verbose);
			if (rc == 0) {
				flnm_in = flnm;
			}
		}
	}
	return 0;
}

void bump_clip_valid(int &tm_clip_valid)
{
	if (tm_clip_valid == 0) {
		tm_clip_valid++;
		options.clip_mode = std::max((int)CLIP_LVL_1, options.clip_mode);
	} else if (tm_clip_valid == 1) {
		tm_clip_valid++;
		options.clip_mode = std::max((int)CLIP_LVL_2, options.clip_mode);
	}
}

static int
get_opt_main (int argc, char **argv)
{
	int c;

	int verbose_flag=0, help_flag=0, show_json=0;

	options.file_mode = FILE_MODE_FIRST;

	struct option long_options[] =
	{
		/* These options set a flag. */
		{"verbose",     no_argument,       0, 'v', "set verbose mode"
			"   Specify '-v' multiple times to increase the verbosity level.\n"
		},
		{"help",        no_argument,       &help_flag, 'h', "display help and exit"},
		{"show_json",   no_argument,        0, 's', "show json data sent to client on stdout. Can be 100s of MBs.\n"
			"   Level 1 shows 'per chart' json data. Level 2 adds the aggregated data actually sent to the client\n"
			"   Specify '-s' multiple times to increase the level.\n"
		},
		{"debug",  no_argument,   0, 0, "on windows, if the 1st arg is '--debug' then oppat will try to attach the\n"
			"   debugger to the oppat process\n"
		},
		/* These options don’t set a flag.
			We distinguish them by their indices. */
		{"beg_tm",      required_argument,   0, 'b', "begin time (absolute timestamp) to clip\n"
		   "   The begin time is the absolute time (not relative time) unless you use --marker_beg_num option too.\n"
		   "   If you use '--marker_beg_num numbr' then the time is relative to the marker's absolute time.\n"
		   "   For example:\n"
		   "     '-b 0.5 --marker_beg_num 1'  will set the begin clip time to marker number 1's absolute time - 0.5 seconds.\n"
		},
		{"marker_beg_num",      required_argument,   0, 0, "set begin clip time to marker number's absolute time"
		   "   The markers are sorted by time the first marker is marker 0\n"
		   "   If beg_tm is not entered then clip beg time is set to the marker 'num' absolute time\n"
		   "   If beg_tm is entered then clip beg time is set to the marker 'num' absolute time + beg_tm\n"
		},
		{"marker_end_num",      required_argument,   0, 0, "set end clip time to marker number's absolute time"
		   "   The markers are sorted by time the first marker is marker 0\n"
		   "   If end_tm is not entered then clip end time is set to the marker 'num' absolute time\n"
		   "   If end_tm is entered then clip end time is set to the marker 'num' absolute time + end_tm\n"
		},
		{"phase0",      required_argument,   0, 0, "select a phase by 'string'"
		   "   The phases are a subset of the markers. They have the strings 'end phase .*some_text.* dura= num .*'\n"
		   "   Enter the string you want to look for in the 'some_text' part of the marker text\n"
		   "   If end_tm used is the 'end phase' marker time and the beg_tm is end_tm - dura.\n"
		},
		{"phase1",     required_argument,   0, 0, "select an ending phase by 'string'"
		   "   If you select a '--phase0 phs_num0' and '--phase1 phs_num1' then the starting time is the start\n"
		   "   of phs_num0 and the ending time is the end of phs_num1.\n"
		   "   if --phase1 is not specified and --ph_step_int not specified then the start time is the start\n"
		   "   of phs_num0 and the end time is the end of phs_num0.\n"
		},
		{"ph_step_int",      required_argument,   0, 0, "start at above phase, step by interval and save canvas"
		   "   Arg to ph_step_int is step_sz[,tms_to_step] where step_sz is size in seconds and tms_to_step is integer\n"
		   "   for how many times to shift interval. This is intended for making gifs.\n"
		   "   So '--phase_step_int 0.1,10' would start at above phase but use interval 0.1 secs, draw the canvas\n"
		   "   then increment the beg,end tm by 0.1 secs and redraw for 9 more times.\n"
		   "   See --ph_image below for option to create gif files.\n"
		   "   If tms_to_step is not entered then number of intervals ('end time' - 'beg time')/step_sz.\n"
		   "   Contrast this with --by_phase below.\n"
		},
		{"follow_proc",      required_argument,   0, 0, "follow process name\n"
		   "   Arg to follow_proc is a process name. Enclose in quotes if contains spaces.\n"
		   "   Use this to get a graph of the percent of time this process occupies each CPU.\n"
		   "   for example '--follow_proc geekbench' will give you a chart showing the pct time geekbench is running on each cpu.\n"
		   "   This is useful for benchmarks and phases. In particular with the cpu_digram, for the single threaded case,\n"
		   "   to make sense of the 'per core' charts, you need to know if the benchmark stayed on 1 cpu or moved around..\n"
		},
		{"by_phase",      no_argument,   0, 0, "step by full phase instead of by time step."
		   "   1st draw the whole of phase0, then the whole of the next phase, until you get to the end phase.\n"
		   "   This is different that --ph_step_int which steps from phase0 to phase1 by a time step.\n"
		},
		{"skip_phases_with_string",      required_argument,   0, 0, "skip phases if the phase text contains string."
		   "   When generating PNGs using the --by_phase, there may be phases you want to skip.\n"
		   "   For example, the way I generate Geekbench4 phases, there is an idle period between each sub-test.\n"
		   "   OPPAT looks for the idle time and inserts a phase with the text 'idle: X' after sub-test X.\n"
		   "   So to skip the idle phases, use '--skip_phases_with_string idle:'\n"
		},
		{"ph_image",      required_argument,   0, 0, "save cpu_diagram canvas using ph_image filename"
		   "   Arg to ph_image is a filename. The cpu_diagram canvas will be converted to png (after ph_step_int actions)\n"
		   "   and then sent to oppat and saved to filename. A number will be appended to the filename.\n"
		   "   So '--ph_image c:\\tmp\\cpu_diag_' will create c:\\tmp\\cpu_diag_000.png, c:\\tmp\\cpu_diag_001.png etc\n"
		},
		{"img_wxh_pxls",      required_argument,   0, 0, "arg is WxH to generate PNG at WxH pixels"
		   "   Arg to img_sz is a string WIDTHxHEIGHT where width and height are pixels.\n"
		   "   The default is 1017x1388. This option is only used if ph_image is used.\n"
		   "   So '--img_wxh_pxls 1000x1200' will create a canvas 1000 pixels wide by 1200 high from which PNGs will get generated\n"
		},
		{"j2x",      required_argument,   0, 0, "save json_table.json to j2x filename"
		   "   Arg to j2x is a filename. The cpu_diagram process generates a json_table.json file.\n"
		   "   Use this option to convert the json_table.json file to the j2x 'filename'.\n"
		   "   So '--j2x pat' will create pat.xlsx containing an excel data\n"
		},
		{"charts",      required_argument,   0, 'c', "json list of charts"},
		{"data_files",  required_argument,   0, 'd', "json list of data files\n"
		   "   By default the file is input_files/input_data_files.json and each data dir you\n"
		   "   want to use should be in this list. You can select a dir from the list by using\n"
		   "   the '-u file_tag' option. The -u option selects only those files from the list\n"
		   "   which has the 'file_tag'.\n"
		   "   Alternatively, you can use the '-r root_data_dir' option to point directly to\n"
		   "   the data dir you want to use. There should be a file_list.json file in the data dir\n"
		   "   which lists the filename and types of files in the data dir.\n"
		},
		{"end_tm",      required_argument,   0, 'e', "end time (absolute) to clip"},
		{"etw_txt", required_argument,   0, 0, "ETW txt data filename\n"
		   "   If you Use this option then input_files/input_data_files.json will not be read\n"
		   "   and you will have to enter all the files using cmdline options.\n"
		   "   The root_data option can be used to provide a path to the file.\n"
		   "   This option corresponds to txt_file field with type:ETW in the input json file.\n"
		},
		{"load",        required_argument,   0, 0, "load replay web data from filename\n"
		   "   Default is don't load a file. This option (with the '--save filename' options) allows just the web data to be replayed.\n"
		   "   So you can save the data and later load it and replay it on the browser.\n"
		   "   This allows you to replay the data in the browser without having to have all perf/xperf/trace-cmd/etc files.\n"
		},
		{"lua_bin",  required_argument,   0, 0, "lua bin energy data filename\n"
		   "   If you Use this option then input_files/input_data_files.json will not be read\n"
		   "   and you will have to enter all the files using cmdline options.\n"
		   "   The root_data option can be used to provide a path to the file.\n"
		   "   This option corresponds to bin_file field with type:LUA in the input json file.\n"
		},
		{"lua_txt", required_argument,   0, 0, "lua txt energy2 data filename\n"
		   "   If you Use this option then input_files/input_data_files.json will not be read\n"
		   "   and you will have to enter all the files using cmdline options.\n"
		   "   The root_data option can be used to provide a path to the file.\n"
		   "   This option corresponds to txt_file field with type:LUA in the input json file.\n"
		},
		{"lua_wait",    required_argument,   0, 0, "lua wait text filename\n"
		   "   If you Use this option then input_files/input_data_files.json will not be read\n"
		   "   and you will have to enter all the files using cmdline options.\n"
		   "   The root_data option can be used to provide a path to the file.\n"
		   "   This option corresponds to wait_file field with type:LUA in the input json file.\n"
		},
		{"mode_for_files",  required_argument,   0, 'm', "mode to use for data files\n"
		   "   modes can be: 'first': use first file tag group in data file.\n"
		   "                 'last':  use last  file tag group in data file.\n"
		   "                 'tag':   require user to enter 'use_tag' option to identify file groups to use.\n"
		},
		{"perf_bin",        required_argument,   0, 0, "perf binary data filename\n"
		   "   If you Use this option then input_files/input_data_files.json will not be read\n"
		   "   and you will have to enter all the files using cmdline options.\n"
		   "   The root_data option can be used to provide a path to the file.\n"
		   "   This option corresponds to bin_file field with type:PERF in the input json file.\n"
		},
		{"perf_txt",        required_argument,   0, 0, "perf text data filename\n"
		   "   If you Use this option then input_files/input_data_files.json will not be read\n"
		   "   and you will have to enter all the files using cmdline options.\n"
		   "   The root_data option can be used to provide a path to the file.\n"
		   "   This option corresponds to txt_file field with type:PERF in the input json file.\n"
		},
		{"port",        required_argument,   0, 'p', "port number to which to connect browser\n"
		   "   Default port is '8081'. This port number may already be in use by another web server.\n"
		   "   This option lets you change the port number.\n"
		   "   Connect you browser to http://localhost:8081 (if you are using the default port).\n"
		},
		{"root_data",   required_argument,   0, 'r', "root dir of data files.\n"
		   "   This overrides the 'root_dir' variable in the input_files/input_data_files.json file.\n"
		   "   This is useful if you move from one machine to another.\n"
		   "   Alternatively, this option also allows you to directly select a data dir to use.\n"
		   "   If you point directly to a data dir then OPPAT expects there to be a file_list.json\n"
		   "   file in the data dir indicating the list of files used and the type of each file.\n"
		   "   You can specify multiple -r options if you want to compare the files in one dir vs another dir.\n"
		},
		{"save",        required_argument,   0, 0, "save web data to replay filename\n"
		   "   Default is don't save the data. This option allows just the web data to be saved.\n"
		   "   Then you can load it and send it later with the '--load filename' option.\n"
		   "   This allows you to replay the data in the browser without having to have all perf/xperf/trace-cmd/etc files.\n"
		   "   Only the charts and events defined during the '--save' run will be in the replay file.\n"
		},
		{"tc_bin",        required_argument,   0, 0, "trace_cmd binary data filename\n"
		   "   If you Use this option then input_files/input_data_files.json will not be read\n"
		   "   and you will have to enter all the files using cmdline options.\n"
		   "   The root_data option can be used to provide a path to the file.\n"
		   "   This option corresponds to bin_file field with type:TRACE_CMD in the input json file.\n"
		},
		{"tc_txt",        required_argument,   0, 0, "trace_cmd text data filename\n"
		   "   If you Use this option then input_files/input_data_files.json will not be read\n"
		   "   and you will have to enter all the files using cmdline options.\n"
		   "   The root_data option can be used to provide a path to the file.\n"
		   "   This option corresponds to txt_file field with type:TRACE_CMD in the input json file.\n"
		},
		{"use_tag",     required_argument,   0, 'u', "use only files with file this file_tag\n"
		   "   If you want to select multiple file group, use this option more than once... for example:\n"
		   "      -u tag1 -u tag2  # to select files with file_tag 'tag1' and files with file_tag 'tag2'.\n"
		},
		{"cpu_diagram", required_argument,   0, 0, "cpu block diagram SVG file\n"
		   "   Default is don't use a cpu block diagram SVG file.\n"
		   "   You have to have a diagram like ./web/haswell_block_diagram.svg.\n"
		   "   The SVG file is CPU specific and you need to specify the right events to be put on the diagram.\n"
		   "   See scripts/run_perf_x86_haswell.sh for a haswell specific events.\n"
		},
		{"web_file",    required_argument,   0, 0, "create standalone html web page named web_file\n"
		   "   Default is don't create web_file.\n"
		   "   This option creates a standalone web file that can be shared with other users without installing OPPAT.\n"
		   "   You can either read the perf/xperf/trace-cmd data files and create the web page\n"
		   "   or you can --load a replay file and create the web_file.\n"
		},
		{"web_fl_quit",    required_argument,   0, 0, "same as web_file option above but quit after making html file\n"
		   "   Default is don't quit after generating web_file and enter the 'wait for browser' loop.\n"
		},
		{0, 0, 0, 0}
	};

	while (1)
	{
		/* getopt_long stores the option index here. */
		int option_index = 0;

		c = mygetopt_long(argc, argv, "b:c:d:e:hm:pr:su:v", long_options, &option_index);

		/* Detect the end of the options. */
		if (c == -1)
			break;

		switch (c)
		{
		case 0:
			if (load_long_opt_val(long_options[option_index].name, "etw_txt", options.etw_txt, optarg) > 0) {
				break;
			}
			if (load_long_opt_val(long_options[option_index].name, "load", options.replay_filename, optarg) > 0) {
				if (options.replay_filename.size() == 0 ||
					ck_filename_exists(options.replay_filename.c_str(), __FILE__, __LINE__, options.verbose) != 0) {
					fprintf(stderr, "You entered '--load %s' but file is not found. Bye at %s %d\n",
							optarg, __FILE__, __LINE__);
					exit(1);
				}
				options.load_replay_file = true;
				break;
			}
			if (load_long_opt_val(long_options[option_index].name, "lua_bin", options.lua_energy, optarg) > 0) {
				break;
			}
			if (load_long_opt_val(long_options[option_index].name, "lua_txt", options.lua_energy2, optarg) > 0) {
				break;
			}
			if (load_long_opt_val(long_options[option_index].name, "lua_wait", options.lua_wait, optarg) > 0) {
				break;
			}
			if (load_long_opt_val(long_options[option_index].name, "perf_bin", options.perf_bin, optarg) > 0) {
				break;
			}
			if (load_long_opt_val(long_options[option_index].name, "perf_txt", options.perf_txt, optarg) > 0) {
				break;
			}
			if (load_long_opt_val(long_options[option_index].name, "save", options.replay_filename, optarg) > 0) {
				break;
			}
			if (load_long_opt_val(long_options[option_index].name, "tc_bin", options.tc_bin, optarg) > 0) {
				break;
			}
			if (load_long_opt_val(long_options[option_index].name, "tc_txt", options.tc_txt, optarg) > 0) {
				break;
			}
			if (load_long_opt_val(long_options[option_index].name, "cpu_diagram", options.cpu_diagram, optarg) > 0) {
				std::string svg_file = optarg;
				size_t pos;
				pos = svg_file.find_last_of(".");
				if (pos == std::string::npos) {
					printf("messed up cpu_diagram filename= %s. didn't find '.' in flnm. bye at %s %d\n", options.cpu_diagram.c_str(), __FILE__, __LINE__);
					exit(1);
				}
				std::string sfx = svg_file.substr(pos+1, svg_file.size());
				printf("svg_file sfx= %s at %s %d\n", sfx.c_str(), __FILE__, __LINE__);
				if (sfx != "svg") {
					printf("svg_file sfx != 'svg', got %s, changing it to svg at %s %d\n", sfx.c_str(), __FILE__, __LINE__);
					svg_file = svg_file.substr(0, pos) + ".svg";
					options.cpu_diagram = svg_file;
				}
				int rc = ck_filename_exists(options.cpu_diagram.c_str(), __FILE__, __LINE__, options.verbose);
				if (rc != 0) {
					printf("didn't find --cpu_diagram %s file at %s %d\n",
						options.cpu_diagram.c_str(), __FILE__, __LINE__);
					exit(1);
				}
				printf("cmdline arg: --cpu_diagram %s at %s %d\n", options.cpu_diagram.c_str(), __FILE__, __LINE__);
				break;
			}
			if (load_long_opt_val(long_options[option_index].name, "web_file", options.web_file, optarg) > 0) {
				break;
			}
			if (load_long_opt_val(long_options[option_index].name, "web_fl_quit", options.web_file, optarg) > 0) {
				options.web_file_quit = true;
				break;
			}
			if (strcmp(long_options[option_index].name, "beg_tm") == 0) {
				options.tm_clip_beg = atof(optarg);
				printf ("option -beg_tm with value `%s', %f\n", optarg, options.tm_clip_beg);
				bump_clip_valid(options.tm_clip_beg_valid);
				break;
			}
			if (strcmp(long_options[option_index].name, "marker_beg_num") == 0) {
				options.marker_beg_num = atoi(optarg);
				printf ("option -marker_beg_num with value `%s', %d\n", optarg, options.marker_beg_num);
				options.clip_mode = CLIP_LVL_2;
				options.tm_clip_beg_valid = CLIP_LVL_2;
				break;
			}
			if (strcmp(long_options[option_index].name, "phase0") == 0) {
				options.phase.push_back(optarg);
				printf ("option --phase0 %s\n", optarg);
				break;
			}
			if (strcmp(long_options[option_index].name, "phase1") == 0) {
				options.phase_end.push_back(optarg);
				printf ("option --phase1 %s\n", optarg);
				break;
			}
			if (strcmp(long_options[option_index].name, "ph_step_int") == 0) {
				options.ph_step_int.push_back(optarg);
				printf ("option --ph_step_int %s\n", optarg);
				break;
			}
			if (strcmp(long_options[option_index].name, "follow_proc") == 0) {
				options.follow_proc.push_back(optarg);
				printf ("option --follow_proc %s\n", optarg);
				break;
			}
			if (strcmp(long_options[option_index].name, "by_phase") == 0) {
				options.by_phase.push_back("1");
				printf ("option --by_phase\n");
				break;
			}
			if (strcmp(long_options[option_index].name, "skip_phases_with_string") == 0) {
				options.skip_phases_with_string.push_back(optarg);
				printf ("option --%s %s\n", long_options[option_index].name, optarg);
				break;
			}
			if (strcmp(long_options[option_index].name, "ph_image") == 0) {
				options.ph_image.push_back(optarg);
				printf ("option --ph_image %s\n", optarg);
				break;
			}
			if (strcmp(long_options[option_index].name, "img_wxh_pxls") == 0) {
				options.img_wxh_pxls.push_back(optarg);
				if (options.img_wxh_pxls.back().find("x") == std::string::npos &&
					options.img_wxh_pxls.back().find(",") == std::string::npos) {
					fprintf(stderr, "Expected '--img_wxh_pxls %s' to have 'x' or ',' in width,height arg. Bye at %s %d\n",
							optarg, __FILE__, __LINE__);
					exit(1);
				}
				printf ("option --img_wxh_pxls %s\n", optarg);
				break;
			}
			if (strcmp(long_options[option_index].name, "j2x") == 0) {
				options.j2x = optarg;
				printf ("option --j2x %s\n", optarg);
				break;
			}
			if (strcmp(long_options[option_index].name, "end_tm") == 0) {
				options.tm_clip_end = atof(optarg);
				printf ("option -end_tm with value `%s', %f\n", optarg, options.tm_clip_beg);
				bump_clip_valid(options.tm_clip_end_valid);
				break;
			}
			if (strcmp(long_options[option_index].name, "marker_end_num") == 0) {
				options.marker_end_num = atoi(optarg);
				printf ("option -marker_end_num with value `%s', %d\n", optarg, options.marker_end_num);
				options.clip_mode = CLIP_LVL_2;
				options.tm_clip_end_valid = CLIP_LVL_2;
				break;
			}
			/* If this option set a flag, do nothing else now. */
			if (long_options[option_index].flag != 0)
				break;
			printf ("option %s", long_options[option_index].name);
			if (optarg)
				printf (" with arg %s", optarg);
			printf ("\n");
			break;

		case 'b':
			printf ("option -b with value `%s'\n", optarg);
			options.tm_clip_beg = atof(optarg);
			bump_clip_valid(options.tm_clip_beg_valid);
			break;

		case 'c':
			printf ("option -c with value `%s'\n", optarg);
			options.chart_file = optarg;
			break;

		case 'd':
			printf ("option -d with value `%s'\n", optarg);
			options.data_file = optarg;
			break;

		case 'e':
			printf ("option -e with value `%s'\n", optarg);
			options.tm_clip_end = atof(optarg);
			bump_clip_valid(options.tm_clip_end_valid);
			break;

		case 'h':
			printf ("option -h set\n");
			help_flag++;
			break;

		case 'm':
			printf ("option -m with value `%s'\n", optarg);
			if (strcmp(optarg, "first") == 0) {
				options.file_mode = FILE_MODE_FIRST;
			} else if (strcmp(optarg, "last") == 0) {
				options.file_mode = FILE_MODE_LAST;
			} else if (strcmp(optarg, "all") == 0) {
				options.file_mode = FILE_MODE_ALL;
			} else {
				printf("got invalid option '-m %s\n", optarg);
				printf("valid args to -m (--mode_for_files) are: 'first', 'last', 'all'. Bye at %s %d\n", __FILE__, __LINE__);
				exit(1);
			}
			break;

		case 'p':
			printf ("option -p with value `%s'\n", optarg);
			options.web_port = atoi(optarg);
			break;

		case 'r':
			printf ("option -r with value `%s'\n", optarg);
			options.root_data_dirs.push_back(std::string(optarg));
			break;

		case 's':
			options.show_json++;
			printf ("option -s set to %d\n", options.show_json);
			break;

		case 'u':
			printf ("option -u with value `%s'\n", optarg);
			options.file_tag_vec.push_back(optarg);
			break;

		case 'v':
			options.verbose++;
			printf ("option -v set to %d\n", options.verbose);
			break;

		case '?':
			/* getopt_long already printed an error message. */
			break;

		default:
			abort ();
		}
	}
	printf("help_flag = %d at %s %d\n", help_flag, __FILE__, __LINE__);

	/* Print any remaining command line arguments (not options). */
	if (myoptind < argc) {
		printf ("non-option ARGV-elements: ");
		while (myoptind < argc)
			printf ("%s ", argv[myoptind++]);
		putchar ('\n');
		printf("got invalid command line options above ------------ going to print options and exit\n");
		fprintf(stderr, "got invalid command line options above ------------ going to print options and exit\n");
		help_flag = 1;
	}

	if (help_flag) {
		uint32_t i = 0;
		printf("options:\n");
		std::vector <std::string> args= {"no_args", "requires_arg", "optional_arg"};
		while (true) {
			if (!long_options[i].name) {
				break;
			}
			if (long_options[i].has_arg < no_argument ||
				long_options[i].has_arg > optional_argument) {
				printf("invalid 'has_arg' value(%d) in long_options list. Bye at %s %d\n",
					long_options[i].has_arg, __FILE__, __LINE__);
				exit(1);
			}
			std::string str = "";
			if (long_options[i].val > 0) {
				char cstr[10];
				snprintf(cstr, sizeof(cstr), "%c", long_options[i].val);
				str = "-" + std::string(cstr) + ",";
			}
			printf("--%s\t%s requires_arg? %s\n\t%s\n",
					long_options[i].name, str.c_str(),
					args[long_options[i].has_arg-1].c_str(),
					long_options[i].help);
			i++;
		}
		printf("Bye at %s %d\n", __FILE__, __LINE__);
		exit(1);
	}

	ck_if_root_points_to_file_list(options.root_data_dirs, "file_list.json", options.file_list);

	printf("options: tm_clip_beg= %f, beg_valid= %d, end= %f, end_valid= %d at %s %d\n",
		options.tm_clip_beg, options.tm_clip_beg_valid,
		options.tm_clip_end, options.tm_clip_end_valid,
		__FILE__, __LINE__);

	if (options.by_phase.size() > 0 && (options.phase.size() == 0 || options.phase_end.size() == 0)) {
		fprintf(stderr, "you entered --by_phase but you didn't enter --phase0 or --phase1. bye at %s %d\n", __FILE__, __LINE__);
		exit(1);
	}

	if (options.file_list.size() > 0) {
		// everything in this routine below this point should be for just filling in list of files
		return 0;
	}

	// allow user to enter just the root_dir option pointing directly to a data dir... and then check for which files are there
	ck_if_root_points_to_data_file(options.root_data_dirs, options.etw_txt,     "etw_trace.txt");
	ck_if_root_points_to_data_file(options.root_data_dirs, options.perf_bin,    "prf_trace.data");
	ck_if_root_points_to_data_file(options.root_data_dirs, options.perf_txt,    "prf_trace.txt");
	ck_if_root_points_to_data_file(options.root_data_dirs, options.tc_bin,      "tc_trace.dat");
	ck_if_root_points_to_data_file(options.root_data_dirs, options.tc_txt,      "tc_trace.txt");
	ck_if_root_points_to_data_file(options.root_data_dirs, options.lua_energy,  "prf_energy.txt");
	ck_if_root_points_to_data_file(options.root_data_dirs, options.lua_energy2, "prf_energy2.txt");
	ck_if_root_points_to_data_file(options.root_data_dirs, options.lua_energy2, "etw_energy2.txt");
	ck_if_root_points_to_data_file(options.root_data_dirs, options.lua_wait,    "wait.txt");

	ck_bin_txt_file_opts("etw_txt", options.etw_txt, true);
	ck_bin_txt_file_opts("perf_bin", options.perf_bin, true);
	ck_bin_txt_file_opts("perf_txt", options.perf_txt, true);
	ck_bin_txt_file_opts("tc_bin", options.tc_bin, true);
	ck_bin_txt_file_opts("tc_txt", options.tc_txt, true);
	ck_bin_txt_file_opts("lua_energy", options.lua_energy, true);
	ck_bin_txt_file_opts("lua_energy2", options.lua_energy2, true);
	ck_bin_txt_file_opts("lua_wait", options.lua_wait, true);

	if ((options.perf_bin.size() >  0 && options.perf_txt.size() == 0) ||
		(options.perf_bin.size() == 0 && options.perf_txt.size() > 0)) {
		fprintf(stderr, "both --perf_bin and --perf_txt must be entered if one is entered. bye at %s %d\n", __FILE__, __LINE__);
		exit(1);
	}
	if ((options.tc_bin.size() >  0 && options.tc_txt.size() == 0) ||
		(options.tc_bin.size() == 0 && options.tc_txt.size() > 0)) {
		fprintf(stderr, "both --tc_bin and --tc_txt must be entered if one is entered. bye at %s %d\n", __FILE__, __LINE__);
		exit(1);
	}
	if (options.etw_txt.size() == 0) {
		if (options.lua_energy.size() > 0 || options.lua_energy2.size() > 0 || options.lua_wait.size() > 0) {
			if (options.lua_energy.size() == 0 || options.lua_energy2.size() == 0 || options.lua_wait.size() == 0) {
				fprintf(stderr, "if you enter any lua file you must enter all 3: --lua_energy and --lua_energy2 and --lua_wait filenames. bye at %s %d\n",
						__FILE__, __LINE__);
				exit(1);
			}
		}
	} else {
		if (options.lua_energy.size() > 0 || options.lua_energy2.size() > 0 || options.lua_wait.size() > 0) {
			if (options.lua_energy2.size() == 0 || options.lua_wait.size() == 0) {
				fprintf(stderr,
					"if you enter any lua file you must enter at least: --lua_energy2 and --lua_wait filenames. bye at %s %d\n",
					__FILE__, __LINE__);
				exit(1);
			}
		}
	}

	return 0;
}

std::unordered_map<std::string, uint32_t> file_tag_hash;
std::vector <std::string> file_tag_vec;

std::vector <std::unordered_map<std::string, int>> flnm_evt_hash;
std::vector <std::vector <flnm_evt_str>> flnm_evt_vec;
std::vector <std::unordered_map<std::string, int>> comm_pid_tid_hash;
std::vector <std::vector <comm_pid_tid_str>> comm_pid_tid_vec;

struct marker_str {
	double ts_abs, dura;
	std::string text, evt_name;
	bool beg, end;
	int prf_obj_idx, evt_idx_in_po, file_tag_idx, zoom_to, zoom_end, prf_sample_idx;
	marker_str(): ts_abs(0.0), dura(0.0), beg(false), end(false),
		prf_obj_idx(-1), evt_idx_in_po(-1), file_tag_idx(-1), zoom_to(0), zoom_end(0), prf_sample_idx(-1) {}
};

static std::vector <marker_str> marker_vec, phase_vec;

struct shape_str {
	double x, y;
	std::string text;
	int pid, typ, cat, subcat;
	shape_str(): pid(-1), typ(-1) {}
};

struct lines_str {
	double x[2], y[2], period, numerator, denom;
	std::string text;
        std::vector <int> text_arr;
	int cpt_idx, fe_idx, pid, prf_idx, typ, cat, subcat, cpu, use_num_denom, csi;
	uint32_t flags;
	std::vector <int> callstack_str_idxs;
	lines_str(): period(0.0), numerator(0.0), denom(0.0), cpt_idx(-1), fe_idx(-1), pid(-1), prf_idx(-1),
		typ(-1), cat(-1), subcat(-1), cpu(-1), use_num_denom(-1), csi(-1), flags(0) {}
};

// order of SHAPE_* enum values must agree with SHAPE_* variables in main.js
// The FOLLOW_PROC 
enum {
	SHAPE_LINE   = 0x01,
	SHAPE_RECT   = 0x02,
	SHAPE_FOLLOW = 0x04,
	SHAPE_MAX    = 0x07,
};

struct min_max_str {
	double x0, x1, y0, y1, total, tot_dura;
	int fe_idx;
	bool initd;
	min_max_str(): x0(-1.0), x1(-1.0), y0(-1.0), y1(-1.0), total(0.0), tot_dura(0.0), fe_idx(-1), initd(false) {}
};

struct chart_lines_rng_str {
	double x_range[2], y_range[2], ts0, ts0x;
	std::vector <std::string> cat;
	std::vector <min_max_str> cat_rng;
	std::vector <std::vector <min_max_str>> subcat_rng;
	std::vector <bool> cat_initd;
	std::vector <std::vector <bool>> subcat_initd;
	std::vector <std::vector <std::string>> sub_cat;
	chart_lines_rng_str(): ts0(-1) {}
};

struct ch_lines_str {
	chart_lines_rng_str range;
	std::vector <std::vector <std::string>> subcats;
	std::vector <std::string> legend;
	std::vector <lines_str> line;
	double tm_beg_offset_due_to_clip;
	double tm_end_offset_due_to_clip;
	prf_obj_str *prf_obj;
	ch_lines_str(): tm_beg_offset_due_to_clip(0.0), tm_end_offset_due_to_clip(0.0), prf_obj(0) {}
};

static ch_lines_str ch_lines;

void prf_add_comm(uint32_t pid, uint32_t tid, std::string comm, prf_obj_str &prf_obj, double tm);

#define BUF_MAX 4096
static char buf[BUF_MAX];

static void sighandler(int sig)
{
	set_signal();
}

bool compareByTime(const prf_samples_str &a, const prf_samples_str &b)
{
	if (a.ts == b.ts) {
		return a.orig_order < b.orig_order;
	}
	return a.ts < b.ts;
}

static bool compareByChTime(const lines_str &a, const lines_str &b)
{
	return a.x[0] < b.x[0];
}

uint16_t *buf_uint16_ptr(char *buf, int off) {
	return (uint16_t *)(buf+off);
}

int32_t *buf_int32_ptr(char *buf, int off) {
	return (int32_t *)(buf+off);
}

uint32_t *buf_uint32_ptr(char *buf, int off) {
	return (uint32_t *)(buf+off);
}

// Below probably works on windows due to only having lower 32bits being occupied.
// TBD I should have better code to handle 'long' on windows.
// Might fail for large (bigger than 2 GBs) files... but I think the amount of data will
// overwhelm the charting before that.
long *buf_long_ptr(char *buf, int off) {
	return (long *)(buf+off);
}

uint64_t *buf_uint64_ptr(char *buf, int off) {
	return (uint64_t *)(buf+off);
}

std::string do_string_with_decimals(double dval, const int n = 6)
{
	std::ostringstream ostr;
	ostr << std::fixed << std::setprecision(n) << dval;
	return ostr.str();
}

int read_till_null(std::ifstream &file, long &pos, int line)
{
	int i=0;
	while (1) {
		if (i >= BUF_MAX) {
			printf("got err on read wth i= %d. called from line= %d at %s %d\n", i, line, __FILE__, __LINE__);
			//return -1;
			exit(1);
		}
		if(!file.read (buf+i, 1)) {
			printf("got err on read wth i= %d. called from line= %d at %s %d\n", i, line, __FILE__, __LINE__);
			exit(1);
		}
		if (file.gcount() != 1) {
			printf("got err on read wth i= %d. called from line= %d at %s %d\n", i, line, __FILE__, __LINE__);
			exit(1);
		}
		if (buf[i] == 0) {
			break;
		}
		i++;
		pos++;
	}
	return i;
}

int mm_read_till_null(const unsigned char *mm_buf, long &pos, int line, char *buf, int buf_sz)
{
	int i=0;
	while (1) {
		if (i >= buf_sz) {
			printf("got err on read wth i= %d. called from line= %d at %s %d\n", i, line, __FILE__, __LINE__);
			//return -1;
			exit(1);
		}
		memcpy(buf+i, mm_buf+pos, 1);
		pos++;
		if (buf[i] == 0) {
			break;
		}
		i++;
	}
	return i;
}

int mm_read_n_bytes(const unsigned char *mm_buf, long &pos, int n, int line, char *buf, int buf_sz)
{
	if (n >= buf_sz) {
		printf("got err on read wth i= %d. called from line= %d at %s %d\n", n, line, __FILE__, __LINE__);
		exit(1);
	}
	memcpy(buf, mm_buf+pos, (size_t)n);
	pos += n;
	return n;
}

int mm_read_n_bytes_buf(const unsigned char *mm_buf, long &pos, int n, char *nw_buf, int line)
{
	memcpy(nw_buf, mm_buf+pos, (size_t)n);
	pos += n;
	return n;
}

int read_n_bytes(std::ifstream &file, long &pos, int n, int line, char *buf, int buf_sz)
{
	if (n >= buf_sz) {
		printf("got err on read wth i= %d. called from line= %d at %s %d\n", n, line, __FILE__, __LINE__);
		exit(1);
	}
	if(!file.read (buf, n)) {
		printf("got err on read wth n= %d from line= %d at %s %d\n", n, line, __FILE__, __LINE__);
		exit(1);
	}
	if (file.gcount() != n) {
		printf("got err on read wth n= %d. called from line= %d at %s %d\n", n, line, __FILE__, __LINE__);
		exit(1);
	}
	pos += n;
	return n;
}

int read_n_bytes_buf(std::ifstream &file, long &pos, int n, char *nw_buf, int line)
{
	if(!file.read (nw_buf, n)) {
		printf("got err on read wth n= %d at %s %d\n", n, __FILE__, __LINE__);
		exit(1);
	}
	if (file.gcount() != n) {
		printf("got err on read wth n= %d. called from line= %d at %s %d\n", n, line, __FILE__, __LINE__);
		exit(1);
	}
	pos += n;
	return n;
}

int hex_dump_n_bytes_from_buf(char *buf_in, long sz, std::string pref, int line)
{
	std::vector<uint8_t> data1;
	data1.clear();
	for (int k=0; k < sz; k++) {
		data1.push_back((uint8_t)buf_in[k]);
	}
	hex::dump hex(data1);
	std::stringstream ss;
	ss << hex;
	printf("%s:\n", pref.c_str());
	std::cout << ss.str() << std::endl;
	return sz;
}

int hex_dump_bytes(std::ifstream &file, long &pos_in, long sz, std::string pref, int line, char *buf, int buf_sz)
{
	long pos = pos_in;
	file.seekg(pos_in);
	read_n_bytes(file, pos, sz, __LINE__, buf, buf_sz);
	file.seekg(pos_in);
	hex_dump_n_bytes_from_buf(buf, sz, pref, line);
	return sz;
}

void _putchar(char character)
{
	putc((int)character, stdout);
}

static std::unordered_map<std::string, uint32_t> callstack_hash;
static std::vector <std::string> callstack_vec;

static uint32_t idle_cpt_idx_m1 = UINT32_M1;

uint32_t hash_comm_pid_tid(std::unordered_map<std::string, int> &hsh_str, std::vector <comm_pid_tid_str> &vec_str, std::string &comm, int pid, int tid)
{
	std::string str = comm + " " + std::to_string(pid) + "/" + std::to_string(tid);
	int idx = hsh_str[str];
	if (idx == 0) {
		comm_pid_tid_str cpts;
		cpts.comm = comm;
		cpts.pid = pid;
		cpts.tid = tid;
		vec_str.push_back(cpts);
		idx = hsh_str[str] = (int32_t)vec_str.size(); // so we are storing vec_str idx+1
		if (pid == 0 && tid == 0) {
			idle_cpt_idx_m1 = idx - 1;
		}
	}
	return (uint32_t)idx;
}

static uint32_t hash_flnm_evt(std::unordered_map<std::string, int> &hsh_str, std::vector <flnm_evt_str> &vec_str, std::string str, std::string flnm_bin, std::string flnm_text, std::string evt, uint32_t prf_obj_idx, uint32_t prf_evt_idx, uint32_t evt_tbl_evt_idx)
{
	int idx = hsh_str[str];
	if (idx == 0) {
		flnm_evt_str fes;
		fes.filename_bin  = flnm_bin;
		fes.filename_text = flnm_text;
		fes.evt_str = evt;
		fes.prf_obj_idx = prf_obj_idx;
		fes.prf_evt_idx = prf_evt_idx;
		fes.evt_tbl_evt_idx = evt_tbl_evt_idx;
		vec_str.push_back(fes);
		idx = (int32_t)vec_str.size(); // so we are storing vec_str idx+1
		hsh_str[str] = idx;
	}
	return (uint32_t)idx;
}

uint32_t get_fe_idxm1(prf_obj_str &prf_obj, std::string event_name_w_area, int line, bool exit_on_err, evt_str &event_table)
{
	std::string str = prf_obj.filename_bin + " " + prf_obj.filename_text + " " + event_name_w_area;
	uint32_t file_tag_idx = prf_obj.file_tag_idx;
	if (file_tag_idx == UINT32_M1) {
		printf("ummm... ck yer code dude... file_tag_idx= %d at %s %d\n", file_tag_idx, __FILE__, __LINE__);
		exit(1);
	}
	uint32_t fe_idx = flnm_evt_hash[file_tag_idx][str];
	if (fe_idx == 0 && exit_on_err) {
		printf("messed up fe_idx lkup. str= '%s'. called from line %d. Bye at %s %d\n",
				str.c_str(), line, __FILE__, __LINE__);
		exit(1);
	}
	return fe_idx - 1;
}

static uint32_t hash_dbl(std::unordered_map <double, uint32_t> &hsh_dbl, std::vector <double> &vec_dbl, double by_var_dbl, std::vector <double> &sub_tot, double var_dbl)
{
	uint32_t idx = hsh_dbl[by_var_dbl];
	if (idx == 0) {
		vec_dbl.push_back(by_var_dbl);
		idx = hsh_dbl[by_var_dbl] = (uint32_t)vec_dbl.size(); // so we are storing vec_str idx+1
		sub_tot.push_back(var_dbl);
	} else {
		sub_tot[idx-1] += var_dbl;
	}
	return idx;
}

static uint32_t hash_dbl_by_var(std::unordered_map <uint32_t, uint32_t> &hsh_u32, std::vector <double> &vec_dbl, double by_var_dbl, std::vector <double> &sub_tot, double var_dbl)
{
	uint32_t val = (uint32_t)by_var_dbl;
	uint32_t idx = hsh_u32[val];
	if (idx == 0) {
		vec_dbl.push_back(by_var_dbl);
		idx = hsh_u32[val] = (uint32_t)vec_dbl.size(); // so we are storing vec_str idx+1
		sub_tot.push_back(var_dbl);
	} else {
		sub_tot[idx-1] += var_dbl;
	}
	return idx;
}

uint32_t hash_uint32(std::unordered_map <uint32_t, uint32_t> &hsh_u32, std::vector <uint32_t> &vec_u32, uint32_t lkup, uint32_t val)
{
	uint32_t idx = hsh_u32[lkup];
	if (idx == 0) {
		vec_u32.push_back(val);
		idx = hsh_u32[lkup] = (uint32_t)vec_u32.size(); // so we are storing vec_str idx+1
	}
	return idx;
}

// given a by_var dbl value, returns the index value in the by_var.
// This is intended to be used for the chart_str by_var_hsh
//static uint32_t get_by_var_idx(std::unordered_map <double, uint32_t> hsh_dbl, double by_var_dbl, int line)
static uint32_t get_by_var_idx(std::unordered_map <uint32_t, uint32_t> &hsh_uint32, double by_var_dbl, int line)
{
	uint32_t val = (uint32_t)by_var_dbl;
	//uint32_t idx = hsh_dbl[by_var_dbl];
	uint32_t idx = hsh_uint32[val];
	if (idx == 0) {
		printf("got by_var_idx == 0. Shouldn't happen if we call this after build_chart_data(). called from line %d. Bye at %s %d\n", line, __FILE__, __LINE__);
		exit(1);
	}
	return idx-1;
}

static void run_actions(double &val, std::vector <action_str> actions, bool &use_value, evt_str &event_table)
{
	for (uint32_t k=0; k < actions.size(); k++) {
		if (actions[k].oper == "replace" && actions[k].val == val) {
			val = actions[k].val1;
		} else if (actions[k].oper == "replace_any") {
			val = actions[k].val;
		} else if (actions[k].oper == "set") {
			val = actions[k].val;
		} else if (actions[k].oper == "mult") {
			val *= actions[k].val;
		} else if (actions[k].oper == "add") {
			val += actions[k].val;
		} else if (actions[k].oper == "div" && actions[k].val != 0) {
			val /= actions[k].val;
		} else if (actions[k].oper == "cap") {
			if (val > actions[k].val) {
				val = actions[k].val;
			}
		} else if (actions[k].oper == "drop_if_gt") {
			if (val > actions[k].val) {
				use_value = false;
			}
#if 1
		} else if (actions[k].oper == "drop_if_str_contains") {
			uint32_t ival = val - 1;
			//printf("got drop_if_str_contains at %s %d\n", __FILE__, __LINE__);
			if (ival < event_table.vec_str.size() && actions[k].str.size() > 0) {
				std::string str = event_table.vec_str[ival];
				//printf("got drop_if_str_contains str= %s at %s %d\n", str.c_str(), __FILE__, __LINE__);
				if (str.find(actions[k].str) != std::string::npos) {
					//printf("got drop_if_str_contains drop at %s %d\n", __FILE__, __LINE__);
					use_value = false;
				}
			}
		}
#endif
	}
}

static int build_chart_data(uint32_t evt_idx, uint32_t chrt, evt_str &event_table, int verbose)
{
	int var_idx, by_var_idx=-1;
	var_idx      = (int)event_table.charts[chrt].var_idx;
	by_var_idx   = (int)event_table.charts[chrt].by_var_idx;
	if (by_var_idx < 0) {
		if (event_table.charts[chrt].by_var_sub_tots.size() < 1) {
			event_table.charts[chrt].by_var_sub_tots.resize(1, 0.0);
		}
	}
	printf("doing build_chart_data(%d, %d) title= %s, by_var_idx= %d data.vals.size()= %d at %s %d\n",
		evt_idx, chrt, event_table.charts[chrt].title.c_str(), by_var_idx,
		(int)event_table.data.vals.size(), __FILE__, __LINE__);
	std::string by_var_str;
	uint32_t fsz = (uint32_t)event_table.flds.size();
	uint32_t prv_ts_idx = (uint32_t)-1;
	uint32_t prv_ts_div_req_idx = (uint32_t)-1;
	uint32_t next_idx = (uint32_t)-1;
	uint32_t after_idx = (uint32_t)-1;
	uint32_t dura_idx = UINT32_M1;
	uint32_t other_by_var_idx = UINT32_M1;
	uint32_t fld_w_other_by_var_idx = UINT32_M1;
	std::string other_by_var_nm;
	int got_lag = 0;
	for (uint32_t j=0; j < fsz; j++) {
		if (event_table.flds[j].diff_ts_with_ts_of_prev_by_var_using_fld.size() > 0) {
			other_by_var_nm = event_table.flds[j].diff_ts_with_ts_of_prev_by_var_using_fld;
			fld_w_other_by_var_idx = j;
		}
		if (event_table.flds[j].lag_prev_by_var_using_fld.size() > 0) {
			for (uint32_t k=0; k < fsz; k++) {
				if (event_table.flds[j].lag_prev_by_var_using_fld == event_table.flds[k].name) {
					event_table.charts[chrt].lag_cfg.resize(got_lag+1);
					event_table.charts[chrt].lag_cfg[got_lag].fld_from = k;
					event_table.charts[chrt].lag_cfg[got_lag].fld_to   = j;
					event_table.charts[chrt].lag_cfg[got_lag].by_var = event_table.flds[j].lag_by_var;
					got_lag++;
					break;
				}
			}
		}
	}
	event_table.charts[chrt].lag_vec.resize(got_lag);
	if (other_by_var_nm.size() > 0) {
		for (uint32_t j=0; j < fsz; j++) {
			if ( event_table.flds[j].name == other_by_var_nm) {
				other_by_var_idx = j;
				break;
			}
		}
	}
	if (fld_w_other_by_var_idx != UINT32_M1 && var_idx != fld_w_other_by_var_idx) {
		fld_w_other_by_var_idx = UINT32_M1;
		other_by_var_idx = UINT32_M1;
	}
	for (uint32_t j=0; j < fsz; j++) {
		uint64_t flg = event_table.flds[j].flags;
		if (flg & (uint64_t)(fte_enum::FLD_TYP_DURATION_PREV_TS_SAME_BY_VAL)) {
			if (prv_ts_idx != (uint32_t)-1) {
				printf("can only handle 1 TYP_DURATION_PREV_TS_SAME_BY_VAL attribute in evt_flds. fld= %s event= %s, Chart= %s. bye at %s %d\n",
					event_table.flds[j].name.c_str(),
					event_table.event_name_w_area.c_str(),
					event_table.charts[chrt].title.c_str(), __FILE__, __LINE__);
				exit(1);
			}
			prv_ts_idx = j;
		}
		if (flg & (uint64_t)fte_enum::FLD_TYP_DIV_BY_INTERVAL2) {
			prv_ts_div_req_idx = j;
		}
		if (event_table.flds[j].next_for_name_idx != -1 &&
			event_table.flds[j].next_for_name_idx == var_idx) {
			next_idx = j;
		}
		if (flg & (uint64_t)fte_enum::FLD_TYP_STATE_AFTER) {
			after_idx = j;
		}
		if (flg & (uint64_t)fte_enum::FLD_TYP_DURATION_BEF) {
			dura_idx = j;
		}
	}
	double ts0 = 0.0, dt_cumu = 0;
	if (event_table.data.ts.size() == 0) {
		printf("ummm, why is data.ts size() == 0 for event= %s, chart= %s. maybe charts are marked use_chart=n? Assuming no data for chart and skipping it at %s %d\n",
			event_table.event_name.c_str(),
			event_table.charts[chrt].title.c_str(), __FILE__, __LINE__);
		return 1;
	} else {
		/* this may not always be the right 'initial timestamp' to use.
		 * I'm assuming that the 1st ts in the prf file is the one we want.
		 * This works for events like thermal:thermal_temperature where the 'prev_temp' is assumed to be
		 * the temperature from the start of the run (or from the previous event for a given zone).
		 * But there may be some cases where we truly want the timestamp of the first instance of the current event.
		 */
		ts0 = event_table.prf_ts_initial;
	}
	std::vector <uint32_t> last_val_for_by_var_idx;
	std::vector <double> last_other_by_var_ts;
	bool use_value = true;
	for (uint32_t i=0; i < event_table.data.vals.size(); i++) {
		double var_val = event_table.data.vals[i][var_idx];
		dt_cumu += var_val;
		int idx, other_idx;
		idx = -1;
		other_idx = -1;
		double byvv=-1;
		if (by_var_idx >= 0) {
			double by_var_val = event_table.data.vals[i][by_var_idx];
			byvv = by_var_val;
			idx = (int)hash_dbl_by_var( event_table.charts[chrt].by_var_hsh,
				event_table.charts[chrt].by_var_vals, by_var_val,
				event_table.charts[chrt].by_var_sub_tots, var_val) - 1;
			if (idx >= (int)last_val_for_by_var_idx.size()) {
				last_val_for_by_var_idx.resize(idx+1, -1);
			}
			if (fld_w_other_by_var_idx != UINT32_M1 && idx >= (int)last_other_by_var_ts.size()) {
				last_other_by_var_ts.resize(idx+1, -1.0);
			}
		} else {
			event_table.charts[chrt].by_var_sub_tots[0] += var_val;
			idx = 0;
			byvv = 0;
		}
		if (by_var_idx >= 0 && idx >= (int)last_val_for_by_var_idx.size()) {
			printf("screw up here at idx= %d, sz= %d, event= %s, title= %s at %s %d\n",
				idx, (int)last_val_for_by_var_idx.size(),
				event_table.event_name_w_area.c_str(),
				event_table.charts[chrt].title.c_str(), __FILE__, __LINE__);
			exit(1);
		}
		double obyvv = -1.0;
		if (other_by_var_idx != UINT32_M1) {
			double by_var_val = event_table.data.vals[i][other_by_var_idx];
			obyvv = by_var_val;
			other_idx = (int)hash_dbl_by_var( event_table.charts[chrt].by_var_hsh,
				event_table.charts[chrt].by_var_vals, by_var_val,
				event_table.charts[chrt].by_var_sub_tots, 0.0) - 1;
			if (other_idx >= (int)last_other_by_var_ts.size()) {
				last_other_by_var_ts.resize(other_idx+1, -1.0);
				printf("other_idx= %d by_var_val= %f at %s %d\n", other_idx, by_var_val, __FILE__, __LINE__);
			}
			double ts = event_table.data.ts[i].ts;
			uint32_t prev_i = last_other_by_var_ts[idx];
			double ts_other = -1.0;
			if (prev_i != UINT32_M1) {
				ts_other = event_table.data.ts[prev_i].ts;
			}
			double ts_diff = 0.0;
			if (ts_other != -1.0) {
				ts_diff = ts - ts_other;
			}
			event_table.charts[chrt].by_var_sub_tots[idx] += ts_diff;
			event_table.data.vals[i][fld_w_other_by_var_idx] = ts_diff;
			event_table.data.vals[i][var_idx] = ts_diff;
			last_other_by_var_ts[other_idx] = i;
			event_table.data.ts[i].duration = ts_diff;
		}
		for (uint32_t kk=0; kk < event_table.charts[chrt].lag_vec.size(); kk++) {
			uint32_t widx = idx, ubv = event_table.charts[chrt].lag_cfg[kk].by_var;
			if (ubv == 1) {
				widx = other_idx;
			}
			if (widx == UINT32_M1) {
				printf("ummm. widx= %d at %s %d\n", widx, __FILE__, __LINE__);
				exit(1);
			}
			if (widx >= event_table.charts[chrt].lag_vec[kk].size()) {
				event_table.charts[chrt].lag_vec[kk].resize(widx+1, 0.0);
			}
			if (idx >= event_table.charts[chrt].lag_vec[kk].size()) {
				event_table.charts[chrt].lag_vec[kk].resize(idx+1, 0.0);
			}
			uint32_t fld_from = event_table.charts[chrt].lag_cfg[kk].fld_from;
			uint32_t fld_to   = event_table.charts[chrt].lag_cfg[kk].fld_to;
			if (widx >= event_table.charts[chrt].lag_vec[kk].size()) {
				printf("error: widx= %d, event_table.charts[chrt].lag_vec[%d].size()= %d. bye at %s %d\n",
						widx, kk, (int)event_table.charts[chrt].lag_vec[kk].size(), __FILE__, __LINE__);
				exit(1);
			}
			double lag_val = event_table.charts[chrt].lag_vec[kk][idx];
			event_table.data.vals[i][fld_to] = lag_val;
#if 0
			std::string str;
			if (lag_val > 0.0) {
				str = event_table.vec_str[(uint32_t)(lag_val-1)];
			}
			double ts = event_table.data.ts[i].ts - ts0;
			printf("lag ts= %f, fld[%d] %s = %s, ubv= %d at %s %d\n",
					ts, (int)fld_to, event_table.flds[fld_to].name.c_str(), str.c_str(), ubv, __FILE__, __LINE__);
#endif
			event_table.charts[chrt].lag_vec[kk][widx] = event_table.data.vals[i][fld_from];
		}
		if (by_var_idx >= 0) {
			last_val_for_by_var_idx[idx] = i;
		}
		if (prv_ts_idx != (uint32_t)-1) {
			if (idx >= (int)event_table.charts[chrt].dura_prev_ts.size()) {
				event_table.charts[chrt].dura_prev_ts.resize(idx+1, ts0);
				printf("prv_ts_idx, set prev_ts[%d]= %f for event= %s, chart= %s. at %s %d\n",
					(int)idx, ts0, event_table.event_name.c_str(),
					event_table.charts[chrt].title.c_str(), __FILE__, __LINE__);
			}
			double ts = event_table.data.ts[i].ts;
			double dura = ts - event_table.charts[chrt].dura_prev_ts[idx];
			event_table.data.ts[i].duration = dura;
			event_table.charts[chrt].dura_prev_ts[idx] = ts;
			if (prv_ts_div_req_idx != (uint32_t)-1 && dura > 0.0) {
				event_table.data.vals[i][prv_ts_div_req_idx] /= dura;
				run_actions(event_table.data.vals[i][prv_ts_div_req_idx],
					event_table.flds[prv_ts_div_req_idx].actions_stage2, use_value, event_table);
			}
		}
	}
	if (next_idx != (uint32_t)-1 || after_idx != (uint32_t)-1) {
		double ts_end = event_table.ts_last;
		printf("checking for adding last values data.vals.sz= %d at %s %d\n",
			(int)event_table.data.vals.size(), __FILE__, __LINE__);
		for (uint32_t k=0; k < last_val_for_by_var_idx.size(); k++) {
			uint32_t idx = last_val_for_by_var_idx[k];
			if (idx == (uint32_t)-1 || event_table.data.ts[idx].ts == ts_end) {
				printf("idx= %d so skip adding last values at %s %d\n",
					(int)idx, __FILE__, __LINE__);
				// probably shouldn't happen but...
				continue;
			}
			double var_val, old_var_val, ts_prev = event_table.data.ts[idx].ts;
			old_var_val = event_table.data.vals[idx][var_idx];
			if (next_idx != (uint32_t)-1) {
				var_val = event_table.data.vals[idx][next_idx];
			} else {
				var_val = old_var_val;
			}
			if (verbose)
				printf("last idx= %d by_var_idx= %d prev_val= %f, next_val= %f ts_end= %f, ts_prev= %f, diff= %f at %s %d\n",
					idx, var_idx, old_var_val, var_val, ts_end, ts_prev, ts_end-ts_prev, __FILE__, __LINE__);
			// append a new event
			struct ts_str tss;
			tss.ts = ts_end;
			tss.duration = ts_end-ts_prev;
			event_table.data.ts.push_back(tss);
			event_table.data.vals.push_back(event_table.data.vals[idx]);
			event_table.data.vals.back()[var_idx] = var_val;
			event_table.data.prf_sample_idx.push_back(event_table.data.prf_sample_idx[idx]);
			if (by_var_idx >= 0) {
				double by_var_val = event_table.data.vals.back()[by_var_idx];
				hash_dbl_by_var( event_table.charts[chrt].by_var_hsh,
					event_table.charts[chrt].by_var_vals, by_var_val,
					event_table.charts[chrt].by_var_sub_tots, var_val);
			} else {
				event_table.charts[chrt].by_var_sub_tots[0] += var_val;
			}
		}
	}
	printf("dt_cumu = %f event_table[%d].charts[%d].by_var_vals.size()= %d at %s %d\n",
		dt_cumu, evt_idx, chrt, (int)event_table.charts[chrt].by_var_vals.size(), __FILE__, __LINE__);
	return 0;
}

static int ck_events_okay_for_this_chart(int file_grp, int evt_idx, int chrt, std::vector <evt_str> &event_table)
{
	std::string evt_need = event_table[evt_idx].event_name_w_area;
	if (options.verbose)
		printf("ck_events: grp= %d, evt_idx= %d, chrt= %d, evt_need= %s title= %s at %s %d\n",
			file_grp, evt_idx, chrt, evt_need.c_str(), event_table[evt_idx].charts[chrt].title.c_str(), __FILE__, __LINE__);
	if (evt_need.size() == 0) {
		return 0;
	}
	return 1;
}

static int report_chart_data(uint32_t evt_idx, uint32_t chrt, std::vector <evt_str> &event_table, int grp, int verbose)
{
	int var_idx, by_var_idx=-1;
	std::string by_var_str;
	uint64_t by_var_flags=0;

	printf("\nreport_chart_data:\n");
	var_idx      = (int)event_table[evt_idx].charts[chrt].var_idx;
	by_var_idx   = (int)event_table[evt_idx].charts[chrt].by_var_idx;
	if (by_var_idx >= 0) {
		by_var_flags = event_table[evt_idx].flds[by_var_idx].flags;
	}
	printf("report_chart_data: event_table[%d][%d].charts[%d].by_var_vals.size()= %d title= %s at %s %d\n",
		grp, evt_idx, chrt,
		(int)event_table[evt_idx].charts[chrt].by_var_vals.size(),
		event_table[evt_idx].charts[chrt].title.c_str(), __FILE__, __LINE__);
	event_table[evt_idx].charts[chrt].by_var_strs.resize(event_table[evt_idx].charts[chrt].by_var_vals.size());

	for (uint32_t i=0; i < event_table[evt_idx].charts[chrt].by_var_vals.size(); i++) {
		if (by_var_idx >= 0) {
			double by_var_val = event_table[evt_idx].charts[chrt].by_var_vals[i];
			if (by_var_flags & (uint64_t)fte_enum::FLD_TYP_STR) {
				by_var_str = event_table[evt_idx].vec_str[(uint32_t)(by_var_val-1)];
			} else if (by_var_flags & (uint64_t)fte_enum::FLD_TYP_INT) {
				by_var_str = std::to_string((int)by_var_val);
			} else if (by_var_flags & (uint64_t)fte_enum::FLD_TYP_DBL) {
				by_var_str = std::to_string(by_var_val);
			}
			uint32_t by_var_idx_val = get_by_var_idx(event_table[evt_idx].charts[chrt].by_var_hsh, by_var_val, __LINE__);
			if (event_table[evt_idx].charts[chrt].by_var_strs[by_var_idx_val].size() == 0) {
				event_table[evt_idx].charts[chrt].by_var_strs[by_var_idx_val] = by_var_str;
			}
			if (event_table[evt_idx].charts[chrt].by_var_sub_tots[i] != 0.0 || verbose) {
				printf("%s\t", by_var_str.c_str());
				printf("%f\n", event_table[evt_idx].charts[chrt].by_var_sub_tots[i]);
			}
#if 0
			if (event_table[evt_idx].charts[chrt].chart_tag == "PCT_BUSY_BY_PROC") {
				int idx = event_table[evt_idx].comm_pid_tid_hash[by_var_str];
				if (idx == 0) {
					printf("missed comm_pid_tid lookup for str= %s at %s %d\n", by_var_str.c_str(), __FILE__, __LINE__);
					exit(1);
				}
				event_table[evt_idx].comm_pid_tid_vec[--idx].total += event_table[evt_idx].charts[chrt].by_var_sub_tots[i];
			}
#endif
		} else {
			event_table[evt_idx].charts[chrt].by_var_strs[0] =
				event_table[evt_idx].flds[var_idx].name;
			printf("%s\t%f\n", event_table[evt_idx].flds[var_idx].name.c_str(),
				event_table[evt_idx].charts[chrt].by_var_sub_tots[i]);
		}
	}
	printf("report_chart_data: event_table[%d][%d].charts[%d].by_var_vals.size()= %d title= %s at %s %d\n",
		grp, evt_idx, chrt,
		(int)event_table[evt_idx].charts[chrt].by_var_vals.size(),
		event_table[evt_idx].charts[chrt].title.c_str(), __FILE__, __LINE__);
	return 0;
}


void chart_lines_reset(void)
{
	ch_lines.range.ts0 = -1.0;
	ch_lines.range.cat.clear();
	ch_lines.range.cat_rng.clear();
	ch_lines.range.cat_initd.clear();
	ch_lines.range.subcat_rng.clear();
	ch_lines.range.subcat_initd.clear();
	ch_lines.line.clear();
	ch_lines.tm_beg_offset_due_to_clip = 0.0;
	ch_lines.tm_end_offset_due_to_clip = 0.0;
	ch_lines.prf_obj = 0;
}

void chart_lines_ck_rng(double x, double y, double ts0, int cat, int subcat, double cur_val, double cur_dura, int fe_idx)
{
	if (ch_lines.range.ts0 == -1.0) {
		ch_lines.range.x_range[0] = x;
		ch_lines.range.x_range[1] = x;
		ch_lines.range.y_range[0] = y;
		ch_lines.range.y_range[1] = y;
		ch_lines.range.ts0 = ts0;
		ch_lines.range.ts0x = x;
		//printf("set ts0= %.9f at x= %.9f at %s %d\n", ts0, x, __FILE__, __LINE__);
	}
	if (ch_lines.range.ts0 > ts0) {
		ch_lines.range.ts0 = ts0;
	}
	if (ch_lines.range.x_range[0] > x) {
		ch_lines.range.x_range[0] = x;
	}
	if (ch_lines.range.x_range[1] < x) {
		ch_lines.range.x_range[1] = x;
	}
	if (ch_lines.range.y_range[0] > y) {
		ch_lines.range.y_range[0] = y;
	}
	if (ch_lines.range.y_range[1] < y) {
		ch_lines.range.y_range[1] = y;
	}
	if ((int)ch_lines.range.cat.size() <= cat) {
		ch_lines.range.cat.resize(cat+1);
		ch_lines.range.cat_rng.resize(cat+1);
		ch_lines.range.cat_initd.resize(cat+1, false);
		ch_lines.range.subcat_rng.resize(cat+1);
		ch_lines.range.subcat_initd.resize(cat+1);
	}
	if (!ch_lines.range.cat_initd[cat]) {
		ch_lines.range.cat_initd[cat] = true;
		ch_lines.range.cat_rng[cat].x0 = x;
		ch_lines.range.cat_rng[cat].x1 = x;
		ch_lines.range.cat_rng[cat].y0 = y;
		ch_lines.range.cat_rng[cat].y1 = y;
		ch_lines.range.cat_rng[cat].fe_idx = fe_idx;
	}
	if ((int)ch_lines.range.subcat_initd[cat].size() <= subcat) {
		ch_lines.range.subcat_rng[cat].resize(subcat+1);
		ch_lines.range.subcat_initd[cat].resize(subcat+1, false);
	}
	if (!ch_lines.range.subcat_initd[cat][subcat]) {
		ch_lines.range.subcat_initd[cat][subcat] = true;
		ch_lines.range.subcat_rng[cat][subcat].x0 = x;
		ch_lines.range.subcat_rng[cat][subcat].x1 = x;
		ch_lines.range.subcat_rng[cat][subcat].y0 = y;
		ch_lines.range.subcat_rng[cat][subcat].y1 = y;
		ch_lines.range.subcat_rng[cat][subcat].fe_idx = fe_idx;
	}
	if (ch_lines.range.cat_rng[cat].x0 > x) {
		ch_lines.range.cat_rng[cat].x0 = x;
	}
	if (ch_lines.range.cat_rng[cat].x1 < x) {
		ch_lines.range.cat_rng[cat].x1 = x;
	}
	if (ch_lines.range.cat_rng[cat].y0 > y) {
		ch_lines.range.cat_rng[cat].y0 = y;
	}
	if (ch_lines.range.cat_rng[cat].y1 < y) {
		ch_lines.range.cat_rng[cat].y1 = y;
	}
	if (ch_lines.range.subcat_rng[cat][subcat].x0 > x) {
		ch_lines.range.subcat_rng[cat][subcat].x0 = x;
	}
	if (ch_lines.range.subcat_rng[cat][subcat].x1 < x) {
		ch_lines.range.subcat_rng[cat][subcat].x1 = x;
	}
	if (ch_lines.range.subcat_rng[cat][subcat].y0 > y) {
		ch_lines.range.subcat_rng[cat][subcat].y0 = y;
	}
	if (ch_lines.range.subcat_rng[cat][subcat].y1 < y) {
		ch_lines.range.subcat_rng[cat][subcat].y1 = y;
	}
	ch_lines.range.subcat_rng[cat][subcat].initd = true;
	ch_lines.range.subcat_rng[cat][subcat].fe_idx = fe_idx;
	ch_lines.range.subcat_rng[cat][subcat].total    += cur_val;
	ch_lines.range.subcat_rng[cat][subcat].tot_dura += cur_dura;
	//printf("subcat[%d][%d]\n", cat, subcat);
}

static std::string dbl_2_str(double val)
{
	if (val == 0.0) {
		return "0.0";
	} else {
		std::string str = std::to_string(val);
		size_t pos = str.find(".");
		if (pos != std::string::npos) {
			int all_numbers = 1, last_zero=-1, pops=0;
			int sz = (int)str.size();
			for (int i=sz-1; i >= (int)(pos+1); i--) {
				if (str[i] >= '0' && str[i] <= '9') {
					if (str[i] == '0' && (i== (sz-1) || last_zero == i+1)) {
						last_zero = i;
						pops++;
					}
				} else {
					all_numbers = 0;
					break;
				}
			}
			if (all_numbers && pops > 0) {
				str.resize(sz-pops);
#if 0
				for (int i=0; i < pops; i++) {
					str.pop();
				}
#endif
			}
                }
		return str;
	}
}

static std::string build_proc_string(uint32_t evt_idx, uint32_t chrt_idx, std::vector <evt_str> &event_table)
{
	std::string str = "\"proc_arr\":[";
	uint32_t file_tag_idx = event_table[evt_idx].file_tag_idx;

	for (uint32_t i=0; i < comm_pid_tid_vec[file_tag_idx].size(); i++) {
		if (i > 0) { str += ", ";}
		double ntot = comm_pid_tid_vec[file_tag_idx][i].total;
		str += "{\"pid\":"+std::to_string(comm_pid_tid_vec[file_tag_idx][i].pid) +
			", \"tid\":"+std::to_string(comm_pid_tid_vec[file_tag_idx][i].tid) +
			", \"comm\":\""+comm_pid_tid_vec[file_tag_idx][i].comm +
			//"\", \"total\":"+std::to_string(ntot)+"}";
			"\", \"total\":"+dbl_2_str(ntot)+"}";
	}
	str += "]";
	if (options.show_json > 0) {
		printf("proc_arr: %s\n", str.c_str());
	}
	return str;
}

static std::string build_flnm_evt_string(uint32_t file_grp, int evt_idx, std::vector <evt_str> &event_table, int verbose)
{
	std::string str = "\"flnm_evt\":[";
	int did_line=0;
	bool skip;
	uint32_t file_tag_idx = event_table[evt_idx].file_tag_idx;
	if (file_tag_idx == UINT32_M1) {
		printf("ck yer code, file_tag_idx= %d at %s %d\n", file_tag_idx, __FILE__, __LINE__);
		exit(1);
	}
	for (uint32_t i=0; i < flnm_evt_vec[file_tag_idx].size(); i++) {
		skip= false;
		if (flnm_evt_vec[file_tag_idx][i].evt_tbl_idx != file_grp) {
			skip = true;
		}
		if (ETW_events_to_skip_hash[flnm_evt_vec[file_tag_idx][i].evt_str] != 0) {
			skip = true;
		}
		if (did_line >  0) { str += ", ";}
		std::string str1 = flnm_evt_vec[file_tag_idx][i].filename_bin;
		replace_substr(str1, "\\", "/", verbose);
		std::string str2 = flnm_evt_vec[file_tag_idx][i].filename_text;
		replace_substr(str2, "\\", "/", verbose);
		std::string str3 = flnm_evt_vec[file_tag_idx][i].evt_str;
		replace_substr(str3, "\\", "/", verbose);
		str += "{\"filename_bin\":\"" + str1 +
			"\", \"filename_text\":\"" + str2 +
			"\", \"event\":\"" + str3 +
			"\", \"skip\":"+std::to_string(skip) +
			", \"idx\":"+std::to_string(i)+
			", \"file_tag_idx\":"+std::to_string(file_tag_idx)+
			", \"prf_obj_idx\":"+std::to_string(flnm_evt_vec[file_tag_idx][i].prf_obj_idx)+
			", \"has_callstacks\":"+std::to_string(flnm_evt_vec[file_tag_idx][i].has_callstacks)+
			", \"total\":"+std::to_string(flnm_evt_vec[file_tag_idx][i].total)+"}";
		did_line = 1;
	}
	str += "]";
	if (options.show_json > 0) 
	{
		printf("flnm_evt: %s\n", str.c_str());
	}
	return str;
}

static uint64_t callstack_sz = 0;

static bool prf_mk_callstacks(prf_obj_str &prf_obj, int prf_idx,
		std::vector <int> &callstacks, int line, std::vector <std::string> &prefx,
		const std::string &follow_proc, int &csindx)
{
	bool rc = false, do_fllw = false;
	if (follow_proc.size() > 0) {
		do_fllw = true;
	}
	std::string mod_new, cs_new, cs_prv;
	for (uint32_t k=0; k < prf_obj.samples[prf_idx].callstack.size(); k++) {
		if (prf_obj.samples[prf_idx].callstack[k].mod == "[unknown]" &&
				prf_obj.samples[prf_idx].callstack[k].rtn == "[unknown]") {
			continue;
		}
		mod_new = prf_obj.samples[prf_idx].callstack[k].mod;
		if (rc == false && do_fllw) {
			if (mod_new.find(follow_proc) != std::string::npos) {
				rc = true;
			}
		}
		if (mod_new == "[kernel.kallsyms]") {
			mod_new = "[krnl]";
		}
		cs_new = mod_new + " " + prf_obj.samples[prf_idx].callstack[k].rtn;
		if (cs_new == cs_prv) {
			continue;
		}
		cs_prv = cs_new;
		int cs_idx = (int)hash_escape_string(callstack_hash, callstack_vec, cs_new) - 1;
		callstacks.push_back(cs_idx);
	}
	for (uint32_t k=0; k < prefx.size(); k++) {
		int cs_idx = (int)hash_escape_string(callstack_hash, callstack_vec, prefx[k]) - 1;
		callstacks.push_back(cs_idx);
	}
	std::string cs_str, cma;
        for (uint32_t k=0; k < callstacks.size(); k++) {
		cs_str += cma;
		cs_str += std::to_string(callstacks[k]);
		cma = ",";
	}
	csindx = (int)hash_string(callstack_hash, callstack_vec, cs_str) - 1;
	callstack_sz += callstacks.size();
	return rc;
}

static bool etw_mk_callstacks(int set_idx, prf_obj_str &prf_obj, int i,
				int stk_mod_rtn_idx, std::vector <int> &callstacks, int line,
				std::vector <std::string> &prefx, const std::string &follow_proc, int &csindx)
{
	bool rc = false, do_fllw= false;
	if (follow_proc.size() > 0) {
		do_fllw = true;
	}
	uint32_t cs_beg = prf_obj.etw_evts_set[set_idx][i].cs_idx_beg;
	if (cs_beg == UINT32_M1) {
		return rc;
	}
	uint32_t cs_end = prf_obj.etw_evts_set[set_idx][i].cs_idx_end;
	std::string cs_prv;
	for (uint32_t k=cs_beg; k <= cs_end; k++) {
		std::string str = prf_obj.etw_data[k][stk_mod_rtn_idx];
		std::string cs_new, mod, rtn;
		size_t pos = str.find("!");
		mod = str.substr(0, pos);
		if (rc == false && do_fllw) {
			if (mod.find(follow_proc) != std::string::npos) {
				rc = true;
			}
		}
		rtn = str.substr(pos+1, str.length());
		if (mod.substr(0, 1) == "\"") {
			mod = mod.substr(1, mod.length());
		}
		size_t pos2 = mod.find("\"");
		if (pos2 != std::string::npos) {
			mod = mod.substr(0, pos2);
			pos2 = mod.find("\"");
			if (pos2 != std::string::npos) {
				printf("messed up mod fixup mod='%s', orig= '%s' at %s %d\n", mod.c_str(), str.c_str(), __FILE__, __LINE__);
			}
		}
		if (rtn.size() > 3 && rtn.substr(0, 2) == "0x") {
			rtn = "unknown";
		}
		//printf("for cs str= '%s', mod= '%s', rtn= '%s' at %s %d\n", str.c_str(), mod.c_str(), rtn.c_str(), __FILE__, __LINE__);
		cs_new = mod + " " + rtn;
		if (cs_new == cs_prv) {
			continue;
		}
		cs_prv = cs_new;
		int cs_idx = (int)hash_escape_string(callstack_hash, callstack_vec, cs_new) - 1;
		callstacks.push_back(cs_idx);
	}
	for (uint32_t k=0; k < prefx.size(); k++) {
		int cs_idx = (int)hash_escape_string(callstack_hash, callstack_vec, prefx[k]) - 1;
		callstacks.push_back(cs_idx);
	}
	std::string cs_str, cma;
        for (uint32_t k=0; k < callstacks.size(); k++) {
		cs_str += cma;
		cs_str += std::to_string(callstacks[k]);
		cma = ",";
	}
	csindx = (int)hash_string(callstack_hash, callstack_vec, cs_str) - 1;
	callstack_sz += callstacks.size();
	return rc;
}

enum {
	OVERLAP_ADD,
	OVERLAP_TRUNC,
	OVERLAP_IGNORE,
};
static double block_delta= 0.25;

static int build_chart_lines(uint32_t evt_idx, uint32_t chrt, prf_obj_str &prf_obj, std::vector <evt_str> &event_table, int verbose)
{
	int var_pid_idx= -1, var_comm_idx = -1, var_cpu_idx=-1, var_idx, by_var_idx=-1;
	var_idx      = (int)event_table[evt_idx].charts[chrt].var_idx;
	by_var_idx   = (int)event_table[evt_idx].charts[chrt].by_var_idx;
	uint32_t fsz = (uint32_t)event_table[evt_idx].flds.size();
	int doing_pct_busy_by_cpu = 0;
	enum {
		CHART_TYPE_BLOCK,
		CHART_TYPE_LINE,
		CHART_TYPE_STACKED,
	};
	int chart_type = CHART_TYPE_BLOCK;
	if (event_table[evt_idx].data.ts.size() == 0) {
		return 1;
	}
	bool skip_idle = false;
	if (event_table[evt_idx].flds[var_idx].flags & (uint64_t)fte_enum::FLD_TYP_EXCL_PID_0) {
		if (options.verbose)
			fprintf(stdout, "skip idle= true, var_idx= %d at %s %d\n", var_idx, __FILE__, __LINE__);
		skip_idle = true;
	}
	if (event_table[evt_idx].charts[chrt].chart_tag == "PCT_BUSY_BY_CPU") {
		doing_pct_busy_by_cpu = 1;
	}
	int doing_PCT_BUSY_BY_SYSTEM = 0;
	if (event_table[evt_idx].charts[chrt].chart_tag == "PCT_BUSY_BY_SYSTEM") {
		doing_PCT_BUSY_BY_SYSTEM = 1;
	}
	uint32_t file_tag_idx = event_table[evt_idx].file_tag_idx;
	std::string follow_proc;
	if (doing_pct_busy_by_cpu && options.follow_proc.size() > file_tag_idx) {
		follow_proc = options.follow_proc[file_tag_idx];
	}
	uint32_t tot_samples= 0;
	for (uint32_t i=0; i < prf_obj.events.size(); i++) {
		if (doing_pct_busy_by_cpu) {
			printf("event[%d]: %s, count= %d at %s %d\n",
				i, prf_obj.events[i].event_name_w_area.c_str(), prf_obj.events[i].evt_count, __FILE__, __LINE__);
		}
		tot_samples += prf_obj.events[i].evt_count;
	}
	if (verbose)
		printf("tot_evts= %d, tot_samples= %d at %s %d\n", (int32_t)prf_obj.events.size(), tot_samples, __FILE__, __LINE__);
	if (event_table[evt_idx].charts[chrt].chart_type == "line") {
		chart_type = CHART_TYPE_LINE;
	}
	else if (event_table[evt_idx].charts[chrt].chart_type == "stacked") {
		chart_type = CHART_TYPE_STACKED;
	}
	//uint32_t wait_ts_idx= UINT32_M1, wait_dura_idx= UINT32_M1;
	std::vector <uint32_t> add_2_extra;
	int numerator_idx = -1, denom_idx = -1;
	for (uint32_t j=0; j < fsz; j++) {
		uint64_t flg = event_table[evt_idx].flds[j].flags;
		//fprintf(stderr, "fld[%d] flgs= 0x%x, name= %s at %s %d\n", j, flg, event_table[evt_idx].flds[j].name.c_str(), __FILE__, __LINE__);
		if (event_table[evt_idx].flds[j].name == "numerator") {
			numerator_idx = j;
		}
		if (event_table[evt_idx].flds[j].name == "denominator") {
			denom_idx = j;
		}
		if (flg & (uint64_t)fte_enum::FLD_TYP_EXCL_PID_0 && verbose) {
			fprintf(stderr, "skip idle= true, var_idx= %d j= %d at %s %d\n", var_idx, j, __FILE__, __LINE__);
		}
		if (flg & (uint64_t)fte_enum::FLD_TYP_SYS_CPU) {
			var_cpu_idx = (int)j;
		}
		if (flg & (uint64_t)fte_enum::FLD_TYP_COMM_PID_TID) {
			var_comm_idx = (int)j;
		}
		if (flg & (uint64_t)fte_enum::FLD_TYP_PID) {
			var_pid_idx = (int)j;
		}
		if (flg & (uint64_t)fte_enum::FLD_TYP_ADD_2_EXTRA || event_table[evt_idx].flds[j].name == "extra_str") {
			add_2_extra.push_back(j);
		}
		if (event_table[evt_idx].flds[j].name == "extra_str") {
//LUA
			if (verbose)
				printf("got fld[%d].name= extra_str for FILE_TYP= %d at %s %d\n",
					j, prf_obj.file_type, __FILE__, __LINE__);
		}
	}
	std::vector <uint32_t> lua_col_map;
	if (prf_obj.file_type == FILE_TYP_LUA) {
		lua_col_map.resize(fsz, -1);
		for (uint32_t j=0; j < fsz; j++) {
			uint32_t k;
			for (k=0; k < prf_obj.lua_data.col_names[evt_idx].size(); k++) {
				if (event_table[evt_idx].flds[j].lkup == prf_obj.lua_data.col_names[evt_idx][k]) {
					lua_col_map[j] = k;
					break;
				}
			}
		}
	}
	uint32_t have_filter_regex = UINT32_M1;
	for (uint32_t i=0; i < event_table[evt_idx].charts[chrt].actions.size(); i++) {
		if (event_table[evt_idx].charts[chrt].actions[i].oper == "filter_regex") {
			event_table[evt_idx].charts[chrt].actions[i].regex_fld_idx = UINT32_M1;
			for (uint32_t j=0; j < fsz; j++) {
				if (event_table[evt_idx].flds[j].name == event_table[evt_idx].charts[chrt].actions[i].str) {
					event_table[evt_idx].charts[chrt].actions[i].regex_fld_idx = j;
					event_table[evt_idx].charts[chrt].actions[i].regx =
						std::regex(event_table[evt_idx].charts[chrt].actions[i].str1);
					break;
				}
			}
			if (event_table[evt_idx].charts[chrt].actions[i].regex_fld_idx == UINT32_M1) {
				fprintf(stderr, "didn't find regex evt_flds.name= %s in chart.json for chart title= '%s'. bye at %s %d\n",
					event_table[evt_idx].charts[chrt].actions[i].str.c_str(),
					event_table[evt_idx].charts[chrt].title.c_str(), __FILE__, __LINE__);
				exit(1);
			}
			if (have_filter_regex == UINT32_M1) {
				have_filter_regex = i;
			}
		}
	}
	uint32_t have_drop_if_contains = UINT32_M1;
	for (uint32_t i=0; i < event_table[evt_idx].charts[chrt].actions.size(); i++) {
		if (event_table[evt_idx].charts[chrt].actions[i].oper == "drop_if_str_contains") {
			event_table[evt_idx].charts[chrt].actions[i].drop_if_fld_idx = UINT32_M1;
			for (uint32_t j=0; j < fsz; j++) {
				if (event_table[evt_idx].flds[j].name == event_table[evt_idx].charts[chrt].actions[i].str) {
					event_table[evt_idx].charts[chrt].actions[i].drop_if_fld_idx = j;
					event_table[evt_idx].charts[chrt].actions[i].drop_if_str =
						event_table[evt_idx].charts[chrt].actions[i].str1;
					if (have_drop_if_contains == UINT32_M1) {
						have_drop_if_contains = i;
					}
					break;
				}
			}
			if (event_table[evt_idx].charts[chrt].actions[i].drop_if_fld_idx == UINT32_M1) {
				fprintf(stderr, "didn't find drop_if_flds.name= %s in chart.json for chart title= '%s'. bye at %s %d\n",
					event_table[evt_idx].charts[chrt].actions[i].str.c_str(),
					event_table[evt_idx].charts[chrt].title.c_str(), __FILE__, __LINE__);
				exit(1);
			}
		}
	}
	double ts0 = prf_obj.tm_beg;
	ch_lines.tm_beg_offset_due_to_clip = prf_obj.tm_beg_offset_due_to_clip;
	ch_lines.prf_obj = &prf_obj;
	if (verbose) {
		printf("build_chart_lines(%d, %d): ts0= %f chart= '%s' by_var_idx= %d by_var_str.size()= %d by_var_vals.sz= %d at %s %d\n",
		evt_idx, chrt, ts0, event_table[evt_idx].charts[chrt].title.c_str(), (int)by_var_idx,
		(int)event_table[evt_idx].charts[chrt].by_var_strs.size(),
		(int)event_table[evt_idx].charts[chrt].by_var_vals.size(),
		__FILE__, __LINE__);
		fflush(NULL);
	}
	ch_lines.line.clear();
	uint32_t by_var_sz;
	if (by_var_idx >= 0) {
		by_var_sz = (uint32_t)event_table[evt_idx].charts[chrt].by_var_sub_tots.size();
	} else {
		by_var_sz = 1;
	}
	ch_lines.legend.resize(by_var_sz);
	ch_lines.subcats.resize(by_var_sz);
	if (by_var_idx >= 0) {
		for (uint32_t i=0; i < by_var_sz; i++) {
			ch_lines.legend[i] = event_table[evt_idx].charts[chrt].by_var_strs[i];
			if (doing_pct_busy_by_cpu == 1) {
				ch_lines.subcats[i].resize(2);
				ch_lines.subcats[i][1] = "events";
			} else {
				ch_lines.subcats[i].resize(1);
			}
			ch_lines.subcats[i][0] = event_table[evt_idx].flds[var_idx].name;
#if 0
			printf("by_var_idx= %d, subcats[%d][%d] size= %d legend[%d] = %s at %s %d\n",
				(int)by_var_idx, i, (int)ch_lines.subcats[i].size(), (int)ch_lines.subcats[i][0].size(),
				i, ch_lines.legend[i].c_str(), __FILE__, __LINE__);
#endif
		}
	} else {
		ch_lines.legend[0] = event_table[evt_idx].flds[var_idx].name;
		if (doing_pct_busy_by_cpu == 1) {
			ch_lines.subcats[0].resize(2);
		} else {
			ch_lines.subcats[0].resize(1);
		}
		ch_lines.subcats[0][0] = event_table[evt_idx].flds[var_idx].name;
		if (doing_pct_busy_by_cpu == 1) {
			ch_lines.subcats[0][1] = "events";
		}
		printf("by_var_idx= %d, subcats[%d][%d] at %s %d\n",
			(int)by_var_idx, (int)ch_lines.subcats.size(), (int)ch_lines.subcats[0].size(), __FILE__, __LINE__);
	}
	if (verbose)
		printf("doing build_chart_data(%d, %d)\n", evt_idx, chrt);
	std::string by_var_str;
	double x, y;
	double delta=block_delta, lo=0.0, hi=1.0;
	bool idle_pid;
	double var_val;
	uint32_t by_var_idx_val = 0;
	struct lines2_st_str {
		double x0, x1, y0, y1; // orig beg, end of interval
		int skip_count;
		lines2_st_str(): x0(-1.0), x1(-1.0), y0(-1.0), y1(-1.0), skip_count(0) {}
	};
	struct lines_st_str {
		double x0, x1, y0, y1; // orig beg, end of interval
		lines_str ls0;
		lines_st_str(): x0(-1.0), x1(-1.0) {}
	};
	uint32_t stk_mod_rtn_idx = -1;
	uint32_t stk_idx = UINT32_M1;
	if (prf_obj.file_type == FILE_TYP_ETW) {
		stk_idx = prf_obj.etw_evts_hsh["Stack"] - 1;
		if (stk_idx != UINT32_M1) {
			for (uint32_t i=0; i < prf_obj.events[stk_idx].etw_cols.size(); i++) {
				if (prf_obj.events[stk_idx].etw_cols[i] == "Image!Function") {
					stk_mod_rtn_idx = i;
					break;
				}
			}
		}
		if (stk_mod_rtn_idx == UINT32_M1) {
			stk_idx = UINT32_M1;
		}
		if (verbose)
			printf("stk_idx= %d, stk_mod_rtn_idx= %d at %s %d\n", stk_idx, stk_mod_rtn_idx, __FILE__, __LINE__);
	}

	uint32_t other_by_var_idx = UINT32_M1;
	uint32_t fld_w_other_by_var_idx = UINT32_M1;
	std::string other_by_var_nm;
	for (uint32_t j=0; j < fsz; j++) {
		if (event_table[evt_idx].flds[j].diff_ts_with_ts_of_prev_by_var_using_fld.size() > 0) {
			other_by_var_nm = event_table[evt_idx].flds[j].diff_ts_with_ts_of_prev_by_var_using_fld;
			fld_w_other_by_var_idx = j;
		}
	}
	if (other_by_var_nm.size() > 0) {
		for (uint32_t j=0; j < fsz; j++) {
			if ( event_table[evt_idx].flds[j].name == other_by_var_nm) {
				other_by_var_idx = j;
				break;
			}
		}
	}
	if (fld_w_other_by_var_idx != UINT32_M1 && var_idx != fld_w_other_by_var_idx) {
		fld_w_other_by_var_idx = UINT32_M1;
		other_by_var_idx = UINT32_M1;
	}
	if (fld_w_other_by_var_idx != UINT32_M1 && other_by_var_idx == UINT32_M1) {
		printf("apparently missed lookup here at %s %d\n", __FILE__, __LINE__);
		exit(1);
	}

	std::vector <lines_st_str> last_by_var_lines_str;
	std::vector <bool> last_by_var_lines_str_initd;
	struct prv_nxt_str {
		int prv, nxt;
		prv_nxt_str(): prv(-1), nxt(-1) {}
	};
	std::vector <prv_nxt_str> prv_nxt, oprv_nxt;
	std::vector <int> prv_by_var, oprv_by_var;
	std::vector <int> lst_by_var, olst_by_var;
	prv_nxt.resize(event_table[evt_idx].data.vals.size());
	lst_by_var.resize(event_table[evt_idx].data.vals.size(), 0);
	prv_by_var.resize(by_var_sz, -1);
	if (prf_obj.file_type == FILE_TYP_LUA &&
		event_table[evt_idx].charts[chrt].chart_tag == "PHASE_CHART" && phase_vec.size() > 0) {
		printf("PHASE: phase_vec.size= %d, event_table[%d].data.vals.size= %d at %s %d\n",
			(uint32_t)phase_vec.size(), evt_idx,  (uint32_t)event_table[evt_idx].data.vals.size(), __FILE__, __LINE__);
		if (phase_vec.size() == event_table[evt_idx].data.vals.size()) {
			printf("PHASE: override data table timestamps with phase_vec timestamps & durations at %s %d\n", __FILE__, __LINE__);
			for (uint32_t i = 0; i < event_table[evt_idx].data.vals.size(); i++) {
				event_table[evt_idx].data.ts[i].duration = phase_vec[i].dura;
				event_table[evt_idx].data.ts[i].ts = phase_vec[i].ts_abs;
			}
		}
	}
	for (uint32_t i = 0; i < event_table[evt_idx].data.vals.size(); i++) {
		by_var_idx_val = 0;
		if (by_var_idx >= 0) {
			int by_var_val = (int)event_table[evt_idx].data.vals[i][by_var_idx];
			by_var_idx_val = get_by_var_idx(event_table[evt_idx].charts[chrt].by_var_hsh, by_var_val, __LINE__);
		}
		int prv = prv_by_var[by_var_idx_val];
		if (prv > -1) {
			prv_nxt[prv].nxt = i;
		}
		lst_by_var[i] = by_var_idx_val;
		prv_nxt[i].prv = prv;
		prv_by_var[by_var_idx_val] = i;
	}
	if (other_by_var_idx != UINT32_M1) {
		oprv_nxt.resize(event_table[evt_idx].data.vals.size());
		olst_by_var.resize(event_table[evt_idx].data.vals.size(), 0);
		oprv_by_var.resize(by_var_sz, -1);
		uint32_t oby_var_idx_val;
		for (uint32_t i = 0; i < event_table[evt_idx].data.vals.size(); i++) {
			by_var_idx_val = 0;
			oby_var_idx_val = 0;
			if (by_var_idx >= 0) {
				int by_var_val = (int)event_table[evt_idx].data.vals[i][by_var_idx];
				by_var_idx_val = get_by_var_idx(event_table[evt_idx].charts[chrt].by_var_hsh, by_var_val, __LINE__);
				by_var_val = (int)event_table[evt_idx].data.vals[i][other_by_var_idx];
				oby_var_idx_val = get_by_var_idx(event_table[evt_idx].charts[chrt].by_var_hsh, by_var_val, __LINE__);
			}
			int prv = oprv_by_var[by_var_idx_val];
			if (prv > -1) {
				oprv_nxt[prv].nxt = i;
			}
			olst_by_var[i] = oby_var_idx_val;
			oprv_nxt[i].prv = prv;
			//if (by_var_idx_val == oby_var_idx_val) {
				oprv_by_var[oby_var_idx_val] = i;
			//}
		}
	}
	std::vector <lines2_st_str> last_by_var_y_val;

	if (chart_type == CHART_TYPE_STACKED || chart_type == CHART_TYPE_LINE) {
		last_by_var_lines_str.resize(by_var_sz);
		last_by_var_lines_str_initd.resize(by_var_sz, false);
		last_by_var_y_val.resize(by_var_sz);
		std::vector <std::vector <int>> last_by_var_callstacks;
		std::vector <int> overlaps_idx;
		std::vector <double> overlaps_sum;
		std::vector <double> y_val;
		std::vector <double> overlaps_used_sum;
		int lag_state_idx = -1, lag_reason_idx = -1;
		int cur_state_idx = -1, cur_reason_idx = -1;
		for (uint32_t i=0; i < event_table[evt_idx].flds.size(); i++) {
			uint64_t flg = event_table[evt_idx].flds[i].flags;
			if ((flg & (uint64_t)fte_enum::FLD_TYP_CSW_STATE) && !(flg & (uint64_t)fte_enum::FLD_TYP_LAG)) {
				cur_state_idx = i;
			}
			if ((flg & (uint64_t)fte_enum::FLD_TYP_CSW_REASON) && !(flg & (uint64_t)fte_enum::FLD_TYP_LAG)) {
				cur_reason_idx = i;
			}
			if ((flg & (uint64_t)fte_enum::FLD_TYP_CSW_STATE) && (flg & (uint64_t)fte_enum::FLD_TYP_LAG)) {
				lag_state_idx = i;
			}
			if ((flg & (uint64_t)fte_enum::FLD_TYP_CSW_REASON) && (flg & (uint64_t)fte_enum::FLD_TYP_LAG)) {
				lag_reason_idx = i;
			}
		}
		y_val.resize(by_var_sz, 0.0);
		last_by_var_callstacks.resize(by_var_sz);
		double cur_x1=-1.0, cur_x0=-1.0;
		int i, beg_idx = (int)event_table[evt_idx].data.vals.size()-1;
		int cur_idx = beg_idx;
		int skipped_due_to_same_x1=0, overlaps_max_sz=0;
		std::vector <int> overlaps_idx_used;
		overlaps_used_sum.resize(beg_idx+1, 0.0);
		overlaps_idx_used.resize(beg_idx+1, -1);
		int handle_overlap = OVERLAP_ADD;
		if (prf_obj.has_tm_run && chart_type == CHART_TYPE_LINE) {
			handle_overlap = OVERLAP_TRUNC;
			if (event_table[evt_idx].charts[chrt].options & (uint64_t)copt_enum::OVERLAPPING_RANGES_WITHIN_AREA) {
				handle_overlap = OVERLAP_IGNORE;
			}
		}
#if 1
		bool line_ck_overlap;
		lines_str *ls0p;
		while (cur_idx >=0) {
			double fx0, fx1, sx0;
			i = cur_idx;
			if (chart_type != CHART_TYPE_STACKED && overlaps_idx_used[i] == -2) {
				// this one has already been used
				cur_idx--;
				continue;
			}
			double dura;
			dura = event_table[evt_idx].data.ts[i].duration;
			fx1 = event_table[evt_idx].data.ts[i].ts - ts0;
			fx0 = fx1 - dura;
#if 1
			if (options.tm_clip_beg_valid == CLIP_LVL_2  && (fx1+ts0) < options.tm_clip_beg) {
				cur_idx--;
				continue;
			}
			if (options.tm_clip_end_valid == CLIP_LVL_2 && (fx0+ts0) > options.tm_clip_end) {
				cur_idx--;
				continue;
			}
#endif
			if (have_filter_regex != UINT32_M1) {
				bool skip = false;
				for (uint32_t ii=have_filter_regex; ii < event_table[evt_idx].charts[chrt].actions.size(); ii++) {
					if (event_table[evt_idx].charts[chrt].actions[ii].oper == "filter_regex") {
						uint32_t fld_idx = event_table[evt_idx].charts[chrt].actions[ii].regex_fld_idx;
						uint32_t val = event_table[evt_idx].data.vals[i][fld_idx];
						std::string str = event_table[evt_idx].vec_str[val-1];
						if (std::regex_match( str, event_table[evt_idx].charts[chrt].actions[ii].regx)) {
							skip = false;
						} else {
							skip = true;
						}
						if (verbose > 0)
							printf("regex expr= %s in str= %s rc= %d val= %d at %s %d\n",
								event_table[evt_idx].charts[chrt].actions[ii].str1.c_str(),
								str.c_str(), skip, val, __FILE__, __LINE__);
						if (skip) {
							break;
						}
					}
				}
				if (skip) {
					cur_idx--;
					continue;
				}
			}
			if (have_drop_if_contains != UINT32_M1) {
				bool skip = false;
				for (uint32_t ii=have_drop_if_contains; ii < event_table[evt_idx].charts[chrt].actions.size(); ii++) {
					if (event_table[evt_idx].charts[chrt].actions[ii].oper == "drop_if_str_contains") {
						uint32_t fld_idx = event_table[evt_idx].charts[chrt].actions[ii].drop_if_fld_idx;
						uint32_t val = event_table[evt_idx].data.vals[i][fld_idx];
						std::string str = event_table[evt_idx].vec_str[val-1];
						std::string lkfor = event_table[evt_idx].charts[chrt].actions[ii].drop_if_str;
						if (str.find(lkfor) != std::string::npos) {
							//printf("skipping area str= %s due to containing str= %s at %s %d\n",
							//		str.c_str(), lkfor.c_str(), __FILE__, __LINE__);
							skip = true;
							break;
						}
					}
				}
				if (skip) {
					cur_idx--;
					continue;
				}
			}
			bool tmp_verbose = false;

			if (chart_type != CHART_TYPE_STACKED) {
				double used_so_far = 0.0;
				if (overlaps_idx_used[i] > -1) {
					used_so_far = overlaps_used_sum[i];
#if 0
					if (used_so_far > dura) {
						printf("mess up here. used_so_far= %f, dura= %f, fix yer code dude. Bye at %s %d\n",
								used_so_far, dura, __FILE__, __LINE__);
						exit(1);
					}
#else
					used_so_far = dura;
					overlaps_used_sum[i] = used_so_far;
#endif
				}
				//fx1 -= used_so_far;
			}
			sx0 = 0.0;
			if (chart_type == CHART_TYPE_STACKED) {
				if (i > 0) {
					sx0 = event_table[evt_idx].data.ts[i-1].ts - ts0;
				}
			} else {
				line_ck_overlap = false;
				int j = i;
				double nx0, nx1;
				sx0 = fx0;
				while(true) {
					j = prv_nxt[j].prv;
					//printf("cur_idx= %d, i= %d, prv j= %d at %s %d\n", cur_idx, i, j, __FILE__, __LINE__);
					if (j < 0) {
						break;
					}
					nx1 = event_table[evt_idx].data.ts[j].ts - ts0;
					nx0 = nx1 - event_table[evt_idx].data.ts[j].duration;
					if (nx1 <= fx0) {
						// so this event ends before the cur interval
						break;
					}
					if (chart_type == CHART_TYPE_LINE && handle_overlap == OVERLAP_IGNORE) {
						break;
					}
#if 0
					if (handle_overlap == OVERLAP_TRUNC) {
						if (nx1 > fx1) {
							printf("ummm.... screwup here dude, faulty assumption at %s %d\n", __FILE__, __LINE__);
							exit(1);
						}
						printf("setting fx0 from %f to %f at %s %d\n", fx0, nx1, __FILE__, __LINE__);
						sx0 = nx1;
						fx0 = nx1;
						break;
					}
#endif
					if (overlaps_idx_used[j] == -1) {
						line_ck_overlap = true;
						overlaps_idx.push_back(j);
						overlaps_sum.push_back(nx1-nx0);
						overlaps_used_sum[j] = nx1-nx0;
						//printf(" add j= %d, fx0= %f, fx1= %f, nx0= %f nx1= %f, sz= %d\n", j, fx0, fx1, nx0, nx1, (int)overlaps_idx.size());
						overlaps_idx_used[j] = j;
					}
				}
			}
			if (sx0 >= fx1 && chart_type == CHART_TYPE_STACKED) {
				skipped_due_to_same_x1++;
				if (chart_type == CHART_TYPE_STACKED) {
					overlaps_idx.push_back(i);
					overlaps_sum.push_back(fx1-fx0);
					overlaps_used_sum[i] = fx1-fx0;
					overlaps_idx_used[i] = i;
				}
				cur_idx--;
				continue;
			}
			if (overlaps_idx_used[i] == -1) {
				//printf(" def add i= %d, fx0= %f, fx1= %f sz= %d\n", i, fx0, fx1, (int)overlaps_idx.size());
				overlaps_idx.push_back(i);
				overlaps_sum.push_back(fx1-fx0);
				overlaps_used_sum[i] = fx1-fx0;
				overlaps_idx_used[i] = i;
			}
			memset(&y_val[0], 0, sizeof(double)*by_var_sz);
			double nx0, nx1;
			if (chart_type == CHART_TYPE_STACKED) {
				for (int32_t j=i-1; j >= 0;  j--) {
					nx1 = event_table[evt_idx].data.ts[j].ts - ts0;
					nx0 = nx1 - event_table[evt_idx].data.ts[j].duration;
					if (nx1 <= sx0) {
						// so this event ends before the cur interval
						break;
					}
					if (overlaps_idx_used[j] == -1) {
						overlaps_idx.push_back(j);
						overlaps_sum.push_back(nx1-nx0);
						overlaps_used_sum[j] = nx1-nx0;
						overlaps_idx_used[j] = j;
					}
				}
			}
			// Compute the values of each by_var which overlaps current interval.
			// Use the fraction of the by_var interval which overlaps current interval.
			int overlaps_sz = (int)overlaps_idx.size();
			if (overlaps_max_sz < overlaps_sz) {
				overlaps_max_sz = overlaps_sz;
			}
			
				by_var_idx_val = lst_by_var[cur_idx];
				//printf("overlaps_sz= %d at %s %d\n", overlaps_sz, __FILE__, __LINE__);
			if (handle_overlap == OVERLAP_TRUNC) {
				uint32_t prv = prv_nxt[cur_idx].prv;
				by_var_idx_val = lst_by_var[cur_idx];
				double new_val = event_table[evt_idx].data.vals[i][var_idx];
				if (prv != UINT32_M1) {
					double dura2, x0, x1;
					dura2 = event_table[evt_idx].data.ts[prv].duration;
					x1 = event_table[evt_idx].data.ts[prv].ts - ts0;
					x0 = x1 - dura2;
					if (x1 > fx0) {
						fx0 = x1;
					}
					if (x1 < fx0) {
						fx0 = x1;
					}
					sx0 = fx0;
				}
				if (event_table[evt_idx].charts[chrt].actions.size() > 0) {
					bool use_value = true;
					run_actions(new_val, event_table[evt_idx].charts[chrt].actions, use_value, event_table[evt_idx]);
				}
				y_val[by_var_idx_val] = new_val;
			} else {
				for (int32_t j=0; j < overlaps_sz; j++) {
					i = overlaps_idx[j];
					if (chart_type != CHART_TYPE_STACKED && lst_by_var[i] != lst_by_var[cur_idx]) {
						continue;
					}
					by_var_idx_val = lst_by_var[i];
					var_val = event_table[evt_idx].data.vals[i][var_idx];
					double dura2;
					double tx1, x1, tx0, x0;
					dura2 = event_table[evt_idx].data.ts[i].duration;
					x1 = event_table[evt_idx].data.ts[i].ts - ts0;
					x0 = x1 - dura2;
					tx1 = x1;
					tx0 = x0;
					if (x0 < sx0) {
						x0 = sx0;
					}
					if (x1 > fx1) {
						x1 = fx1;
					}
					double dff = (x1 - x0);
					if (dff > 0.0) {
						overlaps_sum[j] -= dff;
						overlaps_used_sum[j] -= dff;
					}
					if ((overlaps_sum[j]/(tx1 - tx0)) < 0.0001) {
						overlaps_sum[j] = 0.0;
						overlaps_used_sum[j] = 0.0;
					}
					if (fx1 < sx0) {
						printf("what's going on here. fx1= %f, sx0= %f, fx0= %f i= %d, cur_idx= %d. dura= %f. evt_nm= %s, evt_nm_w_area= %s bye at %s %d\n",
								fx1, sx0, fx0, i, cur_idx, dura,
								prf_obj.events[evt_idx].event_name.c_str(),
								prf_obj.events[evt_idx].event_name_w_area.c_str(),
								__FILE__, __LINE__);
						if (prf_obj.file_type == FILE_TYP_ETW) {
							//uint32_t cpt_idx = event_table[evt_idx].data.comm_pid_tid_idx[i];
							uint32_t cpt_idx = event_table[evt_idx].data.cpt_idx[0][i];
							uint32_t file_tag_idx = event_table[evt_idx].file_tag_idx;
							printf("proc= %s, pid= %d, tid= %d, ts= %f, relTS= %f at %s %d\n",
									comm_pid_tid_vec[file_tag_idx][cpt_idx].comm.c_str(),
									comm_pid_tid_vec[file_tag_idx][cpt_idx].pid,
									comm_pid_tid_vec[file_tag_idx][cpt_idx].tid,
									event_table[evt_idx].data.ts[i].ts,
									event_table[evt_idx].data.ts[i].ts - ts0,
									__FILE__, __LINE__);
						}
						exit(1);
					}
					double new_val = 0.0;
					if ((fx1 - sx0) > 0.0 && !(tx1 < fx0 || tx0 > fx1)) {
						// interval is > 0 and tx interval is not out of range
						new_val = var_val * (x1 - x0) / (fx1 - sx0);
					}
					//if (y_val[by_var_idx_val] != 0.0)
					if (tmp_verbose)
					{
						printf("aft c=%d j=%d i=%d yv=%f vval= %f nval= %f x0=%f, x1=%f, fx0=%f, fx1=%f ts= %f at %s %d\n",
							cur_idx, j, i, y_val[by_var_idx_val],
							var_val, new_val, x0, x1, fx0, fx1, event_table[evt_idx].data.ts[i].ts,
							__FILE__, __LINE__);
					}
					if (new_val < 0.0 && var_val >= 0.0 && ((x1 - x0) < 0.0 || (fx1 - sx0) < 0.0)) {
						printf("screw up here, new_val= %f, var_val= %f, x0= %f, x1= %f, dff= %f ts0= %f, tx1= %f, sx0= %f, fx0= %f fx1= %f dff= %f at %s %d\n",
							new_val, var_val, x0, x1, (x1 - x0), tx0, tx1, sx0, fx0, fx1, (fx1 - sx0), __FILE__, __LINE__);
					}
					if (skip_idle) {
						idle_pid = false;
						int pid_num = -1;
						if (var_pid_idx > -1) {
							pid_num = (int)event_table[evt_idx].data.vals[i][var_pid_idx];
							if (pid_num == 0) {
								idle_pid = true;
								new_val = 0.0;
							}
						}
					}
					if (event_table[evt_idx].charts[chrt].actions.size() > 0) {
						bool use_value = true;
						run_actions(new_val, event_table[evt_idx].charts[chrt].actions, use_value, event_table[evt_idx]);
					}
					if (new_val < 0.0 && var_val >= 0.0 && ((x1 - x0) < 0.0 || (fx1 - sx0) < 0.0)) {
						printf("screw up here, new_val= %f, var_val= %f, x0= %f, x1= %f, dff= %f sx0= %f, fx1= %f dff= %f at %s %d\n",
							new_val, var_val, x0, x1, (x1 - x0), sx0, fx1, (fx1 - sx0), __FILE__, __LINE__);
					}
					y_val[by_var_idx_val] += new_val;
				}
			}
			if (handle_overlap != OVERLAP_IGNORE) {
				i = 0;
				while (i < overlaps_sz) {
					if (overlaps_sum[i] <= 0.0 && overlaps_sz > 0) {
						// used all of this one
						//printf(" drp i= %d, fx0= %f, fx1= %f, sz= %d\n", overlaps_idx[i], fx0, fx1, (int)overlaps_sz);
						overlaps_idx_used[ overlaps_idx[i] ] = -2;
						overlaps_idx[i] = overlaps_idx[overlaps_sz-1];
						overlaps_sum[i] = overlaps_sum[overlaps_sz-1];
						overlaps_sz--;
						continue;
					}
					i++;
				}
				//printf(" end cur_idx= %d sz= %d\n", cur_idx, (int)overlaps_sz);
				overlaps_idx.resize(overlaps_sz);
				overlaps_sum.resize(overlaps_sz);
			}
			std::string comm, legnd;
			i = cur_idx;

			var_val = event_table[evt_idx].data.vals[i][var_idx];
			by_var_idx_val = lst_by_var[cur_idx];
			idle_pid = false;
			int pid_num = -1;
			if (var_pid_idx > -1) {
				pid_num = (int)event_table[evt_idx].data.vals[i][var_pid_idx];
				if (pid_num == 0) {
					idle_pid = true;
				}
			}
			x = event_table[evt_idx].data.ts[i].ts - dura - ts0;
			if (var_comm_idx > -1) {
				double var_comm_val = event_table[evt_idx].data.vals[i][var_comm_idx];
				comm = event_table[evt_idx].vec_str[(int)(var_comm_val-1)];
			} else {
				comm = legnd;
			}
			int prf_idx = event_table[evt_idx].data.prf_sample_idx[i];
			double x0, x1, ycumu;
			x1 = fx1;
			x0 = fx0;

			if (event_table[evt_idx].charts[chrt].options & (uint64_t)copt_enum::DROP_1ST) {
				uint32_t prv = prv_nxt[cur_idx].prv;
				if (prv == UINT32_M1) {
					cur_idx--;
					continue;
				}
			}

			ls0p = &last_by_var_lines_str[by_var_idx_val].ls0;
#if 1
			if (options.tm_clip_beg_valid == CLIP_LVL_2 && (x1+ts0) < options.tm_clip_beg) {
				cur_idx--;
				continue;
			}
			if (options.tm_clip_end_valid == CLIP_LVL_2 && (x0+ts0) > options.tm_clip_end) {
				cur_idx--;
				continue;
			}
			if (options.tm_clip_beg_valid == CLIP_LVL_2 && (x0+ts0) < options.tm_clip_beg) {
				x0 = options.tm_clip_beg - ts0;
			}
			if (options.tm_clip_end_valid == CLIP_LVL_2 && (x1+ts0) > options.tm_clip_end) {
				x1 = options.tm_clip_end - ts0;
			}
#endif
			last_by_var_lines_str[by_var_idx_val].x0 = x0; // last_by_val_lines_str..x1 - x0 is the true duration
			last_by_var_lines_str[by_var_idx_val].x1 = x1;
			if (i > 0) {
				x0 = sx0;
			} else {
				//x0 = 0.0; // setting x0=0.0 messes up case where there is only one point in line say for T=4.0->8.0 (like win_gui_delay)
			}
			ls0p->x[0] = x0;
			ls0p->x[1] = x1;
			if (chart_type == CHART_TYPE_STACKED) {
				ls0p->typ  = SHAPE_RECT;
			} else {
				ls0p->typ  = SHAPE_LINE;
			}
			ls0p->text = comm;
			ls0p->pid  = pid_num;
			if (numerator_idx != -1 && denom_idx != -1) {
				ls0p->use_num_denom = 1;
				ls0p->numerator = event_table[evt_idx].data.vals[i][numerator_idx];
				ls0p->denom     = event_table[evt_idx].data.vals[i][denom_idx];
			}
			bool use_this_line = true;
			//ls0p->cpt_idx = event_table[evt_idx].data.comm_pid_tid_idx[i];
			bool got_follow_proc = false;
			uint32_t cpt_idx = UINT32_M1;
			if (prf_obj.file_type != FILE_TYP_ETW) {
			    //cpt_idx = prf_obj.samples[prf_idx].cpt_idx;
				cpt_idx = event_table[evt_idx].data.cpt_idx[0][i];
				if (cpt_idx == UINT32_M1 && prf_obj.samples[i].comm.size() > 0 && prf_obj.samples[i].pid > -1 &&
						prf_obj.samples[i].tid > -1) {
					uint32_t file_tag_idx = event_table[evt_idx].file_tag_idx;
					cpt_idx = (int)hash_comm_pid_tid(comm_pid_tid_hash[file_tag_idx], comm_pid_tid_vec[file_tag_idx],
						prf_obj.samples[i].comm, prf_obj.samples[i].pid, prf_obj.samples[i].tid) - 1;
					printf("need new code here at %s %d\n", __FILE__, __LINE__);
					exit(1);
			    	//prf_obj.samples[prf_idx].cpt_idx = cpt_idx;
				}
			    ls0p->cpt_idx = cpt_idx;
				ls0p->fe_idx  = prf_obj.samples[prf_idx].fe_idx;
				if (doing_PCT_BUSY_BY_SYSTEM && prf_obj.samples[prf_idx].flags & GOT_FOLLOW_PROC) {
					got_follow_proc = true;
					//printf("fllw2 at %s %d\n", __FILE__, __LINE__);
					ls0p->flags  |= GOT_FOLLOW_PROC;
				}
				ls0p->period  = (double)prf_obj.samples[prf_idx].period;
				ls0p->cpu     = (int)prf_obj.samples[prf_idx].cpu;
				ls0p->pid     = (int)prf_obj.samples[prf_idx].tid;
				ls0p->callstack_str_idxs.resize(0);
				if (event_table[evt_idx].charts[chrt].chart_tag == "RUN_QUEUE" ||
					event_table[evt_idx].charts[chrt].chart_tag == "RUN_QUEUE_BY_CPU" ||
					event_table[evt_idx].charts[chrt].chart_tag == "WAIT_TIME_BY_proc") {
					std::vector <std::string> prefx;
					bool got_ready = true;
			    	uint32_t file_tag_idx = event_table[evt_idx].file_tag_idx;
					if (cpt_idx != UINT32_M1 && file_tag_idx != UINT32_M1 && comm_pid_tid_vec[file_tag_idx][cpt_idx].pid == 0) {
						//printf("skip idle run_q at %s %d\n", __FILE__, __LINE__);
						cur_idx--;
						continue;
					}
					if (lag_reason_idx != -1 || lag_state_idx != -1 || cur_state_idx != -1) {
						//
						int val0= -1, val1= -1;
						std::string str;
						if (lag_reason_idx != -1) {
							val0 = event_table[evt_idx].data.vals[i][lag_reason_idx];
						}
#if 1
						uint32_t jj = oprv_nxt[i].prv;
						if (cur_state_idx != -1 && jj != UINT32_M1) {
							val1 = event_table[evt_idx].data.vals[jj][cur_state_idx];
						}
#else
						if (lag_state_idx != -1) {
							val1 = event_table[evt_idx].data.vals[i][lag_state_idx];
						}
#endif
						if (val0 > 0) {
							str = event_table[evt_idx].vec_str[val0 - 1];
							//printf("reason= %s\n", str.c_str());
							prefx.push_back(str);
						}
						if (val1 > 0) {
							str = event_table[evt_idx].vec_str[val1 - 1];
							//printf("ck Run state= %s, at %s %d\n", str.c_str(), __FILE__, __LINE__);
							if ((event_table[evt_idx].charts[chrt].chart_tag == "RUN_QUEUE" ||
								event_table[evt_idx].charts[chrt].chart_tag == "RUN_QUEUE_BY_CPU") &&
								//str != "R" && str != "R+")
								str != "R") {
								got_ready = false;
								//printf("got Run state= %s, at %s %d\n", str.c_str(), __FILE__, __LINE__);
							}
							prefx.push_back(str);
						}
						if (!got_ready) {
							y_val[by_var_idx_val] = 0.0;
						}
					}
					if (got_ready) {
						std::vector <int> callstacks;
						uint32_t jj = oprv_nxt[i].prv;
						if (jj != UINT32_M1) {
							int prf_idx2 = event_table[evt_idx].data.prf_sample_idx[jj];
							fx1 = event_table[evt_idx].data.ts[i].ts - ts0;
							fx0 = event_table[evt_idx].data.ts[jj].ts - ts0;
							double ux0=x0, ux1=x1, uclip_beg = 0, uclip_end=0;
							if (options.tm_clip_beg_valid == CLIP_LVL_1) {
								uclip_beg = options.tm_clip_beg - ch_lines.range.ts0;
							}
							if (options.tm_clip_end_valid == CLIP_LVL_1) {
								uclip_end = options.tm_clip_end - ch_lines.range.ts0;
							}
							if (uclip_end != 0.0 && ux0 < uclip_beg && ux1 >= uclip_beg) {
								ux0 = uclip_beg;
							}
							if (uclip_end != 0.0 && ux1 > uclip_end && ux0 <= uclip_end) {
								ux1 = uclip_end;
							}
							ls0p->x[0] = ux0;
							ls0p->x[1] = ux1;
							y_val[by_var_idx_val] = ux1-ux0;
							std::string fllw;
							prf_mk_callstacks(prf_obj, prf_idx2, callstacks, __LINE__, prefx, fllw, ls0p->csi);
							ls0p->callstack_str_idxs = callstacks;
#if 0
							// printfs useful for debugging RUN_QUEUE chart
							std::string ostr;
							prf_sample_to_string(prf_idx2, ostr, prf_obj);
							printf("\tckck: %s at %s %d\n", ostr.c_str(), __FILE__, __LINE__);
							//int cs_idx = (int)hash_string(callstack_hash, callstack_vec, cs_new) - 1;
							for (uint32_t jk=0; jk < callstacks.size(); jk++) {
								uint32_t j1 = callstacks[jk];
								printf("\t%s\n", callstack_vec[j1].c_str());
							}
							// save off the current event's callstack for prev_comm
							//last_by_var_callstacks[other_by_var_idx] = callstacks;
							//last_by_var_callstacks[other_idx] = callstacks;
#endif
						}
					}
				}
			} else if (prf_obj.file_type == FILE_TYP_ETW) {
			    //cpt_idx = event_table[evt_idx].data.comm_pid_tid_idx[i];
			    cpt_idx = event_table[evt_idx].data.cpt_idx[0][i];
			    uint32_t file_tag_idx = event_table[evt_idx].file_tag_idx;
				if (cpt_idx != UINT32_M1) {
					ls0p->pid        = comm_pid_tid_vec[file_tag_idx][cpt_idx].pid;
				} else {
					ls0p->pid        = -1;
				}
				ls0p->fe_idx     = get_fe_idxm1(prf_obj, event_table[evt_idx].event_name_w_area, __LINE__, true, event_table[evt_idx]);
				if (var_cpu_idx > -1) {
			        ls0p->cpu    = event_table[evt_idx].data.vals[i][var_cpu_idx];
				} else {
			        ls0p->cpu    = -1;
				}
				if (event_table[evt_idx].charts[chrt].chart_tag == "WAIT_TIME_BY_proc" ||
					event_table[evt_idx].charts[chrt].chart_tag == "RUN_QUEUE" ||
					event_table[evt_idx].charts[chrt].chart_tag == "RUN_QUEUE_BY_CPU") {
					if (ls0p->pid == 0) {
						//printf("skip idle run_q at %s %d\n", __FILE__, __LINE__);
						cur_idx--;
						continue;
					}
				}
				uint32_t prf_evt_idx = event_table[evt_idx].event_idx_in_file;
				//int prv = prv_nxt[i].prv;
				//int prv = i;
				int prv = event_table[evt_idx].data.prf_sample_idx[i];
				if (doing_PCT_BUSY_BY_SYSTEM && prf_obj.etw_evts_set[prf_evt_idx][prv].flags & GOT_FOLLOW_PROC) {
					got_follow_proc = true;
					//printf("fllw3 at %s %d\n", __FILE__, __LINE__);
					ls0p->flags  |= GOT_FOLLOW_PROC;
				}
				if (stk_idx != UINT32_M1 && prf_obj.etw_evts_set[prf_evt_idx][prv].cs_idx_beg != UINT32_M1 &&
					event_table[evt_idx].charts[chrt].chart_tag == "WAIT_TIME_BY_proc") {
					// for CSwitch, the callstack below the event is callstack of the incoming thread before starting the new thread
					std::vector <int> callstacks;
					std::vector <std::string> prefx;
					if (lag_reason_idx != -1 && lag_state_idx != -1) {
						int val0, val1;
						std::string str;
						val0 = event_table[evt_idx].data.vals[i][lag_reason_idx];
						val1 = event_table[evt_idx].data.vals[i][lag_state_idx];
						if (val0 > 0 && val1 > 0) {
							str = event_table[evt_idx].vec_str[val0 - 1];
							//printf("reason= %s\n", str.c_str());
							prefx.push_back(str);
							str = event_table[evt_idx].vec_str[val1 - 1];
							prefx.push_back(str);
						}
					}
					std::string fllw;
					etw_mk_callstacks(prf_evt_idx, prf_obj, prv, stk_mod_rtn_idx, callstacks, __LINE__, prefx, fllw, ls0p->csi);
					ls0p->callstack_str_idxs = callstacks;
					//last_by_var_callstacks[by_var_idx_val] = callstacks;
				}
				if (event_table[evt_idx].charts[chrt].chart_tag == "RUN_QUEUE" ||
					event_table[evt_idx].charts[chrt].chart_tag == "RUN_QUEUE_BY_CPU") {
					std::vector <std::string> prefx;
					bool got_ready = false;
					if (lag_reason_idx != -1 || lag_state_idx != -1) {
						//
						int val0= -1, val1= -1;
						std::string str0, str1;
						if (lag_reason_idx != -1) {
							val0 = event_table[evt_idx].data.vals[i][lag_reason_idx];
						}
						if (lag_state_idx != -1) {
							val1 = event_table[evt_idx].data.vals[i][lag_state_idx];
						}
						if (val0 > 0) {
							str0 = event_table[evt_idx].vec_str[val0 - 1];
							//printf("reason= %s\n", str0.c_str());
							prefx.push_back(str0);
						}
						if (val1 > 0) {
							str1 = event_table[evt_idx].vec_str[val1 - 1];
							if (str1 == "Ready" || str1 == "Running") {
								if (str0 != "WrYieldExecution") {
									got_ready = true;
								}
								//printf("got Run state= %s, at %s %d\n", str1.c_str(), __FILE__, __LINE__);
							}
							prefx.push_back(str1);
						}
						if (!got_ready) {
							//use_this_line = false;
							y_val[by_var_idx_val] = 0.0;
						}
					}
					if (got_ready && stk_idx != UINT32_M1 &&
						prf_obj.etw_evts_set[prf_evt_idx][prv].cs_idx_beg != UINT32_M1) {
						std::vector <int> callstacks;
						std::string fllw;
						etw_mk_callstacks(prf_evt_idx, prf_obj, prv, stk_mod_rtn_idx, callstacks, __LINE__, prefx, fllw, ls0p->csi);
						ls0p->callstack_str_idxs = callstacks;
						//last_by_var_callstacks[by_var_idx_val] = callstacks;
					}
				}
				ls0p->cpt_idx    = cpt_idx;
				{
					int ti;
					if (prf_obj.file_type == FILE_TYP_ETW) {
						ti = cpt_idx - 1; // not sure this should be -1 for ETW
					} else {
						//ti = prf_obj.samples[prf_idx].cpt_idx;
						ti = event_table[evt_idx].data.cpt_idx[0][i];
					}
					uint32_t file_tag_idx = event_table[evt_idx].file_tag_idx;
					if (ti != -1 && ti >= comm_pid_tid_vec[file_tag_idx].size()) {
						//printf("ckck ti: got ti= %d, comm_pid_tid_vec[file_tag_idx].sz= %d at %s %d\n", ti, (int)comm_pid_tid_vec[file_tag_idx].size(), __FILE__, __LINE__);
					}
					if (ti != -1 && ti < comm_pid_tid_vec[file_tag_idx].size()) {
						//comm_pid_tid_vec[file_tag_idx][ti].total += y_val[by_var_idx_val];
					}
				}
			}
#if 1
			if (options.tm_clip_beg_valid == CLIP_LVL_2 && (ls0p->x[1]+ts0) < options.tm_clip_beg) {
				cur_idx--;
				continue;
			}
			if (options.tm_clip_end_valid == CLIP_LVL_2 && (ls0p->x[0]+ts0) > options.tm_clip_end) {
				cur_idx--;
				continue;
			}
			if (options.tm_clip_beg_valid == CLIP_LVL_2 && (ls0p->x[0]+ts0) < options.tm_clip_beg) {
				ls0p->x[0] = options.tm_clip_beg - ts0;
			}
			if (options.tm_clip_end_valid == CLIP_LVL_2 && (ls0p->x[1]+ts0) > options.tm_clip_end) {
				ls0p->x[1] = options.tm_clip_end - ts0;
			}
#endif
			if (ls0p->pid != 0) {
				ls0p->y[0] = 0.0;
				ls0p->y[1] = y_val[by_var_idx_val];
			} else {
				ls0p->y[0] = 0.0;
				ls0p->y[1] = 0.0;
			}
			last_by_var_lines_str[by_var_idx_val].y0 = ls0p->y[0];
			last_by_var_lines_str[by_var_idx_val].y1 = ls0p->y[1];
			ls0p->prf_idx = prf_idx;
			ls0p->cat = (int)by_var_idx_val;
			ls0p->subcat = 0;

			last_by_var_lines_str_initd[by_var_idx_val] = true;

			if (prf_obj.file_type == FILE_TYP_ETW) {
				i = cur_idx;
				uint32_t prf_evt_idx = event_table[evt_idx].event_idx_in_file;
				ls0p->text += "<br>line " + std::to_string(prf_obj.etw_evts_set[prf_evt_idx][i].txt_idx);
				if (add_2_extra.size() > 0) {
					uint32_t data_idx = prf_obj.etw_evts_set[prf_evt_idx][i].data_idx;
					for (uint32_t ii=0; ii < add_2_extra.size(); ii++) {
						uint32_t fld_idx = add_2_extra[ii];
						uint32_t str_idx = event_table[evt_idx].data.vals[i][fld_idx];
						ls0p->text += "<br>" + event_table[evt_idx].vec_str[(uint32_t)(str_idx-1)];
						printf("add_2_extra: %s, ii= %d, fld_idx= %d, by_var_idx_val= %d by_var_sz= %d at %s %d\n",
								ls0p->text.c_str(), ii, fld_idx, by_var_idx_val, by_var_sz, __FILE__, __LINE__);
					}
				}
			}
//LUA
			if (prf_obj.file_type != FILE_TYP_ETW && prf_obj.file_type != FILE_TYP_LUA) {
				if (add_2_extra.size() > 0) {
					int prf_idx = event_table[evt_idx].data.prf_sample_idx[i];
					ls0p->text += ", line_a " + std::to_string(prf_obj.samples[prf_idx].line_num);
					ls0p->text += "<br>" + prf_obj.samples[prf_idx].extra_str;
				}
			}
			if (prf_obj.file_type == FILE_TYP_LUA) {
				if (add_2_extra.size() > 0) {
					uint32_t k = lua_col_map[add_2_extra[0]];
					int prf_idx = event_table[evt_idx].data.prf_sample_idx[i];
					ls0p->text += ", line_b " + std::to_string(prf_obj.samples[prf_idx].line_num);
					if (prf_idx >= prf_obj.lua_data.data_rows.size()) {
						printf("=========== mess up, prf_idx= %d, sz= %d, add2ex= %d at %s %d\n", 
							prf_idx, (uint32_t)prf_obj.lua_data.data_rows.size(), k, __FILE__, __LINE__);
						exit(0);
					}
					if (verbose > 0)
						printf("=========== extra_str, prf_idx= %d, sz= %d, add2ex= %d str= %s at %s %d\n", 
							prf_idx, (uint32_t)prf_obj.lua_data.data_rows.size(),
							k, prf_obj.lua_data.data_rows[prf_idx][k].str.c_str(),
							__FILE__, __LINE__);
					ls0p->text += "<br>" + prf_obj.lua_data.data_rows[prf_idx][k].str;
				}
			}
			if (chart_type != CHART_TYPE_STACKED) {
				ls0p->y[0] = y_val[by_var_idx_val];
				ls0p->y[1] = y_val[by_var_idx_val];
				if (tmp_verbose) {
					printf("ckck x0= %f, x1= %f, y0= %f, y1= %f var_val= %f at %s %d\n",
						ls0p->x[0], ls0p->x[1], ls0p->y[0], ls0p->y[1], var_val, __FILE__, __LINE__);
				}
				if (use_this_line) {
					chart_lines_ck_rng(ls0p->x[0], ls0p->y[0], ts0, ls0p->cat, ls0p->subcat, 0.0, (ls0p->x[1]-ls0p->x[0]), ls0p->fe_idx);
					chart_lines_ck_rng(ls0p->x[1], ls0p->y[1], ts0, ls0p->cat, ls0p->subcat, y_val[by_var_idx_val], 0.0, ls0p->fe_idx);
					ch_lines.line.push_back(*ls0p);
				}
				if (handle_overlap != OVERLAP_IGNORE) {
					i = 0;
					overlaps_sz = (int)overlaps_idx.size();
					while (i < overlaps_sz) {
						if (overlaps_idx[i] == cur_idx) {
							// used all of this one
							//printf(" dr2 i= %d, fx0= %f, fx1= %f, sz= %d\n", overlaps_idx[i], fx0, fx1, (int)overlaps_sz);
							overlaps_idx[i] = overlaps_idx[overlaps_sz-1];
							overlaps_sum[i] = overlaps_sum[overlaps_sz-1];
							overlaps_sz--;
							break;
						}
						i++;
					}
					overlaps_idx_used[ cur_idx ] = -2;
					//printf(" en2 i= %d, fx0= %f, fx1= %f, sz= %d\n", cur_idx, fx0, fx1, (int)overlaps_sz);
					overlaps_idx.resize(overlaps_sz);
					overlaps_sum.resize(overlaps_sz);
				}
				cur_idx--;
				continue;
			}
			double tm_srt1 = dclock();

			ycumu = 0.0;
			for (uint32_t kk=0; kk < by_var_sz; kk++) {
				double yv;
				lines_str ls1;
				yv = y_val[kk];
				if (last_by_var_lines_str_initd[kk]) {
					ls1 = last_by_var_lines_str[kk].ls0;
				} else {
					//yv = 0.0;
					ls1 = last_by_var_lines_str[by_var_idx_val].ls0;
					ls1.text = "";
					ls1.pid = -1;
					ls1.period = 0;
					ls1.cpt_idx = -1;
					ls1.fe_idx = -1;
				}
				ls1.x[0] = x0;
				ls1.x[1] = x1;
				if (chart_type == CHART_TYPE_STACKED) {
					ls1.y[0] = ycumu;
					ycumu += yv;
					ls1.y[1] = ycumu;
				} else {
					ls1.y[0] = yv;
					ls1.y[1] = yv;
				}
				ls1.cat = kk;
				ls1.subcat = 0;
				chart_lines_ck_rng(ls1.x[0], ls1.y[0], ts0, ls1.cat, ls1.subcat, 0.0, (ls1.x[1]-ls1.x[0]), ls1.fe_idx);
				chart_lines_ck_rng(ls1.x[1], ls1.y[1], ts0, ls1.cat, ls1.subcat, yv, 0.0, ls1.fe_idx);
				if (cur_idx > 2 && yv == 0.0 && last_by_var_y_val[kk].y1 == 0.0) {
					last_by_var_y_val[kk].skip_count++;
					continue;
				}
				if (cur_idx > 2 && yv != 0.0 && last_by_var_y_val[kk].y1 == 0.0 &&
					last_by_var_y_val[kk].skip_count > 0) {
					lines2_st_str l2s;
					l2s.x0 = ls1.x[0];
					l2s.x1 = ls1.x[1];
					l2s.y0 = ls1.y[0];
					l2s.y1 = ls1.y[1];
					ls1.x[0] = last_by_var_y_val[kk].x1;
					ls1.x[1] = x0;
					ls1.y[0] = 0.0;
					ls1.y[1] = 0.0;
					ch_lines.line.push_back(ls1);
					ls1.x[0] = l2s.x0;
					ls1.x[1] = l2s.x1;
					ls1.y[0] = l2s.y0;
					ls1.y[1] = l2s.y1;
				}
				last_by_var_y_val[kk].x0 = x0;
				last_by_var_y_val[kk].x1 = x1;
				last_by_var_y_val[kk].y0 = yv;
				last_by_var_y_val[kk].y1 = yv;
				last_by_var_y_val[kk].skip_count = 0;
				ch_lines.line.push_back(ls1);
			}
			cur_idx--;
		}
		if (options.verbose)
		fprintf(stderr, "stacked: skipped_due_to_same_x1= %d of %d events, o_lap_mx_sz= %d skip_idle= %d, title= %s at %s %d\n",
				skipped_due_to_same_x1, beg_idx, overlaps_max_sz, (int)skip_idle,
				event_table[evt_idx].charts[chrt].title.c_str(),
				__FILE__, __LINE__);
#endif
		//fprintf(stderr, "  stacked ch_lines.lines.sz= %" PRId64 " at %s %d\n", ch_lines.line.size(), __FILE__, __LINE__);
		double tm_srt1 = dclock();
		std::sort(ch_lines.line.begin(), ch_lines.line.end(), compareByChTime);
		double tm_srt2 = dclock() - tm_srt1;
		//fprintf(stderr, "  stacked ch_lines.lines sort tm= %f at %s %d\n",
		//		tm_srt2, __FILE__, __LINE__);
		return 0;
	}
	// BLOCK chart
	for (uint32_t i=0; i < event_table[evt_idx].data.vals.size(); i++) {
		var_val = event_table[evt_idx].data.vals[i][var_idx];
		int pid_num = -1;
		std::string legnd, comm;
		by_var_idx_val = 0;
		if (by_var_idx >= 0) {
			int by_var_val = (int)event_table[evt_idx].data.vals[i][by_var_idx];
			by_var_idx_val = get_by_var_idx(event_table[evt_idx].charts[chrt].by_var_hsh, by_var_val, __LINE__);
		} else {
			event_table[evt_idx].charts[chrt].by_var_sub_tots[0] += var_val;
		}

		if (chart_type == CHART_TYPE_BLOCK && var_cpu_idx > -1) {
			lo = event_table[evt_idx].data.vals[i][var_cpu_idx];
			hi = lo + delta;
		}

		double dura = event_table[evt_idx].data.ts[i].duration;
		idle_pid = false;
		if (var_pid_idx > -1) {
			pid_num = (int)event_table[evt_idx].data.vals[i][var_pid_idx];
			if (pid_num == 0) {
				idle_pid = true;
			}
		}
		x = event_table[evt_idx].data.ts[i].ts - dura - ts0;
#if 1
		if (options.tm_clip_beg_valid == CLIP_LVL_2 && (x+dura+ts0) < options.tm_clip_beg) {
			continue;
		}
		if (options.tm_clip_beg_valid == CLIP_LVL_2 && (x+ts0) < options.tm_clip_beg) {
			x = options.tm_clip_beg - ts0;
		}
		if (options.tm_clip_end_valid == CLIP_LVL_2 && (x+ts0) > options.tm_clip_end) {
			continue;
		}
#endif
		y = lo;
		if (var_comm_idx > -1) {
			double var_comm_val = event_table[evt_idx].data.vals[i][var_comm_idx];
			comm = event_table[evt_idx].vec_str[(int)(var_comm_val-1)];
		} else {
			comm = legnd;
		}
		int prf_idx = event_table[evt_idx].data.prf_sample_idx[i];
		if (prf_obj.file_type != FILE_TYP_ETW) {
			//int cpt_idx = prf_obj.samples[prf_idx].cpt_idx;
			int cpt_idx = event_table[evt_idx].data.cpt_idx[0][i];
			if (cpt_idx == -1 && prf_obj.samples[prf_idx].comm.size() > 0 && prf_obj.samples[prf_idx].pid > -1 &&
					prf_obj.samples[prf_idx].tid > -1) {
				uint32_t file_tag_idx = event_table[evt_idx].file_tag_idx;
				cpt_idx = (int)hash_comm_pid_tid(comm_pid_tid_hash[file_tag_idx], comm_pid_tid_vec[file_tag_idx],
					prf_obj.samples[prf_idx].comm, prf_obj.samples[prf_idx].pid, prf_obj.samples[prf_idx].tid) - 1;
				printf("Need new code at %s %d\n", __FILE__, __LINE__);
				exit(1);
			}
		}
		if (chart_type == CHART_TYPE_BLOCK && doing_pct_busy_by_cpu == 1) {
			// this is for the vertical 'line' above the sched_switch block to mark the end of the event
			lines_str ls1;
			ls1.x[0] = event_table[evt_idx].data.ts[i].ts - ts0;
			ls1.y[0] = y + delta;
			ls1.pid  = -1; // want to show all the events...
			ls1.typ  = SHAPE_LINE;
			ls1.fe_idx  = get_fe_idxm1(prf_obj, event_table[evt_idx].event_name_w_area, __LINE__, true, event_table[evt_idx]);
			ls1.period = 0.0;
			if (prf_obj.file_type != FILE_TYP_ETW) {
				ls1.cpt_idx = event_table[evt_idx].data.cpt_idx[0][i];
				ls1.period  = (double)prf_obj.samples[prf_idx].period;
				ls1.cpu     = (int)prf_obj.samples[prf_idx].cpu;
			} else {
			    uint32_t cpt_idx = event_table[evt_idx].data.cpt_idx[0][i];
				if (var_cpu_idx > -1) {
			        ls1.cpu = event_table[evt_idx].data.vals[i][var_cpu_idx];
				} else {
			        ls1.cpu = -1;
				}
			    ls1.cpt_idx = cpt_idx;
				ls1.period  = 1.0;
			}
			ls1.period = dura;
			if (prf_obj.file_type != FILE_TYP_ETW) {
				std::string ostr;
				std::string fsp;
				bool got_follow = false;
				if (follow_proc.size() > 0) {
					if (prf_obj.samples[prf_idx].comm.find(follow_proc) != std::string::npos) {
						prf_obj.samples[prf_idx].flags |= GOT_FOLLOW_PROC;
						//printf("got fllw0 at %s %d\n", __FILE__, __LINE__);
					} else {
						fsp = follow_proc;
					}
				}
				prf_sample_to_string(prf_idx, ostr, prf_obj);
				//ls1.text = ostr;
				int tidx;
				ls1.text = "_P_"; // placeholder will be replaced by main.js with trace record
				tidx = (int)hash_escape_string(callstack_hash, callstack_vec, ls1.text) - 1;
				ls1.text_arr.push_back(tidx);
				ls1.text = "<br>" + prf_obj.samples[prf_idx].extra_str;
				tidx = (int)hash_escape_string(callstack_hash, callstack_vec, ls1.text) - 1;
				ls1.text_arr.push_back(tidx);
				if (prf_obj.samples[prf_idx].args.size() > 0) {
					ls1.text = "<br>";
					tidx = (int)hash_escape_string(callstack_hash, callstack_vec, ls1.text) - 1;
					ls1.text_arr.push_back(tidx);
					for (uint32_t k=0; k < prf_obj.samples[prf_idx].args.size(); k++) {
						ls1.text = " " + prf_obj.samples[prf_idx].args[k];
						tidx = (int)hash_escape_string(callstack_hash, callstack_vec, ls1.text) - 1;
						ls1.text_arr.push_back(tidx);
					}
				}
				tidx = prf_obj.samples[prf_idx].line_num;
				ls1.text_arr.push_back(-tidx);
				ls1.text = "";
				if (prf_obj.samples[prf_idx].callstack.size() > 0) {
					std::vector <int> callstacks;
					std::vector <std::string> prefx;
					got_follow = prf_mk_callstacks(prf_obj, prf_idx, callstacks, __LINE__, prefx, fsp, ls1.csi);
					ls1.callstack_str_idxs = callstacks;
					if (got_follow) {
						prf_obj.samples[prf_idx].flags |= GOT_FOLLOW_PROC;
						//printf("got fllw1 at %s %d\n", __FILE__, __LINE__);
					}
				}
			} else if (stk_idx != UINT32_M1 && prf_obj.file_type == FILE_TYP_ETW) {
				std::string fsp;
			   	uint32_t cpt_idx = ls1.cpt_idx;
				uint32_t prf_evt_idx = event_table[evt_idx].event_idx_in_file;
				if (follow_proc.size() > 0 && cpt_idx != UINT32_M1) {
			    	uint32_t file_tag_idx = event_table[evt_idx].file_tag_idx;
					std::string comm = comm_pid_tid_vec[file_tag_idx][cpt_idx].comm;
					if (comm.find(follow_proc) != std::string::npos) {
						prf_obj.etw_evts_set[prf_evt_idx][i].flags |= GOT_FOLLOW_PROC;
						//printf("got fllw0 at %s %d\n", __FILE__, __LINE__);
					} else {
						fsp = follow_proc;
					}
				}
				int jj = prv_nxt[i].prv;
				if(jj > -1 && prf_obj.etw_evts_set[prf_evt_idx][jj].cs_idx_beg != UINT32_M1) {
					bool got_follow = false;
					std::vector <int> callstacks;
					std::vector <std::string> prefx;
					got_follow = etw_mk_callstacks(prf_evt_idx, prf_obj, jj, stk_mod_rtn_idx, callstacks, __LINE__, prefx, fsp, ls1.csi);
					ls1.callstack_str_idxs = callstacks;
					if (got_follow) {
						// I'm not sure this will ever happen but...
						prf_obj.etw_evts_set[prf_evt_idx][i].flags |= GOT_FOLLOW_PROC;
						//printf("got fllw1 at %s %d\n", __FILE__, __LINE__);
					}
				}
				ls1.text = "line " + std::to_string(prf_obj.etw_evts_set[prf_evt_idx][i].txt_idx);
				if (add_2_extra.size() > 0) {
					uint32_t data_idx = prf_obj.etw_evts_set[prf_evt_idx][i].data_idx;
					for (uint32_t ii=0; ii < add_2_extra.size(); ii++) {
						uint32_t fld_idx = add_2_extra[ii];
						uint32_t str_idx = event_table[prf_evt_idx].data.vals[i][fld_idx];
						ls1.text += "<br>" + event_table[prf_evt_idx].vec_str[(uint32_t)(str_idx-1)];
					}
				}
			} else if (stk_idx == UINT32_M1 && prf_obj.file_type == FILE_TYP_ETW) {
				uint32_t prf_evt_idx = event_table[evt_idx].event_idx_in_file;
				ls1.text = "line " + std::to_string(prf_obj.etw_evts_set[prf_evt_idx][i].txt_idx);
				if (add_2_extra.size() > 0) {
					uint32_t data_idx = prf_obj.etw_evts_set[prf_evt_idx][i].data_idx;
					for (uint32_t ii=0; ii < add_2_extra.size(); ii++) {
						uint32_t fld_idx = add_2_extra[ii];
						uint32_t str_idx = event_table[prf_evt_idx].data.vals[i][fld_idx];
						ls1.text += "<br>" + event_table[prf_evt_idx].vec_str[(uint32_t)(str_idx-1)];
					}
				}
			}
			ls1.x[1] = ls1.x[0];
			ls1.y[1] = ls1.y[0]+delta;
			ls1.cat = (int)by_var_idx_val;
			ls1.subcat = 1;
			ls1.prf_idx = prf_idx;
			ls1.typ = SHAPE_LINE;
			bool do_it = true;
			if (options.tm_clip_end_valid == CLIP_LVL_2 &&
				   ((ls1.x[0]+ts0) < options.tm_clip_beg || (ls1.x[0]+ts0) > options.tm_clip_end)) {
				do_it = false;
			}
			if (do_it) {
				chart_lines_ck_rng(ls1.x[0], ls1.y[0], ts0, ls1.cat, ls1.subcat, 0.0, (ls1.x[1]-ls1.x[0]), ls1.fe_idx);
				chart_lines_ck_rng(ls1.x[1], ls1.y[1], ts0, ls1.cat, ls1.subcat, 1.0, 0.0, ls1.fe_idx);
				ch_lines.line.push_back(ls1);
			}
		}
		if (chart_type == CHART_TYPE_BLOCK && doing_pct_busy_by_cpu == 1 && idle_pid) {
			continue;
		}
		lines_str ls0;
		ls0.x[0] = x;
		ls0.x[1] = event_table[evt_idx].data.ts[i].ts - ts0;
#if 1
		if (options.tm_clip_end_valid == CLIP_LVL_2 && (ls0.x[1]+ts0) > options.tm_clip_end) {
			ls0.x[1] = options.tm_clip_end - ts0;
		}
#endif
		if (chart_type == CHART_TYPE_LINE) {
			ls0.y[0] = var_val;
			ls0.y[1] = var_val;
			ls0.typ  = SHAPE_LINE;
		} else if (chart_type == CHART_TYPE_STACKED) {
			ls0.y[0] = var_val;
			ls0.y[1] = var_val;
			ls0.typ  = SHAPE_RECT;
		} else {
			ls0.y[0] = y;
			ls0.y[1] = hi;
			ls0.typ  = SHAPE_RECT;
		}
		int tidx0;
		ls0.text = comm;
		tidx0 = (int)hash_escape_string(callstack_hash, callstack_vec, ls0.text) - 1;
		ls0.text_arr.push_back(tidx0);
		ls0.pid  = pid_num;
		if (prf_obj.file_type != FILE_TYP_ETW) {
			ls0.fe_idx  = prf_obj.samples[prf_idx].fe_idx;
			ls0.cpt_idx = event_table[evt_idx].data.cpt_idx[0][i];
			ls0.period  = (double)prf_obj.samples[prf_idx].period;
			ls0.cpu     = (int)prf_obj.samples[prf_idx].cpu;
			ls0.text_arr.push_back(-prf_obj.samples[prf_idx].line_num);
			ls0.text = "<br>" + prf_obj.samples[prf_idx].extra_str;
			tidx0 = (int)hash_escape_string(callstack_hash, callstack_vec, ls0.text) - 1;
			ls0.text_arr.push_back(tidx0);
			ls0.text = "";
		} else {
			ls0.fe_idx  = get_fe_idxm1(prf_obj, event_table[evt_idx].event_name_w_area, __LINE__, true, event_table[evt_idx]);
			uint32_t cpt_idx = event_table[evt_idx].data.cpt_idx[0][i];
			if (var_cpu_idx > -1) {
				ls0.cpu    = event_table[evt_idx].data.vals[i][var_cpu_idx];
			} else {
				ls0.cpu    = -1;
			}
			ls0.period  = 1.0;
			uint32_t prf_evt_idx = event_table[evt_idx].event_idx_in_file;
			ls0.text = "line " + std::to_string(prf_obj.etw_evts_set[prf_evt_idx][i].txt_idx);
			if (add_2_extra.size() > 0) {
				uint32_t data_idx = prf_obj.etw_evts_set[prf_evt_idx][i].data_idx;
				for (uint32_t ii=0; ii < add_2_extra.size(); ii++) {
					uint32_t fld_idx = add_2_extra[ii];
					uint32_t str_idx = event_table[prf_evt_idx].data.vals[i][fld_idx];
					ls0.text += "<br>" + event_table[prf_evt_idx].vec_str[(uint32_t)(str_idx-1)];
				}
			}
			ls0.cpt_idx = cpt_idx;
		}
		ls0.prf_idx = prf_idx;
		ls0.cat     = (int)by_var_idx_val;
		ls0.subcat  = 0;
		chart_lines_ck_rng(ls0.x[0], ls0.y[0], ts0, ls0.cat, ls0.subcat, 0.0, (ls0.x[1]-ls0.x[0]), ls0.fe_idx);
		chart_lines_ck_rng(ls0.x[1], ls0.y[1], ts0, ls0.cat, ls0.subcat, 1.0, 0.0, ls0.fe_idx);
		ch_lines.line.push_back(ls0);
	}
	if (chart_type == CHART_TYPE_LINE) {
		printf("ummmm ch_lines subcat_rng[0] sz= %d at %s %d\n", (int)ch_lines.range.subcat_rng[0].size(), __FILE__, __LINE__);
	}
	if (doing_pct_busy_by_cpu != 1 || chart_type == CHART_TYPE_LINE || chart_type == CHART_TYPE_STACKED) {
		return 0;
	}
	bool got_cpt_neg = false;
	for (int i=0; i < prf_obj.features_nr_cpus_online; i++) {
		// if sched_switch didn't occur on a cpu, we can get errors in get_by_var_idx (since the cpu won't have been registered)
		double cpu = i;
		if (event_table[evt_idx].charts[chrt].by_var_hsh[cpu] == 0) {
			hash_dbl_by_var( event_table[evt_idx].charts[chrt].by_var_hsh,
					event_table[evt_idx].charts[chrt].by_var_vals, cpu,
					event_table[evt_idx].charts[chrt].by_var_sub_tots, 0.0);
			std::string by_var_str = std::to_string(i);
			if (event_table[evt_idx].charts[chrt].by_var_strs.size() < event_table[evt_idx].charts[chrt].by_var_vals.size()) {
				event_table[evt_idx].charts[chrt].by_var_strs.resize(event_table[evt_idx].charts[chrt].by_var_vals.size());
				event_table[evt_idx].charts[chrt].by_var_strs.back() = by_var_str;
			}
			if (event_table[evt_idx].charts[chrt].by_var_sub_tots.size() < event_table[evt_idx].charts[chrt].by_var_vals.size()) {
				event_table[evt_idx].charts[chrt].by_var_sub_tots.resize(event_table[evt_idx].charts[chrt].by_var_vals.size(), 0.0);
			}
		}
	}

	if (prf_obj.file_type != FILE_TYP_ETW) {
		if (chart_defaults.dont_show_events_on_cpu_busy_if_samples_exceed > 0 &&
			tot_samples > chart_defaults.dont_show_events_on_cpu_busy_if_samples_exceed && options.tm_clip_beg_valid == 0 && options.tm_clip_end_valid == 0) {
			static int first_tm = 1;
			if (first_tm == 1) {
				fprintf(stderr, "skipping adding other events to cpu_busy chart due to sample count(%d) > %d at %s %d\n",
					tot_samples, chart_defaults.dont_show_events_on_cpu_busy_if_samples_exceed, __FILE__, __LINE__);
				first_tm = 0;
			}
			return 0;
		}
		for (uint32_t i=0; i < prf_obj.samples.size(); i++) {
			if (prf_obj.samples[i].evt_idx == event_table[evt_idx].event_idx_in_file) {
				// already did these events so skip
				continue;
			}
			double ts = 1.0e-9 * (double)prf_obj.samples[i].ts;
#if 1
			if (options.tm_clip_beg_valid == CLIP_LVL_2 && ts < options.tm_clip_beg) {
				continue;
			}
			if (options.tm_clip_end_valid == CLIP_LVL_2 && ts > options.tm_clip_end) {
				continue;
			}
#endif
			lines_str ls0;

			int cpt_idx = -1;
			uint32_t file_tag_idx = prf_obj.file_tag_idx;
			if (cpt_idx == -1 && prf_obj.samples[i].comm.length() > 0 &&
					(int)prf_obj.samples[i].pid > -1 &&
					(int)prf_obj.samples[i].tid > -1) {
				cpt_idx = (int)hash_comm_pid_tid(comm_pid_tid_hash[file_tag_idx], comm_pid_tid_vec[file_tag_idx],
					prf_obj.samples[i].comm, prf_obj.samples[i].pid, prf_obj.samples[i].tid) - 1;
			}
			//int e_idx = prf_obj.samples[i].evt_idx;
			//if (e_idx != -1 && prf_obj.events[e_idx].event_name == "ext4_direct_IO") {
			//	continue;
			//}
			//shape_str ss;
			double cpu = prf_obj.samples[i].cpu;
			ls0.x[0] = ts - ts0;
			ls0.y[0] = cpu + delta;
			ls0.x[1] = ls0.x[0];
			ls0.y[1] = ls0.y[0]+delta;
			//if (ls0.cpt_idx < 0) {
				cpt_idx = (int)hash_comm_pid_tid(comm_pid_tid_hash[file_tag_idx], comm_pid_tid_vec[file_tag_idx],
					prf_obj.samples[i].comm, prf_obj.samples[i].pid, prf_obj.samples[i].tid) - 1;
				ls0.cpt_idx = cpt_idx;
			//}
			ls0.period  = (double)prf_obj.samples[i].period;
			ls0.cpu     = (int)prf_obj.samples[i].cpu;
			ls0.fe_idx  = (int)prf_obj.samples[i].fe_idx;
			ls0.prf_idx = (int)i;
			ls0.pid     = -1; // want to show all the events...
			ls0.typ     = SHAPE_LINE;
			std::string ostr;
			prf_sample_to_string(i, ostr, prf_obj);
			int tidx0;
			ls0.text = "_P_"; // placeholder will be replaced by main.js with trace record
			tidx0 = (int)hash_escape_string(callstack_hash, callstack_vec, ls0.text) - 1;
			ls0.text_arr.push_back(tidx0);
			if (prf_obj.samples[i].args.size() > 0) {
				ls0.text = "<br>";
				tidx0 = (int)hash_escape_string(callstack_hash, callstack_vec, ls0.text) - 1;
				ls0.text_arr.push_back(tidx0);
				for (uint32_t k=0; k < prf_obj.samples[i].args.size(); k++) {
					ls0.text = " " + prf_obj.samples[i].args[k];
					tidx0 = (int)hash_escape_string(callstack_hash, callstack_vec, ls0.text) - 1;
					ls0.text_arr.push_back(tidx0);
				}
			}
			ls0.text = "<br>" + prf_obj.samples[i].extra_str;
			tidx0 = (int)hash_escape_string(callstack_hash, callstack_vec, ls0.text) - 1;
			ls0.text_arr.push_back(tidx0);
			ls0.text_arr.push_back(-prf_obj.samples[i].line_num);
			ls0.text = "";
			if (prf_obj.samples[i].callstack.size() > 0) {
				std::vector <int> callstacks;
				std::vector <std::string> prefx;
				std::string fllw;
				prf_mk_callstacks(prf_obj, i, callstacks, __LINE__, prefx, fllw, ls0.csi);
				ls0.callstack_str_idxs = callstacks;
			}
			//printf("evt_nm= %s cpu= %f at %s %d\n", event_table[evt_idx].event_name_w_area.c_str(), cpu, __FILE__, __LINE__);
			int by_var_idx_val2 = (int)get_by_var_idx(event_table[evt_idx].charts[chrt].by_var_hsh, cpu, __LINE__);
			ls0.cat = by_var_idx_val2;
			ls0.subcat = 1;
			chart_lines_ck_rng(ls0.x[0], ls0.y[0], ts0, ls0.cat, ls0.subcat, 0.0, (ls0.x[1]-ls0.x[0]), ls0.fe_idx);
			chart_lines_ck_rng(ls0.x[1], ls0.y[1], ts0, ls0.cat, ls0.subcat, 1.0, 0.0, ls0.fe_idx);
			ch_lines.line.push_back(ls0);
		}
		if (got_cpt_neg == true) {
			printf("quiting due to got_cpt_neg at %s %d\n", __FILE__, __LINE__);
			exit(1);
		}
	}

	if (prf_obj.file_type == FILE_TYP_ETW) {
		for (uint32_t set_idx=0; set_idx < prf_obj.etw_evts_set.size(); set_idx++) {
			if (set_idx == event_table[evt_idx].event_idx_in_file) {
				printf("skipping set_idx= %d evt= %s at %s %d\n", set_idx, prf_obj.etw_evts_vec[set_idx].c_str(), __FILE__, __LINE__);
				continue; // already did this one
			}
			if (prf_obj.etw_evts_set[set_idx].size() == 0) {
				continue;
			}
			if (ETW_events_to_skip_hash[prf_obj.etw_evts_vec[set_idx]] != 0) {
				continue;
			}
			std::string evt_nm = prf_obj.etw_evts_vec[set_idx];
			if (evt_nm == "Stack") {
				// the callstacks are attached to the events which caused them so don't add a line here
				continue;
			}
			uint32_t fe_idx, cpu_idx, comm_pid_idx, tid_idx, comm_pid_idx2, tid_idx2;
			fe_idx = cpu_idx = comm_pid_idx = tid_idx = comm_pid_idx2 = tid_idx2 = UINT32_M1;

			if (set_idx >= prf_obj.events.size()) {
				printf("mess up here. set_idx= %d, events.sz= %d. bye at %s %d\n",
					set_idx, (int)prf_obj.events.size(), __FILE__, __LINE__);
				exit(1);
			}
			for (uint32_t i=0; i < prf_obj.events[set_idx].etw_cols.size(); i++) {
				std::string str = prf_obj.events[set_idx].etw_cols[i];
				if (str == "CPU") {
					cpu_idx = i;
				}
				if (str == "Process Name ( PID)" || str == "New Process Name ( PID)") {
					comm_pid_idx = i;
				}
				if (str == "Old Process Name ( PID)") {
					comm_pid_idx2 = i;
				}
				if (str == "ThreadID" || str == "New ThreadID") {
					tid_idx = i;
				}
				if (str == "Old ThreadID") {
					tid_idx2 = i;
				}
			}
			fe_idx = get_fe_idxm1(prf_obj, evt_nm, __LINE__, false, event_table[evt_idx]);
			if (fe_idx == UINT32_M1) {
				std::string str = prf_obj.filename_bin + " " + prf_obj.filename_text + " " + evt_nm;
				uint32_t p_o_idx = UINT32_M1;
				uint32_t file_tag_idx = prf_obj.file_tag_idx;
				if (file_tag_idx == UINT32_M1) {
					printf("file_tag_idx= %d at %s %d\n", file_tag_idx, __FILE__, __LINE__);
					exit(1);
				}
				printf("p_o_idx= %d at %s %d\n", p_o_idx, __FILE__, __LINE__);
				fe_idx = (int)hash_flnm_evt(flnm_evt_hash[file_tag_idx], flnm_evt_vec[file_tag_idx],
						str, prf_obj.filename_bin, prf_obj.filename_text,  evt_nm, p_o_idx, evt_idx, -1) - 1;
			}
			printf("set_idx= %d, fe_idx= %d at %s %d\n", set_idx, fe_idx, __FILE__, __LINE__);

			for (uint32_t i=0; i < prf_obj.etw_evts_set[set_idx].size(); i++) {
				lines_str ls0;

				//shape_str ss;
				double cpu = 0;
				std::string comm, pid_str;
				std::string comm2, pid_str2;
				uint32_t cpt_idx, pid, tid, cpt_idx2, pid2, tid2;
				cpt_idx = pid = tid = cpt_idx2 = pid2 = tid2 = UINT32_M1;
				uint32_t data_idx = prf_obj.etw_evts_set[set_idx][i].data_idx;
				uint32_t file_tag_idx = prf_obj.file_tag_idx;
				comm = pid_str = comm2 = pid_str2 = "";
				if (comm_pid_idx != UINT32_M1 && tid_idx != UINT32_M1) {
					if (-1 == etw_split_comm(prf_obj.etw_data[data_idx][comm_pid_idx], comm, pid_str,
							(double)prf_obj.etw_evts_set[set_idx][i].ts)) {
						std::string str2 = prf_obj.events[set_idx].event_name;
						fprintf(stderr, "failed for evt= %s ts= %s, evt2= %s, evt_nm= %s at %s %d\n",
							prf_obj.etw_data[data_idx][0].c_str(),
							prf_obj.etw_data[data_idx][1].c_str(), str2.c_str(), evt_nm.c_str(),
							__FILE__, __LINE__);
						for (uint32_t i=0; i < prf_obj.events[set_idx].etw_cols.size(); i++) {
							printf("col[%d]= %s\n", i, prf_obj.events[set_idx].etw_cols[i].c_str());
						}
						exit(1);
					}
					pid = atoi(pid_str.c_str());
					if (tid_idx != UINT32_M1) {
						tid = atoi(prf_obj.etw_data[data_idx][tid_idx].c_str());
					}
					//std::string str = comm + " " + pid_str + "/" + std::to_string(tid);
					cpt_idx = (int)hash_comm_pid_tid(comm_pid_tid_hash[file_tag_idx], comm_pid_tid_vec[file_tag_idx],
							comm, pid, tid) - 1;
				}
				if (comm_pid_idx2 != UINT32_M1 && tid_idx2 != UINT32_M1) {
					if (-1 == etw_split_comm(prf_obj.etw_data[data_idx][comm_pid_idx2], comm2, pid_str2,
							(double)prf_obj.etw_evts_set[set_idx][i].ts)) {
						fprintf(stderr, "failed for evt= %s ts= %s, evt_nm= %s at %s %d\n",
							prf_obj.etw_data[data_idx][0].c_str(),
							prf_obj.etw_data[data_idx][1].c_str(), evt_nm.c_str(),
							__FILE__, __LINE__);
						exit(1);
					}
					pid2 = atoi(pid_str2.c_str());
					if (tid_idx2 != UINT32_M1) {
						tid2 = atoi(prf_obj.etw_data[data_idx][tid_idx2].c_str());
					}
					cpt_idx2 = (int)hash_comm_pid_tid(comm_pid_tid_hash[file_tag_idx], comm_pid_tid_vec[file_tag_idx],
						comm2, pid2, tid2) - 1;
				}
				if (cpu_idx != UINT32_M1) {
					cpu = atoi(prf_obj.etw_data[data_idx][cpu_idx].c_str());
				}
				double ts = prf_obj.tm_beg + 1.0e-6 * (double)prf_obj.etw_evts_set[set_idx][i].ts;
				//double ts = 1.0e-9 * (double)prf_obj.samples[i].ts;
#if 1
				if (options.tm_clip_beg_valid == CLIP_LVL_2 && (ts+ts0) < options.tm_clip_beg) {
					continue;
				}
				if (options.tm_clip_end_valid == CLIP_LVL_2 && (ts+ts0) > options.tm_clip_end) {
					continue;
				}
#endif
				ls0.x[0] = ts - prf_obj.tm_beg;
				ls0.y[0] = cpu + delta;
				ls0.x[1] = ls0.x[0];
				ls0.y[1] = ls0.y[0]+delta;
				ls0.cpt_idx = cpt_idx;
				ls0.period  = 1.0;
				ls0.cpu     = cpu;
				ls0.fe_idx  = fe_idx;
				ls0.prf_idx = (int)data_idx;
				ls0.pid     = -1; // want to show all the events...
				ls0.typ     = SHAPE_LINE;
				std::string ostr;
				//prf_sample_to_string(i, ostr, prf_obj);
				//ls0.text = ostr;
				ls0.text = "line " + std::to_string(prf_obj.etw_evts_set[set_idx][i].txt_idx);
				if (stk_idx != UINT32_M1 && prf_obj.etw_evts_set[set_idx][i].cs_idx_beg != UINT32_M1) {
					std::vector <int> callstacks;
					std::vector <std::string> prefx;
					std::string fllw;
					etw_mk_callstacks(set_idx, prf_obj, i, stk_mod_rtn_idx, callstacks, __LINE__, prefx, fllw, ls0.csi);
					ls0.callstack_str_idxs = callstacks;
				}
				int by_var_idx_val2 = (int)get_by_var_idx(event_table[evt_idx].charts[chrt].by_var_hsh, cpu, __LINE__);
				ls0.cat = by_var_idx_val2;
				ls0.subcat = 1;
				chart_lines_ck_rng(ls0.x[0], ls0.y[0], ts0, ls0.cat, ls0.subcat, 0.0, (ls0.x[1]-ls0.x[0]), ls0.fe_idx);
				chart_lines_ck_rng(ls0.x[1], ls0.y[1], ts0, ls0.cat, ls0.subcat, 1.0, 0.0, ls0.fe_idx);
				ch_lines.line.push_back(ls0);
			}
		}
	}
	return 0;
}

static std::unordered_map<std::string, uint32_t> chart_categories_hash;
static std::vector <std::string> chart_categories_vec;

static std::string drop_trailing_zeroes(std::string str)
{
	size_t pos = str.find(".");
	if (pos == std::string::npos) {
		return str;
	}
	std::string nstr = str;
	size_t len = str.size();
	for (uint32_t i= len-1; i >= pos; i--) {
		if (nstr.substr(i, 1) == ".") {
			nstr.pop_back();
			return nstr;
		}
		if (nstr.substr(i, 1) == "0") {
			nstr.pop_back();
		} else {
			break;
		}
	}
	return nstr;
}

static int build_shapes_json(std::string file_tag, uint32_t evt_tbl_idx, uint32_t evt_idx,
		uint32_t chrt, std::vector <evt_str> &event_table, std::string &json, int verbose)
{
	double tt0, tt1, tt2, tt3;
	double tmr[10];
	for (uint32_t i=0; i < 10; i++) { tmr[i] = 0.0;}
	tt0 = dclock();
	int var_idx = (int)event_table[evt_idx].charts[chrt].var_idx;
	// main.js looks for '\{ "title":' so be careful changing this string
	//std::string json = "{ ";
	json = "{ ";
	json += "\"title\": \"" + event_table[evt_idx].charts[chrt].title + "\"";
	if (event_table[evt_idx].charts[chrt].options != 0) {
		json += ", \"chart_options\":[";
		for (uint32_t i=0; i < event_table[evt_idx].charts[chrt].options_strs.size(); i++) {
			if (i > 0) {
				json += ",";
			}
			json += "\"" + event_table[evt_idx].charts[chrt].options_strs[i] + "\"";
		}
		json += "]";
	}
	json += ", \"x_label\": \"Time(secs)\"";
	json += ", \"y_fmt\": \""+ event_table[evt_idx].charts[chrt].y_fmt + "\"";
	if (event_table[evt_idx].charts[chrt].tot_line.size() > 0) {
		json += ", \"tot_line\": \""+ event_table[evt_idx].charts[chrt].tot_line + "\"";
	} else {
		json += ", \"tot_line\": \"\"";
	}
	if (verbose)
		printf("tot_line= %s for title= %s at %s %d\n", 
			event_table[evt_idx].charts[chrt].tot_line.c_str(), 
			event_table[evt_idx].charts[chrt].title.c_str(), __FILE__, __LINE__);
	if (event_table[evt_idx].charts[chrt].tot_line_opts.xform.size() > 0) {
		std::string xf = ", \"tot_line_opts_xform\": \""+ event_table[evt_idx].charts[chrt].tot_line_opts.xform + "\"";
		json += xf;
		if (verbose)
			printf("got xform= '%s' at %s %d\n", xf.c_str(), __FILE__, __LINE__);
	}
	if (event_table[evt_idx].charts[chrt].tot_line_opts.yval_fmt.size() > 0) {
		json += ", \"tot_line_opts_yval_fmt\": \""+ event_table[evt_idx].charts[chrt].tot_line_opts.yval_fmt + "\"";
	}
	if (event_table[evt_idx].charts[chrt].tot_line_opts.yvar_fmt.size() > 0) {
		json += ", \"tot_line_opts_yvar_fmt\": \""+ event_table[evt_idx].charts[chrt].tot_line_opts.yvar_fmt + "\"";
	}
	if (event_table[evt_idx].charts[chrt].tot_line_opts.desc.size() > 0) {
		json += ", \"tot_line_opts_desc\": \""+ event_table[evt_idx].charts[chrt].tot_line_opts.desc + "\"";
	}
	if (ch_lines.line.size() > 0 && ch_lines.line[0].use_num_denom > -1) {
		std::string s = ", \"tot_line_opts_has_num_den\":1";
		json += s;
	}
	if (event_table[evt_idx].charts[chrt].tot_line.size() > 0 &&
		event_table[evt_idx].charts[chrt].tot_line_opts.scope.size() > 0) {
		std::string cma= "", s = ", \"tot_line_opts_scope\":[";
		uint32_t sz = event_table[evt_idx].charts[chrt].tot_line_opts.scope.size();
		for (uint32_t m=0; m < sz; m++) {
			s += cma;
			s += "\"";
			s += event_table[evt_idx].charts[chrt].tot_line_opts.scope[m];
			s += "\"";
			cma = ",";
		}
		s += "]";
		if (verbose)
			printf("scope= '%s' at %s %d\n", s.c_str(), __FILE__, __LINE__);
		json += s;
	}
	if (options.follow_proc.size() > 0) {
		json += ", \"follow_proc\":[";
		for (uint32_t i=0; i < options.follow_proc.size(); i++) {
			if (i > 0) {
				json += ", ";
			}
			json += "\"" + options.follow_proc[i] + "\"";
		}
		json += "]";
	}
	json += ", \"pixels_high\": "+ std::to_string(event_table[evt_idx].charts[chrt].pixels_high);
	json += ", \"file_tag\": \"" + file_tag + "\"";
	if (event_table[evt_idx].charts[chrt].marker_type.size() > 0) {
		json += ", \"marker_type\": \"" + event_table[evt_idx].charts[chrt].marker_type + "\"";
	}
	if (event_table[evt_idx].charts[chrt].marker_size.size() > 0) {
		json += ", \"marker_size\": \"" + event_table[evt_idx].charts[chrt].marker_size + "\"";
	}
	if (event_table[evt_idx].charts[chrt].marker_ymin.size() > 0) {
		std::string ymin_str = ", \"marker_ymin\": \"" + event_table[evt_idx].charts[chrt].marker_ymin + "\"";
		//printf("chart: marker_ymin str= %s at %s %d\n", ymin_str.c_str(), __FILE__, __LINE__);
		//json += ", \"marker_ymin\": \"" + event_table[evt_idx].charts[chrt].marker_ymin + "\"";
		json += ymin_str;
	}
	if (event_table[evt_idx].charts[chrt].marker_connect.size() > 0) {
		json += ", \"marker_connect\": \"" + event_table[evt_idx].charts[chrt].marker_connect + "\"";
	}
	if (event_table[evt_idx].charts[chrt].marker_text.size() > 0) {
		json += ", \"marker_text\": \"" + event_table[evt_idx].charts[chrt].marker_text + "\"";
	}
	json += ", \"chart_tag\": \"" + event_table[evt_idx].charts[chrt].chart_tag + "\"";
	if (event_table[evt_idx].charts[chrt].y_label.size() == 0) {
		json += ", \"y_label\": \"" + event_table[evt_idx].flds[var_idx].name + "\"";
	} else {
		json += ", \"y_label\": \"" + event_table[evt_idx].charts[chrt].y_label + "\"";
	}

	if (ch_lines.prf_obj != 0 && ch_lines.prf_obj->map_cpu_2_core.size() > 0) {
		std::string topo, skt= ", \"socket\":0}", cor = "{ \"core\":";
		topo = ", \"map_cpu_2_core\":[";
		for (uint32_t i=0; i < ch_lines.prf_obj->map_cpu_2_core.size(); i++) {
			std::string cma = (i > 0 ? "," : "");
			topo += cma + cor + std::to_string(ch_lines.prf_obj->map_cpu_2_core[i]) + skt;
		}
		topo += "]";
		if (verbose > 0) {
			printf("topo= '%s' at %s %d\n", topo.c_str(), __FILE__, __LINE__);
		}
		json += topo;
	}
	json += ", \"file_tag_idx\": " + std::to_string(event_table[evt_idx].file_tag_idx);
	json += ", \"prf_obj_idx\": " + std::to_string(event_table[evt_idx].prf_obj_idx);
	json += ", \"chart_type\": \"" + event_table[evt_idx].charts[chrt].chart_type + "\"";
	std::string cat = event_table[evt_idx].charts[chrt].chart_category;
	if (cat.size() == 0) {
		cat = "general";
	}
	hash_string(chart_categories_hash,chart_categories_vec, cat);
	json += ", \"chart_category\": \"" + cat + "\"";
	json += ", \"y_by_var\": \"" + event_table[evt_idx].charts[chrt].by_var + "\"";
	double uclip_beg= 0, uclip_end= 0;
	if (options.tm_clip_beg_valid == CLIP_LVL_1) {
		uclip_beg = options.tm_clip_beg - ch_lines.range.ts0;
	}
	if (options.tm_clip_end_valid == CLIP_LVL_1) {
		uclip_end = options.tm_clip_end - ch_lines.range.ts0;
	}
	json += ", \"ts_initial\": { \"ts\":" + do_string_with_decimals(ch_lines.range.ts0, 9) +
		", \"ts0x\":" + do_string_with_decimals(0.0, 9) +
		", \"tm_beg_offset_due_to_clip\":" + do_string_with_decimals(uclip_beg, 9) +
		", \"tm_end_offset_due_to_clip\":" + do_string_with_decimals(uclip_end, 9) + "}";
	json += ", \"x_range\": { \"min\":" + std::to_string(ch_lines.range.x_range[0]) +
		", \"max\":" + std::to_string(ch_lines.range.x_range[1]) + "}";
	json += ", \"y_range\": { \"min\":" + std::to_string(ch_lines.range.y_range[0]) +
		", \"max\":" + std::to_string(ch_lines.range.y_range[1]) + "}";
	if (verbose) {
		printf("x_rng= %f, %f, y_rng= %f, %f\n",
			ch_lines.range.x_range[0],
			ch_lines.range.x_range[1],
			ch_lines.range.y_range[0],
			ch_lines.range.y_range[1]);
	}
	uint32_t by_sz = (uint32_t)event_table[evt_idx].charts[chrt].by_var_sub_tots.size();
	std::string var_strs;
	json += ", \"block_delta\":"+std::to_string(block_delta) + ", \"myxs\": {";
	for (uint32_t i=0; i < by_sz; i++) {
		if (i > 0) {
			var_strs += ", ";
		}
		std::string use_str;
		if (event_table[evt_idx].charts[chrt].by_var_strs.size() == 0) {
			use_str = event_table[evt_idx].charts[chrt].var_name;
		} else {
			use_str = event_table[evt_idx].charts[chrt].by_var_strs[i];
		}
		replace_substr(use_str, "\"", "", verbose);
		var_strs += "\"" + use_str + "\":\"x_" + std::to_string(i) + "\"";
	}
	if (by_sz != ch_lines.legend.size()) {
		fprintf(stderr, "mess up here. Expected event_table[%d].charts[%d].by_var_sub_tots.size()= %d to be == to ch_lines.legend.size()= %d at %s %d\n",
			evt_idx, chrt, by_sz, (int)ch_lines.legend.size(), __FILE__, __LINE__);
		fprintf(stderr, "for chart titled= %s at %s %d\n", event_table[evt_idx].charts[chrt].title.c_str(), __FILE__, __LINE__);
		fprintf(stderr, "proceeding but there may be problems...\n");
		//exit(1);
	}
	json += var_strs + "}";
	json += ", \"myshapes\":[";
	std::string zero = "0";
	std::string ch_lines_line_str;
	std::vector <double> ovr_totals;
	if (event_table[evt_idx].charts[chrt].chart_tag == "SYSCALL_TIME_CHART") {
		ovr_totals.resize(by_sz, 0.0);
	}
	tt1 = dclock();
	std::string nstr;
	double y0_prv=-1001.99, y1_prv=-1002.99; // all the y values have been >= -1 so far so this should be okay.
	double x0_prv=-1001.99, x1_prv=-1002.99; // all the x values have been >= 0 so far so this should be okay.
	std::string x0, x1, y0, y1;
	std::vector <std::string> typ_vec;
	for (uint32_t i=0; i < SHAPE_MAX; i++) {
		typ_vec.push_back(std::to_string(i));
	}

	int32_t txt_sz=0;
	for (uint32_t i=0; i < ch_lines.line.size(); i++) {
		//tmr[0] = dclock();
		if (i > 0) { ch_lines_line_str += ", "; }
		// order of ival array values must agree with IVAL_* variables in main.js
		if (ovr_totals.size() > 0) {
			ovr_totals[ch_lines.line[i].cat] += ch_lines.line[i].x[1] - ch_lines.line[i].x[0];
		}
		// try optimizing loop.
		// For the cpu_busy chart, there are rectangles and vertical lines (x0==x1)
		// The y values might be the same as the previous values.
		// For line charts, the y0 == y1.
		// These checks let us avoid the costly (over 100,000s of pts) the conv_2_string and get rid of trailing zeroes stuff
		if (ch_lines.line[i].y[0] != y0_prv) {
			y0 = drop_trailing_zeroes(std::to_string(ch_lines.line[i].y[0]));
			y0_prv = ch_lines.line[i].y[0];
		}
		if (ch_lines.line[i].y[0] == ch_lines.line[i].y[1]) {
			if (ch_lines.line[i].y[1] != y1_prv) {
			y1 = y0;
			}
		} else {
			if (ch_lines.line[i].y[1] != y1_prv) {
				y1 = drop_trailing_zeroes(std::to_string(ch_lines.line[i].y[1]));
			}
		}
		y1_prv = ch_lines.line[i].y[1];
		if (ch_lines.line[i].x[0] != x0_prv) {
			// for the 'collect all the samples based on trigger event the x0 and x1 will be the same for the group
			x0 = drop_trailing_zeroes(std::to_string(ch_lines.line[i].x[0]));
			x0_prv = ch_lines.line[i].x[0];
		}
		if (ch_lines.line[i].x[0] == ch_lines.line[i].x[1]) {
			x1 = x0;
		} else {
			if (ch_lines.line[i].x[1] != x1_prv) {
				x1 = drop_trailing_zeroes(std::to_string(ch_lines.line[i].x[1]));
			}
		}
		x1_prv = ch_lines.line[i].x[1];
		// IVAL array
		int shape_follow = 0;
		if (ch_lines.line[i].flags & GOT_FOLLOW_PROC) {
			//printf("got fllw6 chart tag %s at %s %d\n", event_table[evt_idx].charts[chrt].chart_tag.c_str(), __FILE__, __LINE__);
			shape_follow = SHAPE_FOLLOW;
		}
		ch_lines_line_str += std::string("{\"ival\":[") +
				typ_vec[shape_follow|ch_lines.line[i].typ] +
				"," +
				std::to_string(ch_lines.line[i].cpt_idx) +
				"," +
				std::to_string(ch_lines.line[i].fe_idx) +
				"," +
				std::to_string(ch_lines.line[i].cat) +
				"," +
				std::to_string(ch_lines.line[i].subcat) +
				"," +
				drop_trailing_zeroes(std::to_string(ch_lines.line[i].period)) +
				"," +
				std::to_string(ch_lines.line[i].cpu) +
				"],";
		if (event_table[evt_idx].charts[chrt].chart_type == "block" && event_table[evt_idx].charts[chrt].chart_tag == "PCT_BUSY_BY_CPU") {
			if (ch_lines.line[i].typ == SHAPE_LINE && x0 != x1) {
				fprintf(stderr, "got chart_type == block and shape== line but x0= %s != x1= %s. Bye at %s %d\n", x0.c_str(), x1.c_str(), __FILE__, __LINE__);
				exit(1);
			}
			double xcpu=ch_lines.line[i].cpu;
			double y0l=xcpu + block_delta - 0.001;
			double y0h=xcpu + block_delta + 0.001;
			double y1l=xcpu + 2*block_delta - 0.001;
			double y1h=xcpu + 2*block_delta + 0.001;
			double y0t= ch_lines.line[i].y[0];
			double y1t= ch_lines.line[i].y[1];
			if (ch_lines.line[i].typ == SHAPE_LINE && (y0t < y0l || y0t > y0h || y1t < y1l || y1t > y1h)) {
				fprintf(stderr, "got chart_type == block and shape== line but cpu= %.0f y0t= %f, y1t= %f. Bye at %s %d\n", xcpu, y0t, y1t, __FILE__, __LINE__);
				exit(1);
			}
			y0l -= block_delta;
			y0h -= block_delta;
			y1l -= block_delta;
			y1h -= block_delta;
			if (ch_lines.line[i].typ == SHAPE_RECT && (y0t < y0l || y0t > y0h || y1t < y1l || y1t > y1h)) {
				fprintf(stderr, "got chart_type == block and shape== line but cpu= %.0f y0t= %f, y1t= %f. Bye at %s %d\n", xcpu, y0t, y1t, __FILE__, __LINE__);
				exit(1);
			}
			if (ch_lines.line[i].typ == SHAPE_LINE) {
				ch_lines_line_str += "\"pt\":" + x0;
			}
			if (ch_lines.line[i].typ == SHAPE_RECT) {
				ch_lines_line_str += "\"ptx\":[" + x0 + "," + x1 + "]";
			}
                } else {
			ch_lines_line_str += 
				"\"pts\":[" + x0 +
				"," + y0 +
				"," + x1 +
				"," + y1 +
				"]";
                }
#if 0
		if (ch_lines.line[i].cat >= 0 && ch_lines.legend[ch_lines.line[i].cat] == "nanosleep") {
			printf("nano[%d] y0= %.3f, y1= %.3f at %s %d\n", i, 
				ch_lines.line[i].y[0], ch_lines.line[i].y[1], __FILE__, __LINE__);
		}
#endif
		//tmr[1] = dclock();
		if (ch_lines.line[i].use_num_denom > -1) {
#if 0
			double nval= ch_lines.line[i].denom;
			if (nval != 0.0) {
				nval = ch_lines.line[i].numerator / nval;
			} else {
				nval = 0.0;
			}
#endif
			ch_lines_line_str += std::string(", \"num\":") +
				drop_trailing_zeroes(std::to_string(ch_lines.line[i].numerator));
			ch_lines_line_str += std::string(", \"den\":") +
				drop_trailing_zeroes(std::to_string(ch_lines.line[i].denom));
#if 0
			printf("ch_lines.line[%d].numerator= %f, denom= %f, nval= %f, oval= %f at %s %d\n",
				i, ch_lines.line[i].numerator, ch_lines.line[i].denom, nval,
				ch_lines.line[i].y[1], __FILE__, __LINE__);
#endif
		}
		//tmr[2] = dclock();
		if (ch_lines.line[i].text.size() > 0) {
			//printf("ch_lines.line[%d].text= %s at %s %d\n", i, ch_lines.line[i].text.c_str(), __FILE__, __LINE__);
			int txt_idx = (int)hash_escape_string(callstack_hash, callstack_vec, ch_lines.line[i].text) - 1;
			ch_lines_line_str += ",\"txtidx\":" + std::to_string(txt_idx);
			txt_sz += (int)ch_lines.line[i].text.size();
		}
		if (ch_lines.line[i].text_arr.size() > 0) {
			ch_lines_line_str += ",\"taa\":[";
			for (uint32_t kk=0; kk < ch_lines.line[i].text_arr.size(); kk++) {
				if (kk > 0) { ch_lines_line_str += ","; }
				ch_lines_line_str += std::to_string(ch_lines.line[i].text_arr[kk]);
			}
			ch_lines_line_str += "]";
		}
		std::string cs_txt;
#if 0
		for (uint32_t j=0; j < ch_lines.line[i].callstack_str_idxs.size(); j++) {
			if (j > 0) {
				cs_txt += ",";
			}
			cs_txt += std::to_string(ch_lines.line[i].callstack_str_idxs[j]);
		}
		if (cs_txt.size() > 0) {
			cs_txt = ",\"cs_strs\":[" + cs_txt + "]";
		}
#endif
		if (ch_lines.line[i].csi != -1) {
			cs_txt = ",\"csi\":" + std::to_string(ch_lines.line[i].csi);
		}
		ch_lines_line_str += cs_txt + "}";
		//tmr[3] = dclock();
		//tmr[5] += tmr[1] - tmr[0];
		//tmr[6] += tmr[2] - tmr[1];
		//tmr[7] += tmr[3] - tmr[2];
	}
	tt2 = dclock();
	ch_lines_line_str += "]";
	fprintf(stderr, "for chart= %s txt_sz= %d tot_sz= %d MB at %s %d\n", event_table[evt_idx].charts[chrt].chart_tag.c_str(), txt_sz, (int)(ch_lines_line_str.size()/(1024*1024)), __FILE__, __LINE__);
#if 0
	if (event_table[evt_idx].charts[chrt].chart_tag == "VMSTAT_MEM_cHART") {
		printf("%s ch_line data at %s %d:\n%s\n",
				event_table[evt_idx].charts[chrt].chart_tag.c_str(), __FILE__, __LINE__, ch_lines_line_str.c_str());
	}
#endif
	json += ch_lines_line_str;
	json += "," + build_proc_string(evt_idx, chrt, event_table);
	json += "," + build_flnm_evt_string(evt_tbl_idx, evt_idx, event_table, verbose);
	if (verbose)
		printf("title= %s\n",  event_table[evt_idx].charts[chrt].title.c_str());
	std::string sc_rng = ", \"subcat_rng\":[";
	bool did_line=false;
	for (uint32_t i=0; i < ch_lines.range.subcat_rng.size(); i++) {
		for (uint32_t j=0; j < ch_lines.range.subcat_rng[i].size(); j++) {
			if (did_line) { sc_rng += ", "; }
			std::string legnd, sbcat;
			std::string ev = "evt unknown";
			uint32_t file_tag_idx = UINT32_M1;
			if (ch_lines.prf_obj != 0) {
				file_tag_idx = ch_lines.prf_obj->file_tag_idx;
			}
			uint32_t fe_idx = ch_lines.range.subcat_rng[i][j].fe_idx;
			if (file_tag_idx != UINT32_M1 && fe_idx != UINT32_M1) {
				ev = flnm_evt_vec[file_tag_idx][fe_idx].evt_str;
			}

			// we can have some entries (like a cpu) with no samples. See if we can pick up a string from elsewhere
			if (i < ch_lines.legend.size()) {
				legnd = ch_lines.legend[i];
			} else {
				if (i < event_table[evt_idx].charts[chrt].by_var_strs.size()) {
					legnd = event_table[evt_idx].charts[chrt].by_var_strs[i];
				} else {
					legnd =  "unknwn "+std::to_string(i);
				}
				printf("using legnd= %s at %s %d\n", legnd.c_str(), __FILE__, __LINE__);
			}
			if (i >= ch_lines.range.subcat_rng.size()) {
				printf("err: ch_lines.range.subcat_rng.size()= %d, i= %d at %s %d\n",
					(int)ch_lines.range.subcat_rng.size(), i, __FILE__, __LINE__);
			}
			if (j >= ch_lines.range.subcat_rng[i].size()) {
				printf("err: ch_lines.range.subcat_rng[%d].size()= %d, j= %d at %s %d\n",
					i, (int)ch_lines.range.subcat_rng[i].size(), j, __FILE__, __LINE__);
			}
			if (i >= ch_lines.subcats.size()) {
				printf("err: ch_lines.subcats.size()= %d, i= %d at %s %d\n",
					(int)ch_lines.subcats.size(), i, __FILE__, __LINE__);
				sbcat = "unknown "+std::to_string(i);
			}
			if (j >= ch_lines.subcats[i].size()) {
				printf("err: ch_lines.subcats[%d].size()= %d, j= %d at %s %d\n",
					i, (int)ch_lines.subcats[i].size(), j, __FILE__, __LINE__);
				sbcat = "unknown "+std::to_string(i) + " " + std::to_string(j);
			}
#if 0
			if (i < ch_lines.subcats.size() && j < ch_lines.subcats[i].size()) {
				printf("subcats[%d][%d]= %s at %s %d\n", i, j, ch_lines.subcats[i][j].c_str(), __FILE__, __LINE__);
			}
#endif
			double ux0, ux1;
			ux0 = ch_lines.range.subcat_rng[i][j].x0;
			ux1 = ch_lines.range.subcat_rng[i][j].x1;
#if 0
			// this needs work
			if (options.tm_clip_beg_valid == CLIP_LVL_2) {
				double ubeg = options.tm_clip_beg - ch_lines.range.ts0;
				if (ux1 < ubeg) {
					did_line = false;
					continue;
				}
				if (ux0 < ubeg) {
					ux0 = ubeg;
				}
			}
			if (options.tm_clip_end_valid == CLIP_LVL_2) {
				double uend = options.tm_clip_end - ch_lines.range.ts0;
				if (ux0 > uend) {
					did_line = false;
					continue;
				}
				if (ux1 > uend) {
					ux1 = uend;
				}
			}
#endif
			//if (event_table[evt_idx].charts[chrt].chart_tag == "SYSCALL_TIME_CHART")
			if (verbose > 0)
			printf("subcat_rng[%d][%d].x0= %f, x1= %f, y0= %f, y1= %f, fe_idx= %d, ev= %s total= %f, txt[%d]= %s, initd= %d\n", i, j,
				//ch_lines.range.subcat_rng[i][j].x0,
				//ch_lines.range.subcat_rng[i][j].x1,
				ux0, ux1,
				ch_lines.range.subcat_rng[i][j].y0,
				ch_lines.range.subcat_rng[i][j].y1,
				ch_lines.range.subcat_rng[i][j].fe_idx,
				ev.c_str(),
				ch_lines.range.subcat_rng[i][j].total,
				(int)i, legnd.c_str(),
				ch_lines.range.subcat_rng[i][j].initd
				);
			if (false && ovr_totals.size() > 0) {
				printf("ovr_total:= %f, lcat= %s, iby_var= %s icat= %d, i= %d at %s %d\n",
						ovr_totals[i], legnd.c_str(),
						event_table[evt_idx].charts[chrt].by_var_strs[i].c_str(),
						ch_lines.line[i].cat, i, __FILE__, __LINE__);
			}
			sc_rng += "{ \"x0\":" + std::to_string(ux0) +
				", \"x1\":" + std::to_string(ux1) +
				", \"y0\":" + std::to_string(ch_lines.range.subcat_rng[i][j].y0) +
				", \"y1\":" + std::to_string(ch_lines.range.subcat_rng[i][j].y1) +
				", \"fe_idx\":" + std::to_string(ch_lines.range.subcat_rng[i][j].fe_idx) +
				", \"event\":\"" + ev + "\"";
			if (ovr_totals.size() > 0) {
				sc_rng +=
				", \"total\":" + std::to_string(ovr_totals[i]);
			} else {
				sc_rng +=
				", \"total\":" + std::to_string(ch_lines.range.subcat_rng[i][j].total);
			}
			sc_rng += ", \"tot_dura\":" + std::to_string(ch_lines.range.subcat_rng[i][j].tot_dura);
			sc_rng += 
				", \"cat\":" + std::to_string(i) +
				", \"subcat\":" + std::to_string(j) +
				", \"cat_text\":\"" + legnd + "\"" +
				", \"subcat_text\":\"" + sbcat + "\""+
				"}";
			did_line = true;
		}
	}
	tmr[3] = dclock();
	sc_rng += "]";
#if 0
	//if (verbose)
	if (event_table[evt_idx].charts[chrt].chart_tag == "VMSTAT_MEM_cHART") {
		printf("subcat_rng(chart_tag=%s)= '%s' at %s %d\n",
				event_table[evt_idx].charts[chrt].chart_tag.c_str(), sc_rng.c_str(), __FILE__, __LINE__);
	}
#endif
	json += sc_rng;
	json += "}";
	// below can print lots
	if (options.show_json > 0) {
		printf("did build_chart_json(): '%s'\n", json.c_str());
	}
	tt3 = dclock();
	std::string ct = event_table[evt_idx].charts[chrt].chart_tag;
	//if (verbose)
	if (ct == "PCT_BUSY_BY_CPU" || ct == "SYSCALL_OUTSTANDING_CHART") {
		fprintf(stderr, "tt1= %.3f, tt2= %.3f, tt3= %.3f tot= %.3f tmr[5]= %.3f, tmr[6]= %.3f, tmr[7]= %.3f at %s %d\n",
			tt1-tt0, tt2-tt1, tt3-tt2, tt3-tt0, tmr[5], tmr[6], tmr[7], __FILE__, __LINE__);
	}

	return 0;
}


static std::string get_str_between_dlms(std::string str_in, std::string lhs, std::string rhs, evt_str &event_table, int line)
{
	size_t pos = str_in.find(lhs);
	if (pos == std::string::npos) {
		printf("something wrong. expected to find string '%s' in str= '%s' for event '%s'. called from line= %d. bye at %s %d\n",
			lhs.c_str(), str_in.c_str(), event_table.event_name.c_str(), line, __FILE__, __LINE__);
		exit(1);
	}
	std::string subs = str_in.substr(pos + lhs.size());
	if (rhs == "__EOL__") {
		return subs;
	}
	pos = subs.find(rhs);
	if (pos == std::string::npos) {
		printf("something wrong. expected to find string '%s' in str= '%s' for event '%s'. called from line= %d. bye at %s %d\n",
			rhs.c_str(), str_in.c_str(), event_table.event_name.c_str(), line, __FILE__, __LINE__);
		exit(1);
	}
	subs = subs.substr(0, pos);
	return subs;
}

static void add_basic_typ(uint32_t flg, std::string str, evt_str &event_table, std::vector <double> &dv)
{
	if (flg & (uint64_t)fte_enum::FLD_TYP_STR) {
		uint32_t idx = hash_string(event_table.hsh_str, event_table.vec_str, str);
		dv.push_back(idx);
	} else if (flg & (uint64_t)fte_enum::FLD_TYP_DBL) {
		double x = atof(str.c_str());
		dv.push_back(x);
	} else if (flg & (uint64_t)fte_enum::FLD_TYP_INT) {
		double x = atoi(str.c_str());
		dv.push_back(x);
	}
}

static int fill_data_table(uint32_t prf_idx, uint32_t evt_idx, uint32_t prf_obj_idx, prf_obj_str &prf_obj,
		evt_str &event_table, file_list_str &file_list, int verbose)
{
	std::vector <double> ts_cpu, state_prev_cpu, ts_by_var;
	std::vector <int> state_prev_cpu_initd;
	double ts_0, ts_end;
	double dura=0.0;
	int skipped_evts= 0, added_evts= 0;

	//run_heapchk("top of fill_data_table:", __FILE__, __LINE__, 1);
	ts_0   = prf_obj.tm_beg;
	ts_end = prf_obj.tm_end;
#define TMR_MAX 10
	double tmr[TMR_MAX];
	for (uint32_t i=0; i < TMR_MAX; i++) {
		tmr[i] = 0.0;
	}
	double tm_0 = dclock();
	static double tm_cumulative = 0.0;

	ts_cpu.resize(prf_obj.features_nr_cpus_online, ts_0);
	state_prev_cpu.resize(prf_obj.features_nr_cpus_online, 0);
	state_prev_cpu_initd.resize(prf_obj.features_nr_cpus_online, 0);
	if (verbose)
		printf("nr_cpus_online= %d, ts_initial= %.9f, ts_end= %f, ts_diff= %f\n",
			(int)prf_obj.features_nr_cpus_online, ts_0, ts_end, ts_end-ts_0);
	double ts_cumu = 0.0, ts_idle= 0.0;
	std::string str;
	std::string flnm = prf_obj.filename_bin + " " + prf_obj.filename_text + " ";
	if (verbose > 0)
		printf("filenames= %s at %s %d\n", flnm.c_str(), __FILE__, __LINE__);
	static int doing_SCHED_SWITCH = 0;
	if (doing_SCHED_SWITCH == 0 &&
			(event_table.event_name_w_area == "sched:sched_switch" || event_table.event_name == "CSwitch")) {
		// just want to do the 'doing sched_switch' logic once for the event...
		// might need to revisit if multiple file in same file group have sched_switch... then which sched_switch do we use?
		printf("doing sched_switch at %s %d\n", __FILE__, __LINE__);
		doing_SCHED_SWITCH = 1;
	}
	event_table.prf_ts_initial = ts_0;
	event_table.ts_last = ts_end;
	uint32_t fsz = (uint32_t)event_table.flds.size();
	static std::string flnm_prev;
	static std::vector <bool> did_prf_obj_idx;

	std::vector <uint32_t> lua_col_map;
	std::vector <uint32_t> etw_col_map;
	std::vector <std::string> etw_col_strs;
	uint32_t samples_sz = 0, cpu_idx, comm_pid_idx, tid_idx, comm_pid_idx2, tid_idx2;
	cpu_idx = comm_pid_idx = tid_idx = comm_pid_idx2 = tid_idx2 = UINT32_M1;
	int dura_idx = -1;
	uint32_t fld_w_other_by_var_idx = UINT32_M1;
	std::vector <int> lag_by_var;
	lag_by_var.resize(fsz, -1);
	struct mk_proc_str {
		uint32_t def_idx, comm_idx, tid_idx;
		mk_proc_str(): def_idx(-1), comm_idx(-1), tid_idx(-1) {}
	};
	uint64_t how_many_by_var_flags = 0;
	std::vector <mk_proc_str> mk_proc;
	for (uint32_t j=0; j < fsz; j++) {
		uint64_t flg = event_table.flds[j].flags;
		if (event_table.flds[j].diff_ts_with_ts_of_prev_by_var_using_fld.size() > 0) {
			fld_w_other_by_var_idx = j;
		}
		if (event_table.flds[j].mk_proc_from_comm_tid_flds.comm.size() > 0) {
			mk_proc_str mp;
			mp.def_idx = j;
			for (uint32_t k=0; k < fsz; k++) {
				if (event_table.flds[j].mk_proc_from_comm_tid_flds.comm == event_table.flds[k].name) {
					mp.comm_idx = k;
					break;
				}
			}
			for (uint32_t k=0; k < fsz; k++) {
				if (event_table.flds[j].mk_proc_from_comm_tid_flds.tid == event_table.flds[k].name) {
					mp.tid_idx = k;
					break;
				}
			}
			mk_proc.push_back(mp);
		}
		if (event_table.flds[j].lag_prev_by_var_using_fld.size() > 0) {
			for (uint32_t k=0; k < (int)fsz; k++) {
				if (event_table.flds[j].lag_prev_by_var_using_fld == event_table.flds[k].name) {
					lag_by_var[j] = k;
					break;
				}
			}
			if (lag_by_var[j] == -1) {
				printf("got input json file with \"lag_prev_by_var_using_fld\":\"%s\" but didn't find field in evt_flds for event %s. bye at %s %d\n",
					event_table.flds[j].lag_prev_by_var_using_fld.c_str(),
					event_table.event_name_w_area.c_str(),
					__FILE__, __LINE__);
				exit(1);
			}
			if (lag_by_var[j] == j) {
				printf("got input json file with \"lag_prev_by_var_using_fld\":\"%s\" but the field it points to is itself. error for event %s. bye at %s %d\n",
					event_table.flds[j].lag_prev_by_var_using_fld.c_str(),
					event_table.event_name_w_area.c_str(),
					__FILE__, __LINE__);
				exit(1);
			}
		}
		if (flg & (uint64_t)fte_enum::FLD_TYP_DURATION_BEF) {
			dura_idx = (int)j;
		}
		if (flg & (uint64_t)fte_enum::FLD_TYP_BY_VAR0) {
			if (how_many_by_var_flags & 1) {
				printf("got more than 1 (uint64_t)fte_enum::FLD_TYP_BY_VAR0 flags. Bye at %s %d\n", __FILE__, __LINE__);
				exit(1);
			}
			how_many_by_var_flags |= 1;
		}
		if (flg & (uint64_t)fte_enum::FLD_TYP_BY_VAR1) {
			if (how_many_by_var_flags & 2) {
				printf("got more than 1 (uint64_t)fte_enum::FLD_TYP_BY_VAR1 flags. Bye at %s %d\n", __FILE__, __LINE__);
				exit(1);
			}
			how_many_by_var_flags |= 2;
		}
	}
	if (how_many_by_var_flags == 3) {
		event_table.data.cpt_idx.resize(2);
	} else {
		event_table.data.cpt_idx.resize(1);
	}
	if (mk_proc.size() != 0) {
		for (uint32_t j=0; j < mk_proc.size(); j++) {
		if (mk_proc[j].comm_idx == UINT32_M1 || mk_proc[j].tid_idx == UINT32_M1) {
			printf("missed match mk_proc_from_comm_tid_flds comm(%s) or tid(%s) to name fields for event= %s evt_fld= %s. Bye at %s %d\n",
					event_table.flds[mk_proc[j].def_idx].mk_proc_from_comm_tid_flds.comm.c_str(),
					event_table.flds[mk_proc[j].def_idx].mk_proc_from_comm_tid_flds.tid.c_str(),
					event_table.event_name_w_area.c_str(),
					event_table.flds[mk_proc[j].def_idx].name.c_str(),
					__FILE__, __LINE__);
			exit(1);
		}
		}
	}
	if (prf_obj.file_type != FILE_TYP_ETW) {
		samples_sz = prf_obj.samples.size();
	} else {
		if (prf_idx >= prf_obj.etw_evts_set.size()) {
			printf("out of bounds prf_idx(%d) >= etw_evts_set.sz(%d). Bye at %s %d\n",
				(int)prf_idx, (int)prf_obj.etw_evts_set.size(), __FILE__, __LINE__);
			exit(1);
		}
		samples_sz = prf_obj.etw_evts_set[prf_idx].size();
		//printf("ETW event= %s samples_sz= %d, at %s %d\n",
		//	prf_obj.events[prf_idx].event_name.c_str(), samples_sz, __FILE__, __LINE__);
		for (uint32_t j=0; j < fsz; j++) {
			uint64_t flg = event_table.flds[j].flags;
			if ((flg & (uint64_t)fte_enum::FLD_TYP_SYS_CPU) && event_table.flds[j].lkup.size() > 0) {
				cpu_idx = j;
			} else if ((flg & (uint64_t)fte_enum::FLD_TYP_TID) && event_table.flds[j].lkup.size() > 0) {
				tid_idx = j;
			} else if ((flg & (uint64_t)fte_enum::FLD_TYP_TID2) && event_table.flds[j].lkup.size() > 0) {
				tid_idx2 = j;
			} else if ((flg & (uint64_t)fte_enum::FLD_TYP_ETW_COMM_PID) && event_table.flds[j].lkup.size() > 0) {
				comm_pid_idx = j;
			} else if ((flg & (uint64_t)fte_enum::FLD_TYP_ETW_COMM_PID2) && event_table.flds[j].lkup.size() > 0) {
				comm_pid_idx2 = j;
			}
		}

		// lets do the column mapping
		etw_col_map.resize(fsz, -1);
		etw_col_strs.resize(fsz, "");
		//printf("fsz= %d, prf_idx= %d cols.sz= %d at %s %d\n",
		//		fsz, prf_idx, (int)prf_obj.events[prf_idx].etw_cols.size(), __FILE__, __LINE__);
		for (uint32_t j=0; j < fsz; j++) {
			uint32_t k=UINT32_M1;
			for (k=1; k < prf_obj.events[prf_idx].etw_cols.size(); k++) {
				//printf("ck   etw_col_map[%d](%s) == %d (%s) at %s %d\n", j,
				 //       event_table.flds[j].lkup.c_str(), k, prf_obj.events[prf_idx].etw_cols[k].c_str(),
				  //      __FILE__, __LINE__);
				if (event_table.flds[j].lkup == prf_obj.events[prf_idx].etw_cols[k]) {
					//printf("mtch etw_col_map[%d] = %d at %s %d\n", j, k, __FILE__, __LINE__);
					etw_col_map[j] = k;
					break;
				}
			}
			if (k >= prf_obj.events[prf_idx].etw_cols.size() &&
				event_table.flds[j].lkup == prf_obj.events[prf_idx].etw_cols[0]) {
					etw_col_map[j] = 0;
			}
#if 0
			if (event_table.flds[j].lkup.size() > 0 && k >= prf_obj.events[evt_idx].etw_cols.size()) {
				printf("error on etw col names. Expected to find a match of json chart field lkup names and etw col names at %s %d\n", __FILE__, __LINE__);
				printf("event_name %s\n", event_table.event_name.c_str());
				printf("etw col_names generated by lua script:\n");
				for (uint32_t kk=0; kk < prf_obj.events[evt_idx].etw_cols.size(); kk++) {
					printf("col[%d] col_name= %s\n", k, prf_obj.events[evt_idx].etw_cols[kk].c_str());
				}
				printf("event evt_flds lkup fields: (from json file)\n");
				for (uint32_t kk=0; kk < fsz; kk++) {
					printf("evt_flds[%d] lkup= %s\n", k, event_table.flds[kk].lkup.c_str());
				}
				printf("bye at %s %d\n", __FILE__, __LINE__);
				exit(1);
			}
#endif
		}
	}
	bool use_value = true;
	std::vector <bool> did_actions_already;
	did_actions_already.resize(fsz);

	struct fe_str {
		int fe_idx, got_prf_idx;
		fe_str(): fe_idx(-1), got_prf_idx(0) {}
	};
	static std::vector <fe_str> fe_idx_vec;

	uint32_t file_tag_idx = prf_obj.file_tag_idx;
	//prf_obj_idx should always increase within a file_group
	if (flnm != flnm_prev || (prf_obj_idx == 0 && did_prf_obj_idx.size() > 1)) {
		// if we are doing multiple groups I'm not sure but we might get prf_obj_idx 0 for file group 0 and group 1
		did_prf_obj_idx.resize(0);
		fe_idx_vec.resize(0);
	}
	if (prf_obj_idx >= did_prf_obj_idx.size()) {
		did_prf_obj_idx.resize(prf_obj_idx+1, false);
	}
	if (flnm != flnm_prev) {
		printf("fill_data_table: flnm= %s, flnm_prev= %s, prf_obj_idx= %d at %s %d\n",
				flnm.c_str(), flnm_prev.c_str(), prf_obj_idx, __FILE__, __LINE__);
		flnm_prev = flnm;
	}
	std::string comm, comm2, fl_evt;
	if (!did_prf_obj_idx[prf_obj_idx] && prf_obj.file_type != FILE_TYP_ETW) {
		int prf_evt_idx, fe_idx;
		if (prf_obj.events.size() < fe_idx_vec.size()) {
			fe_idx_vec.resize(prf_obj.events.size());
		}
		for (uint32_t i=0; i < samples_sz; i++) {
			prf_evt_idx = (int)prf_obj.samples[i].evt_idx;
			if (prf_evt_idx == -1) {
				printf("messed up prf event indx number of event[%d] name='%s' at %s %d\n",
						i, prf_obj.samples[i].event.c_str(), __FILE__, __LINE__);
				exit(1);
			}
			if (prf_evt_idx >= fe_idx_vec.size()) {
				fe_idx_vec.resize(prf_evt_idx+1);
			}
			fe_idx_vec[prf_evt_idx].got_prf_idx++;
			
			fe_idx = prf_obj.samples[i].fe_idx;
			if (fe_idx == -1) {
				if (fe_idx_vec[prf_evt_idx].fe_idx != -1) {
					fe_idx = fe_idx_vec[prf_evt_idx].fe_idx;
					prf_obj.samples[i].fe_idx = fe_idx;
				}
				else
				{
					fl_evt = flnm + prf_obj.events[prf_evt_idx].event_name_w_area;
					fe_idx = flnm_evt_hash[file_tag_idx][fl_evt] - 1;
					if (fe_idx == -1) {
						printf("messed up event[%d]= %s fe_idx= %d at %s %d\n",
							i, prf_obj.samples[i].event.c_str(), fe_idx, __FILE__, __LINE__);
						exit(1);
					}
					prf_obj.samples[i].fe_idx = fe_idx;
					fe_idx_vec[prf_evt_idx].fe_idx = fe_idx;
				}
			}
			if (prf_obj.samples[i].callstack.size() > 0) {
				flnm_evt_vec[file_tag_idx][fe_idx].has_callstacks = true;
			}
		}
		did_prf_obj_idx[prf_obj_idx] = true;
	}

	int got_prf_rec_count = 0;
	for (uint32_t i=0; i < samples_sz; i++) {
#ifdef TMR_FDT
		double tm_00 = dclock();
#endif
		uint32_t pid, tid, pid2, tid2;
		double ts;
		int cpt_idx, cpt_idx2, prf_evt_idx, fe_idx, cpu;
		cpt_idx = cpt_idx2 = prf_evt_idx = fe_idx = cpu = -1;
		if (prf_obj.file_type != FILE_TYP_ETW) {
			if (prf_obj.samples[i].evt_idx != prf_idx) {
				if (fe_idx_vec[prf_idx].got_prf_idx == got_prf_rec_count) {
					printf("skipping prf_idx= %d after %d samples  at %s %d\n", prf_idx, got_prf_rec_count, __FILE__, __LINE__);
					// so if we've already processed all the events we are looking for then break
					break;
				}
#ifdef TMR_FDT
				double tm_00a = dclock();
				tmr[3] += tm_00a - tm_00;
#endif
				skipped_evts++;
				continue;
			}
			got_prf_rec_count++;
		}

		ts = dura = 0.0;
		pid = tid = pid2 = tid2 = UINT32_M1;
		comm  = "";
		comm2 = "";

		if (prf_obj.file_type != FILE_TYP_ETW) {
			// events in perf, trace-cmd and my lua files have common header data (comm, pid, tid, cpu, ts)
			comm = prf_obj.samples[i].comm;
			pid  = prf_obj.samples[i].pid;
			tid  = prf_obj.samples[i].tid;

			fe_idx = prf_obj.samples[i].fe_idx;
			flnm_evt_vec[file_tag_idx][fe_idx].total++;
			ts = 1.0e-9 * (double)prf_obj.samples[i].ts;
			cpu = (int)prf_obj.samples[i].cpu;
		}
		if (prf_obj.file_type == FILE_TYP_ETW) {
			ts = ts_0 + 1.0e-6 * (double)prf_obj.etw_evts_set[prf_idx][i].ts;
			uint32_t data_idx = prf_obj.etw_evts_set[prf_idx][i].data_idx;
			if (cpu_idx != UINT32_M1) {
				uint32_t kk = etw_col_map[cpu_idx];
				if (kk != UINT32_M1 && kk < prf_obj.etw_data[data_idx].size()) {
					cpu = atoi(prf_obj.etw_data[data_idx][kk].c_str());
				} else {
					printf("cpu_idx= %d, kk= %d, sz= %d data_idx= %d, ts= %.0f tkn0= %s evt= %s at %s %d\n",
							cpu_idx, kk, (int)prf_obj.etw_data[data_idx].size(),
						    data_idx, (double)prf_obj.etw_evts_set[prf_idx][i].ts,
							prf_obj.etw_data[data_idx][0].c_str(),
							prf_obj.events[prf_idx].event_name.c_str(), __FILE__, __LINE__);
					exit(1);
				}
			}
			if (comm_pid_idx != UINT32_M1) {
				std::string pid_str;
				uint32_t kk = etw_col_map[comm_pid_idx];
				if (kk != UINT32_M1) {
					if (-1 == etw_split_comm(prf_obj.etw_data[data_idx][kk], comm, pid_str, ts)) {
						fprintf(stderr, "got err at %s %d\n", __FILE__, __LINE__);
						exit(1);
					}
				}
				if (pid_str.size() > 0) {
					pid = atoi(pid_str.c_str());
				}
			}
			if (comm_pid_idx2 != UINT32_M1) {
				std::string pid_str;
				uint32_t kk = etw_col_map[comm_pid_idx2];
				if (kk != UINT32_M1) {
					if (-1 == etw_split_comm(prf_obj.etw_data[data_idx][kk], comm2, pid_str, ts)) {
						fprintf(stderr, "got err at %s %d\n", __FILE__, __LINE__);
						exit(1);
					}
				}
				if (pid_str.size() > 0) {
					pid2 = atoi(pid_str.c_str());
				}
			}
			if (tid_idx != UINT32_M1) {
				uint32_t kk = etw_col_map[tid_idx];
				if (kk != UINT32_M1) {
					tid = atoi(prf_obj.etw_data[data_idx][kk].c_str());
				}
			}
			if (tid_idx2 != UINT32_M1) {
				uint32_t kk = etw_col_map[tid_idx2];
				if (kk != UINT32_M1) {
					tid2 = atoi(prf_obj.etw_data[data_idx][kk].c_str());
				}
			}
			replace_substr(comm, "\"", "", verbose);
			if (pid == -1 && tid == -1 && comm == "Unknown") {
				// can get comm/pid/tid like '"Unknown" (  -1),         -1' in etw file. chg to unknown 1/1
				pid = 1;
				tid = 1;
			}
			if (comm2.size() > 0) {
				replace_substr(comm2, "\"", "", verbose);
			}
			if (pid2 == -1 && tid2 == -1 && comm2 == "Unknown") {
				pid2 = 1;
				tid2 = 1;
			}
			fl_evt = flnm + prf_obj.events[prf_idx].event_name_w_area;
			if (file_tag_idx == UINT32_M1) {
				printf("bad file_tag_idx= %d at %s %d\n", file_tag_idx, __FILE__, __LINE__);
				exit(1);
			}
			fe_idx = flnm_evt_hash[file_tag_idx][fl_evt] - 1;
			if (fe_idx == -1) {
				printf("messed up hash of str='%s', evt_nm= %s, prf_evt_idx= %d, evt_nm_w_area= %s at %s %d\n",
					fl_evt.c_str(),
					prf_obj.events[prf_idx].event_name.c_str(),
					prf_evt_idx,
					prf_obj.events[prf_idx].event_name_w_area.c_str(),
					__FILE__, __LINE__);
				exit(1);
			}
			if (fe_idx >= flnm_evt_vec[file_tag_idx].size()) {
				printf("fe_idx= %d >= flnm_evt_vec.size()= %d at %s %d\n", fe_idx, (int)flnm_evt_vec[file_tag_idx].size(), __FILE__, __LINE__);
				exit(1);
			}
			flnm_evt_vec[file_tag_idx][fe_idx].total++;
			if (prf_obj.etw_evts_set[prf_idx][i].cs_idx_beg != UINT32_M1) {
				flnm_evt_vec[file_tag_idx][fe_idx].has_callstacks = true;
			}
		}
		std::string strx = comm + " " + std::to_string(pid) + "/" + std::to_string(tid);
		if (pid == -1 && tid == -1 && (prf_obj.file_type == FILE_TYP_ETW) && comm_pid_idx != UINT32_M1) {
			if ( prf_obj.file_type == FILE_TYP_ETW && (prf_obj.etw_evts_set.size() <= prf_idx ||
					prf_obj.etw_evts_set[prf_idx].size() <= i)) {
				printf("ETW bad szs: set.sz= %d, prf_idx= %d, [prf_idx].sz= %d, i= %d at %s %d\n",
					(int)prf_obj.etw_evts_set.size(), (int)prf_idx,
					(int)prf_obj.etw_evts_set[prf_idx].size(), (int)i, __FILE__, __LINE__);
				exit(1);
			}

			if (prf_obj.file_type == FILE_TYP_ETW) {
				printf("bad strx= '%s' at timestamp= %.0f at %s %d\n",
					strx.c_str(), (double)prf_obj.etw_evts_set[prf_idx][i].ts, __FILE__, __LINE__);
			} else {
				printf("bad strx= '%s' at %s %d\n", strx.c_str(), __FILE__, __LINE__);
			}
			fflush(NULL);
			exit(1);
		}
		if ( prf_obj.file_type == FILE_TYP_ETW || prf_obj.file_type == FILE_TYP_PERF ||
			prf_obj.file_type == FILE_TYP_TRC_CMD) {
			if (comm.size() > 0 && pid != UINT32_M1 && tid != UINT32_M1) {
				cpt_idx = (int)hash_comm_pid_tid(comm_pid_tid_hash[file_tag_idx], comm_pid_tid_vec[file_tag_idx], comm, pid, tid) - 1;
				if (prf_obj.file_type != FILE_TYP_ETW) {
					//prf_obj.samples[i].cpt_idx = cpt_idx-1;
				}
			}
			if (comm2.size() > 0 && pid != UINT32_M1 && tid != UINT32_M1) {
				cpt_idx2 = (int)hash_comm_pid_tid(comm_pid_tid_hash[file_tag_idx], comm_pid_tid_vec[file_tag_idx], comm2, pid2, tid2) - 1;
				if (prf_obj.file_type != FILE_TYP_ETW) {
				//prf_obj.samples[i].cpt_idx2 = cpt_idx2-1;
				}
			}
		}
		std::vector <double> dv;
		dura = 0.0;
		if (prf_obj.file_type == FILE_TYP_LUA) {
			uint32_t lua_evt;
			lua_evt = prf_obj.samples[i].evt_idx;
			if (lua_evt != evt_idx) {
				printf("not sure what is going on here. lua_evt= %d, evt_idx= %d at %s %d\n", lua_evt, evt_idx, __FILE__, __LINE__);
				printf("prf_obj.events[%d].event_name= %s, evt_nm_w_area= %s at %s %d\n",
					evt_idx, prf_obj.events[evt_idx].event_name.c_str(),
					prf_obj.events[evt_idx].event_name_w_area.c_str(),
					__FILE__, __LINE__);
				exit(1);
			}
			lua_col_map.resize(fsz, -1);

			for (uint32_t j=0; j < fsz; j++) {
				uint32_t k;
				for (k=0; k < prf_obj.lua_data.col_names[evt_idx].size(); k++) {
					if (event_table.flds[j].lkup == prf_obj.lua_data.col_names[evt_idx][k]) {
						lua_col_map[j] = k;
						break;
					}
				}
				if (k >= prf_obj.lua_data.col_names[evt_idx].size()) {
					printf("error on lua col names. Expected to find a match of json chart field lkup names and lua col names at %s %d\n", __FILE__, __LINE__);
					printf("event_name %s\n", event_table.event_name.c_str());
					printf("lua col_names generated by lua script:\n");
					for (uint32_t kk=0; kk < prf_obj.lua_data.col_names[evt_idx].size(); kk++) {
						printf("col[%d] col_name= %s\n", k, prf_obj.lua_data.col_names[evt_idx][kk].c_str());
					}
					printf("event evt_flds lkup fields: (from json file)\n");
					for (uint32_t kk=0; kk < fsz; kk++) {
						printf("evt_flds[%d] lkup= %s\n", k, event_table.flds[kk].lkup.c_str());
					}
					printf("bye at %s %d\n", __FILE__, __LINE__);
					exit(1);
				}
			}
		}
		std::vector <int> div_by_interval_idx;
		bool use_u64 = false;
		bool try_skipping_record = false;
		bool tmp_verbose = false;
#ifdef TMR_FDT
		double tm_00a = dclock();
		tmr[0] += tm_00a - tm_00;
#endif
		for (uint32_t j=0; j < fsz; j++) {
			did_actions_already[j] = false;
			uint64_t flg = event_table.flds[j].flags;
			double ts_delta = 0.0;
			if (prf_obj.has_tm_run && (flg & (uint64_t)fte_enum::FLD_TYP_TM_RUN)) {
				ts_delta = 1.0e-9 * (double)prf_obj.samples[i].tm_run;
				//printf("ts_delta[%d]= %f tm_run= %" PRIu64 " ts= %" PRIu64 " at %s %d\n",
				//		i, ts_delta, prf_obj.samples[i].tm_run, prf_obj.samples[i].ts, __FILE__, __LINE__);
			} else if (flg & (uint64_t)fte_enum::FLD_TYP_TM_CHG_BY_CPU) {
				ts_delta = ts - ts_cpu[cpu];
			}
			if ((prf_obj.has_tm_run && (flg & (uint64_t)((uint64_t)fte_enum::FLD_TYP_TM_RUN)))
					   || (flg & (uint64_t)fte_enum::FLD_TYP_TM_CHG_BY_CPU)) {
				if (flg & (uint64_t)fte_enum::FLD_TYP_DURATION_BEF) {
					dura = ts_delta;
					if ((ts - dura) < 0.0) {
						printf("neg beg: ts= %.9f, dura= %.9f, ts_delta= %.9f, ts_cpu[%d]= %.9f at %s %d\n",
							ts, dura, ts_delta, cpu, ts_cpu[cpu], __FILE__, __LINE__);
						exit(1);
					}
				}
				//printf("ts= %f, ts_delta= %f, cpu= %d comm= %s, pid= %d tid= %d at %s %d\n",
				//		ts-ts_0, dura, (int)cpu, comm.c_str(), pid, tid, __FILE__, __LINE__);
				//printf("ts_delta.a[%d][%d]= %f\n", i, j, ts_delta);
				dv.push_back(ts_delta);
				ts_cumu += ts_delta;
				if (pid == 0) {
					ts_idle += ts_delta;
				}
			}
			uint32_t mpj = UINT32_M1;
			for (uint32_t mpjj=0; mpjj < mk_proc.size(); mpjj++) {
				if (mk_proc[mpjj].def_idx == j) {
					mpj = mpjj;
					break;
				}
			}
			if (mpj != UINT32_M1) {
				uint32_t cmm_i, tid_i;
				cmm_i = dv[mk_proc[mpj].comm_idx];
				tid_i = dv[mk_proc[mpj].tid_idx];
				std::string cmm;
				int pid_i = (int)prf_obj.tid_2_comm_indxp1[(uint32_t)tid_i]-1;
				if (cmm_i > 0 && tid_i >= 0 && pid_i >= 0) {
					cmm = prf_obj.comm[pid_i].comm;
#if 0
					printf("mk_proc[%d] comm= %s, tid= %d, lkup comm= %s, pid= %d tid= %d fld= %d ts= %f at %s %d\n",
							mpj, cmm.c_str(), tid_i, prf_obj.comm[pid_i].comm.c_str(),
							prf_obj.comm[pid_i].pid, prf_obj.comm[pid_i].tid, j, ts, __FILE__, __LINE__);
#endif
					std::string str = cmm + " " + std::to_string(prf_obj.comm[pid_i].pid) + "/" + std::to_string(prf_obj.comm[pid_i].tid);
					uint32_t idx = hash_string(event_table.hsh_str, event_table.vec_str, str);
					uint32_t tcpt_idx = (int)hash_comm_pid_tid(comm_pid_tid_hash[file_tag_idx], comm_pid_tid_vec[file_tag_idx],
										cmm, prf_obj.comm[pid_i].pid, prf_obj.comm[pid_i].tid) - 1;
					if (flg & (uint64_t)fte_enum::FLD_TYP_BY_VAR0) {
						cpt_idx = tcpt_idx;
					}
					if (flg & (uint64_t)fte_enum::FLD_TYP_BY_VAR1) {
						cpt_idx2 = tcpt_idx;
					}
					dv.push_back(idx);
					continue;
				}
			}
#if 0
	printf("ETW event= %s i= %d fsz= %d j= %d lkup= %s at %s %d\n",
			prf_obj.events[prf_idx].event_name.c_str(), i, fsz, j,
			event_table.flds[j].lkup.c_str(), __FILE__, __LINE__);
	fflush(NULL);
#endif
			if (prf_obj.file_type != FILE_TYP_ETW &&
				(flg & (uint64_t)fte_enum::FLD_TYP_NEW_VAL)) {
				int prf_evt_idx2 = (int)prf_obj.samples[i].evt_idx;
				std::string nd_lkup = event_table.flds[j].lkup;
				int got_it = -1;
				for (uint32_t kk=0; kk < prf_obj.events[prf_evt_idx2].new_cols.size(); kk++) {
					if (nd_lkup == prf_obj.events[prf_evt_idx2].new_cols[kk]) {
						//printf("got match on derived evt col %s at %s %d\n", nd_lkup.c_str(), __FILE__, __LINE__);
						got_it = kk;
						break;
					}
				}
				if (got_it == -1) {
					printf("missed lkup '%s' for TYP_NEW_VAL for event= %s.\nNeed lkup string to appear in list of derived events fields. Got fields:\n",
							nd_lkup.c_str(), prf_obj.events[prf_evt_idx2].event_name.c_str());
					for (uint32_t kk=0; kk < prf_obj.events[prf_evt_idx2].new_cols.size(); kk++) {
						printf("new_cols[%d]= %s\n", kk, prf_obj.events[prf_evt_idx2].new_cols[kk].c_str());
					}
					printf("bye at %s %d\n", __FILE__, __LINE__);
					exit(1);
				}
				if ((flg & (uint64_t)fte_enum::FLD_TYP_DBL) ||(flg & (uint64_t)fte_enum::FLD_TYP_INT)) {
					double val;
					if (prf_obj.samples[i].new_dvals.size() > got_it) {
						val = prf_obj.samples[i].new_dvals[got_it];
					} else {
						val = atof(prf_obj.samples[i].new_vals[got_it].c_str());
					}
					if (flg & (uint64_t)fte_enum::FLD_TYP_DURATION_BEF) {
						//printf("NEW_VAL duration val= %f, ts= %f at %s %d\n", val, ts, __FILE__, __LINE__);
						dura_idx = j;
						dura = val;
					}
					dv.push_back(val);
				}
				if (flg & (uint64_t)fte_enum::FLD_TYP_STR) {
					std::string str = prf_obj.samples[i].new_vals[got_it];
					//printf("NEW_VAL str= %s at %s %d\n", str.c_str(), __FILE__, __LINE__);
					uint32_t idx = hash_string(event_table.hsh_str, event_table.vec_str, str);
					dv.push_back(idx);
					continue;
				}
				continue;
			}
			if (flg & (uint64_t)fte_enum::FLD_TYP_DIV_BY_INTERVAL) {
				div_by_interval_idx.push_back(j);
			}
			if (fld_w_other_by_var_idx == j) {
				// this value will get filled in build_chart_data()
				dv.push_back(0.0);
				continue;
			}
			if (lag_by_var[j] != -1) {
				// this value will get filled in build_chart_data()
				dv.push_back(0.0);
				continue;
			}
			if (prf_obj.file_type == FILE_TYP_ETW) {
				uint32_t k = etw_col_map[j];
				uint32_t data_idx = prf_obj.etw_evts_set[prf_idx][i].data_idx;

				if ((flg & (uint64_t)fte_enum::FLD_TYP_TM_CHG_BY_CPU) && (flg & (uint64_t)fte_enum::FLD_TYP_DURATION_BEF)) {
					continue; // already pushed it
				}
				if (flg & (uint64_t)fte_enum::FLD_TYP_TIMESTAMP || event_table.flds[j].lkup == "_TIMESTAMP_" ||
				    event_table.flds[j].lkup == "TimeStamp") {
					dv.push_back(ts);
					continue;
				}
				str = "";
				use_u64 = false;
				uint64_t hex_u64_val=0;
				if (k != UINT32_M1) {
					str = prf_obj.etw_data[data_idx][k];
					if (flg & (uint64_t)fte_enum::FLD_TYP_HEX_IN) {
						hex_u64_val = strtoll(str.c_str(), NULL, 16);
#if 0
						printf("hex str= '%s', val= %" PRId64 ", hx= %" PRIx64 " at %s %d\n",
								str.c_str(), hex_u64_val, hex_u64_val, __FILE__, __LINE__);
#endif
						use_u64 = true;
					}
				}
#if 0
				if (flg & (uint64_t)fte_enum::FLD_TYP_DURATION_BEF || event_table.flds[j].lkup == "_DURATION_") {
					//dura = prf_obj.lua_data.data_rows[i][k].dval;
					dv.push_back(dura);
					continue;
				}
#endif
				if ((flg & (uint64_t)fte_enum::FLD_TYP_STR) && k != UINT32_M1) {
					replace_substr(str, "\"", "", verbose);
					uint32_t idx = hash_string(event_table.hsh_str, event_table.vec_str, str);
					dv.push_back(idx);
					continue;
				}
				if ((flg & (uint64_t)fte_enum::FLD_TYP_DBL) && k != UINT32_M1) {
					double x;
					if (use_u64) {
						x = hex_u64_val;
					} else {
				   		x = atof(str.c_str());
					}
					if (!(flg & (uint64_t)fte_enum::FLD_TYP_TM_CHG_BY_CPU) && (flg & (uint64_t)fte_enum::FLD_TYP_DURATION_BEF)) {
						if (event_table.flds[j].actions.size() > 0) {
							did_actions_already[j] = true;
							run_actions(x, event_table.flds[j].actions, use_value, event_table);
						}
						if (dura_idx > -1) {
							dura = x;
							if (dura_idx < dv.size()) {
								dv[dura_idx] = x;
							}
						}
					}
					dv.push_back(x);
					continue;
				}
				if ((flg & (uint64_t)fte_enum::FLD_TYP_DBL) && k == UINT32_M1 && !(flg & (uint64_t)fte_enum::FLD_TYP_TM_CHG_BY_CPU) && (flg & (uint64_t)fte_enum::FLD_TYP_DURATION_BEF)) {
					double x = 0.0;
					if (event_table.flds[j].actions.size() > 0) {
						did_actions_already[j] = true;
						run_actions(x, event_table.flds[j].actions, use_value, event_table);
					}
					dura = x;
					if (j < dv.size()) {
						dv[j] = x;
					}
					dv.push_back(x);
					continue;
				}
				if ((flg & (uint64_t)fte_enum::FLD_TYP_INT) && k != UINT32_M1) {
					double x;
					if (use_u64) {
						x = hex_u64_val;
					} else {
				   		x = (double)strtoll(str.c_str(), NULL, 0);
					}
					dv.push_back(x);
					if (!(flg & (uint64_t)fte_enum::FLD_TYP_TM_CHG_BY_CPU) && (flg & (uint64_t)fte_enum::FLD_TYP_DURATION_BEF)) {
						dura = x;
					}
					continue;
				}
				if ((flg & (uint64_t)fte_enum::FLD_TYP_SYS_CPU)) {
					if (cpu_idx == UINT32_M1) {
						printf("event looking for cpu field but it isn't defined for event %s. bye at %s %d\n",
								prf_obj.events[prf_idx].event_name.c_str(), __FILE__, __LINE__);
						exit(1);
					}
					double x = 0;
					if (k != UINT32_M1) {
					    x = atoi(str.c_str());
					}
					dv.push_back(x);
					continue;
				} else if (flg & (uint64_t)fte_enum::FLD_TYP_PERIOD) {
						printf("event looking for TYP_PERIOD field but it isn't defined for event %s. bye at %s %d\n",
								prf_obj.events[prf_idx].event_name.c_str(), __FILE__, __LINE__);
						exit(1);
					continue;
				} else if (flg & (uint64_t)fte_enum::FLD_TYP_PID) {
					dv.push_back(pid);
					continue;
				} else if (flg & (uint64_t)fte_enum::FLD_TYP_TID) {
					dv.push_back(tid);
					continue;
				} else if (flg & (uint64_t)fte_enum::FLD_TYP_TID2) {
					dv.push_back(tid2);
					continue;
				} else if (flg & (uint64_t)fte_enum::FLD_TYP_TIMESTAMP) {
					dv.push_back(ts);
					continue;
				} else if (flg & (uint64_t)fte_enum::FLD_TYP_COMM) {
					uint32_t idx = hash_string(event_table.hsh_str, event_table.vec_str, comm);
					dv.push_back(idx);
					continue;
				} else if (flg & (uint64_t)fte_enum::FLD_TYP_COMM_PID) {
					str = comm + " " + std::to_string(pid);
					uint32_t idx = hash_string(event_table.hsh_str, event_table.vec_str, str);
					dv.push_back(idx);
					continue;
				} else if (flg & (uint64_t)fte_enum::FLD_TYP_COMM_PID_TID) {
					str = comm + " " + std::to_string(pid) + "/" + std::to_string(tid);
					uint32_t idx = hash_string(event_table.hsh_str, event_table.vec_str, str);
					dv.push_back(idx);
					continue;
				} else if (flg & (uint64_t)fte_enum::FLD_TYP_COMM_PID_TID2) {
					str = comm2 + " " + std::to_string(pid2) + "/" + std::to_string(tid2);
					uint32_t idx = hash_string(event_table.hsh_str, event_table.vec_str, str);
					dv.push_back(idx);
					continue;
				}
				continue;
			}
			if (prf_obj.file_type != FILE_TYP_ETW && prf_obj.file_type != FILE_TYP_LUA &&
				(flg & (uint64_t)fte_enum::FLD_TYP_ADD_2_EXTRA) && 
				(flg & (uint64_t)fte_enum::FLD_TYP_STR)) {
				if (prf_obj.samples[i].extra_str.size() == 0) {
					int prf_evt_idx2 = (int)prf_obj.samples[i].evt_idx;
					printf("extra_str.size()== 0 for FLD_TYP_ADD_2_EXTRA for event= %s evt_fld.name= %s tm_str= %s i= %d in filename= %s at %s %d\n",
						prf_obj.events[prf_evt_idx2].event_name.c_str(), event_table.flds[j].name.c_str(),
						prf_obj.samples[i].tm_str.c_str(), i, flnm.c_str(), __FILE__, __LINE__);
					exit(1);
				}
				printf("add marker extra_str= %s at %s %d\n", prf_obj.samples[i].extra_str.c_str(), __FILE__, __LINE__);
				uint32_t idx = hash_string(event_table.hsh_str, event_table.vec_str, prf_obj.samples[i].extra_str);
				dv.push_back(idx);
				continue;
			}
			if (flg & (uint64_t)fte_enum::FLD_TYP_TRC_FLD_PFX) {
				static int pfx_errs = 0;
				int prf_evt_idx2 = (int)prf_obj.samples[i].evt_idx;
				if (prf_obj.events[prf_evt_idx2].lst_ft_fmt_idx < 0) {
					printf("lst_ft_fmt_idx should be >= 0. Bye at %s %d\n", __FILE__, __LINE__);
					exit(1);
				}
				if (prf_obj.samples[i].extra_str.size() == 0) {
					printf("extra_str.size()== 0 for TYP_TRC_FLD_PFX for event= %s evt_fld.name= %s lkup= %s, tm_str= %s i= %d in filename= %s at %s %d\n",
						prf_obj.events[prf_evt_idx2].event_name.c_str(), event_table.flds[j].name.c_str(),
						event_table.flds[j].lkup.c_str(),
						prf_obj.samples[i].tm_str.c_str(), i, flnm.c_str(), __FILE__, __LINE__);
					if (pfx_errs++ > 100) {
						printf("Got above err %d times. bye at %s %d\n", pfx_errs, __FILE__, __LINE__);
						exit(1);
					} else {
						try_skipping_record = true;
						break;
					}
				}
				int lst_ft_fmt_idx = prf_obj.events[prf_evt_idx2].lst_ft_fmt_idx;
				std::string extracted;
				if (event_table.flds[j].lkup_dlm_str.size() > 0) {
					extracted = get_str_between_dlms(prf_obj.samples[i].extra_str,
						event_table.flds[j].lkup, event_table.flds[j].lkup_dlm_str, event_table, __LINE__);
					add_basic_typ(flg, extracted, event_table, dv);
					//printf("got fld_pfx str= '%s' at %s %d\n", extracted.c_str(), __FILE__, __LINE__);
					continue;
				}
				if (event_table.flds[j].lst_ft_fmt_fld_idx == -2) {
					for(uint32_t f=0; f < file_list.lst_ft_fmt_vec[lst_ft_fmt_idx].per_fld.size(); f++) {
						if (file_list.lst_ft_fmt_vec[lst_ft_fmt_idx].per_fld[f].prefix == event_table.flds[j].lkup) {
							printf("got match on trc_fld_pfx lkup field= %s extra_str.sz= %d at %s %d\n",
								event_table.flds[j].lkup.c_str(),
								(int)prf_obj.samples[i].extra_str.size(),
								__FILE__, __LINE__);
							event_table.flds[j].lst_ft_fmt_fld_idx = (int)f;
							if ((f+1) >= file_list.lst_ft_fmt_vec[lst_ft_fmt_idx].per_fld.size()) {
								event_table.flds[j].lkup_dlm_str = "__EOL__";
							} else {
								event_table.flds[j].lkup_dlm_str = file_list.lst_ft_fmt_vec[lst_ft_fmt_idx].per_fld[f+1].prefix;
							}
						}
					}
					if (event_table.flds[j].lst_ft_fmt_fld_idx == -2) {
						printf("missed match on trc_fld_pfx  field= %s at %s %d\n", event_table.flds[j].lkup.c_str(), __FILE__, __LINE__);
						printf("bye at %s %d\n", __FILE__, __LINE__);
						exit(1);
					}
				}
				extracted = get_str_between_dlms(prf_obj.samples[i].extra_str,
					event_table.flds[j].lkup, event_table.flds[j].lkup_dlm_str, event_table, __LINE__);
				add_basic_typ(flg, extracted, event_table, dv);
				printf("got fld_pfx str= '%s' at %s %d\n", extracted.c_str(), __FILE__, __LINE__);
				continue;
			}
			if (flg & (uint64_t)fte_enum::FLD_TYP_TRC_BIN) {
				int prf_evt_idx2 = (int)prf_obj.samples[i].evt_idx;
				if (prf_obj.events[prf_evt_idx2].lst_ft_fmt_idx < 0) {
					printf("lst_ft_fmt_idx should be >= 0. got= %d. Bye at %s %d\n",
						prf_obj.events[prf_evt_idx2].lst_ft_fmt_idx, __FILE__, __LINE__);
					int prf_evt_idx2 = (int)prf_obj.samples[i].evt_idx;
					printf("event= %s evt_fld.name= %s lkup= %s, tm_str= %s i= %d in filename= %s at %s %d\n",
						prf_obj.events[prf_evt_idx2].event_name.c_str(), event_table.flds[j].name.c_str(),
						event_table.flds[j].lkup.c_str(),
						prf_obj.samples[i].tm_str.c_str(), i, flnm.c_str(), __FILE__, __LINE__);
					exit(1);
				}
				int lst_ft_fmt_idx = prf_obj.events[prf_evt_idx2].lst_ft_fmt_idx;
				if (event_table.flds[j].lst_ft_fmt_fld_idx == -2) {
					if (lst_ft_fmt_idx < 0 || lst_ft_fmt_idx >= file_list.lst_ft_fmt_vec.size()) {
						printf("lst_ft_fmt_idx= %d, file_list.lst_ft_fmt_vec.size()= %d, file_list.idx= %d at %s %d\n",
							lst_ft_fmt_idx, (int)file_list.lst_ft_fmt_vec.size(), file_list.idx, __FILE__, __LINE__);
						fflush(NULL);
						exit(1);
					}
					for(uint32_t f=0; f < file_list.lst_ft_fmt_vec[lst_ft_fmt_idx].fields.size(); f++) {
						if (file_list.lst_ft_fmt_vec[lst_ft_fmt_idx].fields[f].name == event_table.flds[j].lkup) {
							if (verbose)
								printf("got match on trc_bin_lkup field= %s at %s %d\n",
										event_table.flds[j].lkup.c_str(), __FILE__, __LINE__);
							event_table.flds[j].lst_ft_fmt_fld_idx = (int)f;
						}
					}
					if (event_table.flds[j].lst_ft_fmt_fld_idx == -2) {
						printf("missed match on trc_bin_lkup field= %s at %s %d\n", event_table.flds[j].lkup.c_str(), __FILE__, __LINE__);
						printf("bye at %s %d\n", __FILE__, __LINE__);
						exit(1);
					}
				}
				int ft_idx = event_table.flds[j].lst_ft_fmt_fld_idx;
				int mm_idx = prf_obj.mm_idx;
				const unsigned char *mm_buf = prf_obj.mm_buf;
				if (!mm_buf) {
					printf("mm_buf is null at %s %d\n", __FILE__, __LINE__);
					exit(1);
				}
				long mm_off = prf_obj.samples[i].mm_off + (long)file_list.lst_ft_fmt_vec[lst_ft_fmt_idx].fields[ft_idx].offset;
				if (mm_off < 0) {
					printf("mm_off is %ld, must be >= 0. Bye at %s %d\n", mm_off, __FILE__, __LINE__);
					exit(1);
				}
				double v=0;
				if (file_list.lst_ft_fmt_vec[lst_ft_fmt_idx].fields[ft_idx].size == 4) {
#if 0
					printf("bef trc_bin offset= %d, name= %s, evt_nm= %s, lkup_value= %f mm_off= %ld po_off= %ld at %s %d\n",
						file_list.lst_ft_fmt_vec[lst_ft_fmt_idx].fields[ft_idx].offset,
						file_list.lst_ft_fmt_vec[lst_ft_fmt_idx].fields[ft_idx].name.c_str(),
						prf_obj.samples[i].event.c_str(),
						v, mm_off, prf_obj.samples[i].mm_off, __FILE__, __LINE__);
					fflush(NULL);
#endif
					if (file_list.lst_ft_fmt_vec[lst_ft_fmt_idx].fields[ft_idx].sgned == 1) {
						v = (double)*(int32_t *)(mm_buf+mm_off);
					} else {
						v = (double)*(uint32_t *)(mm_buf+mm_off);
					}
#if 0
					if ( prf_obj.samples[i].event.find("cpu_frequency") != std::string::npos) {
					printf("trc_bin offset= %d, name= %s, evt_nm= %s, lkup_value= %f at %s %d\n",
						file_list.lst_ft_fmt_vec[lst_ft_fmt_idx].fields[ft_idx].offset,
						file_list.lst_ft_fmt_vec[lst_ft_fmt_idx].fields[ft_idx].name.c_str(),
						prf_obj.samples[i].event.c_str(),
						v, __FILE__, __LINE__);
					}
#endif
				} else if (file_list.lst_ft_fmt_vec[lst_ft_fmt_idx].fields[ft_idx].size == 8) {
					if (file_list.lst_ft_fmt_vec[lst_ft_fmt_idx].fields[ft_idx].sgned == 1) {
						v = (double)*(int64_t *)(mm_buf+mm_off);
					} else {
						v = (double)*(uint64_t *)(mm_buf+mm_off);
					}
				} else {
					if (file_list.lst_ft_fmt_vec[lst_ft_fmt_idx].fields[ft_idx].typ == "char") {
						if (!(flg & (uint64_t)fte_enum::FLD_TYP_STR)) {
							printf("prf bin fld typ is char but evt_fld def isn't TYP_STR for name= %s, typ= %s at %s %d\n",
							file_list.lst_ft_fmt_vec[lst_ft_fmt_idx].fields[ft_idx].name.c_str(),
							file_list.lst_ft_fmt_vec[lst_ft_fmt_idx].fields[ft_idx].typ.c_str(),
							__FILE__, __LINE__);
							exit(1);
						}
						std::string str;
						for(uint32_t kk=0; kk < file_list.lst_ft_fmt_vec[lst_ft_fmt_idx].fields[ft_idx].size; kk++) {
							if (*(char *)(mm_buf+mm_off+kk) == 0) {
								break;
							}
							str.push_back(*(char *)(mm_buf+mm_off+kk));
						}
						//printf("got bin str= '%s' at %s %d\n", str.c_str(), __FILE__, __LINE__);
						uint32_t idx = hash_string(event_table.hsh_str, event_table.vec_str, str);
						dv.push_back(idx);
						continue;
					} else {
						printf("don't yet handle size= %d name= %s, typ= %s at %s %d\n",
							file_list.lst_ft_fmt_vec[lst_ft_fmt_idx].fields[ft_idx].size,
							file_list.lst_ft_fmt_vec[lst_ft_fmt_idx].fields[ft_idx].name.c_str(),
							file_list.lst_ft_fmt_vec[lst_ft_fmt_idx].fields[ft_idx].typ.c_str(),
							__FILE__, __LINE__);
						exit(1);
					}
				}
				dv.push_back(v);
				continue;
			}
			if (prf_obj.file_type == FILE_TYP_LUA) {
				//uint32_t lua_col = -1, lua_evt;
				uint32_t k = lua_col_map[j];

				if (flg & (uint64_t)fte_enum::FLD_TYP_TIMESTAMP || event_table.flds[j].lkup == "_TIMESTAMP_") {
					ts = prf_obj.lua_data.data_rows[i][k].dval; // this is the sec.ns ts already
					dv.push_back(ts);
					continue;
				}
				if (flg & (uint64_t)fte_enum::FLD_TYP_DURATION_BEF || event_table.flds[j].lkup == "_DURATION_") {
					dura = prf_obj.lua_data.data_rows[i][k].dval;
					dv.push_back(dura);
					continue;
				}
				if (flg & (uint64_t)fte_enum::FLD_TYP_STR) {
					str = prf_obj.lua_data.data_rows[i][k].str;
					uint32_t idx = hash_string(event_table.hsh_str, event_table.vec_str, str);
					dv.push_back(idx);
					continue;
				}
				if (flg & (uint64_t)fte_enum::FLD_TYP_DBL) {
					double x = prf_obj.lua_data.data_rows[i][k].dval;
					dv.push_back(x);
					continue;
				}
				if (flg & (uint64_t)fte_enum::FLD_TYP_INT) {
					double x = prf_obj.lua_data.data_rows[i][k].ival;
					dv.push_back(x);
					continue;
				}
				continue;
			}
			if (flg & (uint64_t)fte_enum::FLD_TYP_SYS_CPU) {
				dv.push_back(prf_obj.samples[i].cpu);
				continue;
			} else if (flg & (uint64_t)fte_enum::FLD_TYP_PERIOD) {
				dv.push_back(prf_obj.samples[i].period);
				continue;
			} else if (flg & (uint64_t)fte_enum::FLD_TYP_PID) {
				dv.push_back(prf_obj.samples[i].pid);
				continue;
			} else if (flg & (uint64_t)fte_enum::FLD_TYP_TID) {
				dv.push_back(prf_obj.samples[i].tid);
				continue;
			} else if (flg & (uint64_t)fte_enum::FLD_TYP_TIMESTAMP) {
				double ts2 = 1.0e-9 * (double)prf_obj.samples[i].ts;
				dv.push_back(ts2);
				continue;
			} else if (flg & (uint64_t)fte_enum::FLD_TYP_COMM) {
				str = prf_obj.samples[i].comm;
				uint32_t idx = hash_string(event_table.hsh_str, event_table.vec_str, str);
				dv.push_back(idx);
				continue;
			} else if (flg & (uint64_t)fte_enum::FLD_TYP_COMM_PID) {
				str = prf_obj.samples[i].comm + " " + std::to_string(prf_obj.samples[i].pid);
				uint32_t idx = hash_string(event_table.hsh_str, event_table.vec_str, str);
				dv.push_back(idx);
				continue;
			} else if (flg & (uint64_t)fte_enum::FLD_TYP_COMM_PID_TID) {
				str = prf_obj.samples[i].comm + " " + std::to_string(prf_obj.samples[i].pid) + "/" + std::to_string(prf_obj.samples[i].tid);
				uint32_t idx = hash_string(event_table.hsh_str, event_table.vec_str, str);
				dv.push_back(idx);
				continue;
			} else if (flg & (uint64_t)fte_enum::FLD_TYP_STR) {
				str = "unknwn2"; // just a dummy string. value will probably be filled in later.
				uint32_t idx = hash_string(event_table.hsh_str, event_table.vec_str, str);
				dv.push_back(idx);
				continue;
			}
			if ((flg & (uint64_t)fte_enum::FLD_TYP_DBL)) {
				if ((flg & (uint64_t)fte_enum::FLD_TYP_TM_CHG_BY_CPU) && (flg & (uint64_t)fte_enum::FLD_TYP_DURATION_BEF)) {
					continue; // already pushed it
				}
				double x=0.0;
				if (!(flg & (uint64_t)fte_enum::FLD_TYP_TM_CHG_BY_CPU) &&
					(flg & (uint64_t)fte_enum::FLD_TYP_DURATION_BEF)) {
					if (event_table.flds[j].actions.size() > 0) {
						did_actions_already[j] = true;
						run_actions(x, event_table.flds[j].actions, use_value, event_table);
					}
					if (dura_idx > -1) {
						dura = x;
						if (dura_idx < dv.size()) {
							dv[dura_idx] = x;
						}
					}
					dv.push_back(x);
					continue;
				}
			}
		}
#ifdef TMR_FDT
		double tm_00b = dclock();
		tmr[1] += tm_00b - tm_00a;
#endif
		if (try_skipping_record) {
			try_skipping_record = false;
			continue;
		}
		if (dv.size() != fsz) {
			printf("mismatch dv.size()= %d, fsz= %d event_name= %s at %s %d\n",
					(int)dv.size(), (int)fsz, event_table.event_name_w_area.c_str(),
					__FILE__, __LINE__);
			exit(1);
		}
		if (div_by_interval_idx.size() > 0) {
			if (dura_idx == -1) {
				printf("mess up here. If you use 'TYP_DIV_BY_INTERVAL' in (chart json file) then you must must have a field with typ_lkup= 'TYP_TM_CHG_BY_CPU|TYP_DURATION_BEF|TYP_DBL' at %s %d\n", __FILE__, __LINE__);
				exit(1);
			}
			for (uint32_t k=0; k < div_by_interval_idx.size(); k++) {
				if (dv[dura_idx] > 0.0) {
					if (tmp_verbose) {
						printf("bef got dv[%d]= %f dv[dura_idx(%d)]= %f at %s %d\n", 
							div_by_interval_idx[k], dv[div_by_interval_idx[k]],
						  	dura_idx, dv[dura_idx], __FILE__, __LINE__);
					}
					dv[div_by_interval_idx[k]] /= dv[dura_idx];
					if (tmp_verbose) {
						printf("aft got dv[%d]= %f dv[dura_idx(%d)]= %f at %s %d\n", 
							div_by_interval_idx[k], dv[div_by_interval_idx[k]],
						  	dura_idx, dv[dura_idx], __FILE__, __LINE__);
					}
				}
			}

		}
#if 0
		if (prf_obj.file_type == FILE_TYP_ETW) {
			for (uint32_t ii=0; ii < dv.size(); ii++) {
				printf("i= %d, ii= %d, %s= %f\n", i, ii, event_table.flds[ii].name.c_str(),  dv[ii]);
			}
			fflush(NULL);
		}
#endif
		use_value = true;
		for (uint32_t j=0; j < fsz; j++) {
			if (did_actions_already[j]) {
				continue;
			}
			run_actions(dv[j], event_table.flds[j].actions, use_value, event_table);
			uint64_t flg = event_table.flds[j].flags;
			if (flg & (uint64_t)fte_enum::FLD_TYP_STATE_AFTER) {
				if (state_prev_cpu_initd[cpu] == 0) {
					use_value = false;
				}
				double nxt_val = dv[j];
				dv[j] =  state_prev_cpu[cpu];
				state_prev_cpu[cpu] = nxt_val;
				state_prev_cpu_initd[cpu] = 1;
			}
		}
		if (doing_SCHED_SWITCH == 1) {
			int ti = -1;
			ti = cpt_idx;
			if (ti != -1 && ti >= comm_pid_tid_vec[file_tag_idx].size()) {
				printf("ckck ti: got ti= %d, comm_pid_tid_vec.sz= %d at %s %d\n", ti, (int)comm_pid_tid_vec[file_tag_idx].size(), __FILE__, __LINE__);
			}
			if (ti != -1 && ti < comm_pid_tid_vec[file_tag_idx].size()) {
			    comm_pid_tid_vec[file_tag_idx][ti].total += dura;
			}
		}

		if (use_value) {
			struct ts_str tss;
			tss.ts = ts;
			tss.duration = dura;
			//printf("bot of fill_data_table: got duration= %f, ts= %f at %s %d\n", dura, ts, __FILE__, __LINE__);
			event_table.data.ts.push_back(tss);
			event_table.data.vals.push_back(dv);
			added_evts++;
			event_table.data.cpt_idx[0].push_back(cpt_idx);
			if (event_table.data.cpt_idx.size() == 2) {
				event_table.data.cpt_idx[1].push_back(cpt_idx2);
			}
			event_table.data.prf_sample_idx.push_back(i);
		}
		//if (cpu >= 0) 
		if (cpu >= 0 && ts != ts_cpu[cpu]) {
			ts_cpu[cpu] = ts;
		}
#ifdef TMR_FDT
		double tm_00c = dclock();
		tmr[2] += tm_00c - tm_00b;
#endif
	}
	double tm_1 = dclock();
	tm_cumulative += tm_1 - tm_0;
	//run_heapchk("bot of fill_data_table:", __FILE__, __LINE__, 1);
	//if (verbose)
	{
		//printf("ts_cumu %f, ts_idle= %f\n", ts_cumu, ts_idle);
		printf("fill_data_table: tmr[e]= %.3f tmr[0]= %.2f tmr[1]= %.2f tmr[2]= %.2f sum tmr= %.3f, tot= %.2f tm_cumu= %.2f event_table[%d].data.vals.size()= %d, added_evts= %d, skipped_evts= %d, prf_idx= %d, prf_obj_idx= %d, event= %s\n",
				tmr[3], tmr[0], tmr[1], tmr[2], tmr[0]+tmr[1]+tmr[2]+tmr[3], tm_1-tm_0, tm_cumulative,
			evt_idx, (int)event_table.data.vals.size(), added_evts, skipped_evts, prf_idx, prf_obj_idx,
			event_table.event_name_w_area.c_str());
	}
	return added_evts;
}


static void web_srvr_start(Queue<std::string>& q_from_srvr, Queue<std::string>& q_bin_from_srvr, Queue <std::string>& q_from_clnt,  int *id, int verbose) {
	std::ostringstream tmp;
	tmp << "started cw thread  " << " --> C" << *id;
	std::cout << tmp.str() << std::endl;
	std::vector <std::string> cw_argv;
	cw_argv.push_back("pat");
	civetweb_main(std::ref(q_from_srvr), std::ref(q_bin_from_srvr), std::ref(q_from_clnt), id, cw_argv, options.web_port, verbose);
	*id = THRD_DONE;
}

static void web_push_from_srvr_to_clnt(Queue<std::string>& q, unsigned int id) {
	for (int i = 1; i <= 4; ++i) {
		std::ostringstream tmp;
		tmp << "from server item --> " << i << " id= " << id;
		std::cout << tmp.str() << std::endl;
		q.push(std::to_string(i));
	}
}

static int start_web_server_threads(Queue<std::string>& q_from_srvr_to_clnt,
	Queue <std::string>& q_bin_from_srvr_to_clnt,
	Queue <std::string>& q_from_clnt_to_srvr,
	std::vector<std::thread> &thrds_started, int verbose)
{
	int thrd_status = THRD_STARTED;

	printf("start civetweb thread at %s %d\n", __FILE__, __LINE__);
	//std::vector<std::thread> thrds_started;
	{
		std::thread thrd(std::bind(&web_srvr_start, std::ref(q_from_srvr_to_clnt), std::ref(q_bin_from_srvr_to_clnt), std::ref(q_from_clnt_to_srvr), &thrd_status, verbose));
		thrds_started.push_back(std::move(thrd));
	}
	//web_push_from_srvr_to_clnt(q_from_srvr_to_clnt, 20);

	printf("waiting for thrd_status= %d at %s %d\n", thrd_status, __FILE__, __LINE__);
	while(thrd_status < THRD_RUNNING) {
#if defined(__linux__) || defined(__APPLE__)
		usleep(100000);
#else
		Sleep(100);
#endif
	}
	printf("waited for thrd_status= %d at %s %d\n", thrd_status, __FILE__, __LINE__);
	return thrd_status;
}

static std::string bin_map, bin_map2, chrts_cats;
static std::vector <std::string> chrts_json, chrts_json2;

static int compress_string(unsigned char *dst, unsigned char *src, size_t sz)
{
	z_stream defstream;
	defstream.zalloc = Z_NULL;
	defstream.zfree = Z_NULL;
	defstream.opaque = Z_NULL;
	defstream.avail_in = (uint32_t)(sz); // src sz, string + terminator
	defstream.next_in = (unsigned char *)src;
	defstream.avail_out = (uint32_t)sz; // dst sz
	defstream.next_out = (unsigned char *)dst;

	deflateInit(&defstream, Z_BEST_COMPRESSION);
	deflate(&defstream, Z_FINISH);
	deflateEnd(&defstream);

	int len = (int)(defstream.next_out - dst);
	fprintf(stderr, "len_in= %d, Compressed size is: %d\n", (int)sz, len);
	return len;
}


static int str_2_base64(uint8_t *dst, uint8_t *src, int isz)
{
	unsigned char *out;
	int i, len, olen, ilen;

	ilen = isz;
	len = Base64encode_len(ilen);
	printf("str_2_base64: ilen= %d, exp_len= %d\n", ilen, len);
	olen = Base64encode((char *)dst, (char *)src, ilen);
	printf("str_2_base64: olen= %d, strlen(dst)= %d\n", olen, (int)strlen((char *)dst));
	return olen;
}

static std::string cpu_diag_str, cpu_diag_flds;
static std::string cpu_diag_flds_beg="cpu_diagram_flds=";
static std::string cpu_diag_flds_filenm;

static void create_web_file(int verbose)
{
	double tm_beg = dclock();
	std::ifstream file, file2;
	std::string root_dir = std::string(get_root_dir_of_exe());
	std::string ds = std::string(DIR_SEP);
	std::string web_dir = root_dir + ds + ".." + ds + "web" + ds;
	std::string js_file, in_file = web_dir + "index.html";
	std::ofstream ofile;
	ofile.open (options.web_file.c_str(), std::ios::out);
	if (!ofile.is_open()) {
		printf("messed up open of web_file flnm= %s at %s %d\n", options.web_file.c_str(), __FILE__, __LINE__);
		exit(1);
	}

	chrts_json2 = chrts_json;
	if (chrts_json2.size() == 0) {
		fprintf(stderr, "chrts_json2.size() == 0. Bye at %s %d\n", __FILE__, __LINE__);
		exit(1);
	}
	bin_map2 = bin_map;
	replace_substr(bin_map2, "`", "'", verbose);
	replace_substr(bin_map2, "'", "\\\\'", verbose);
	if (bin_map2.size() == 0) {
		fprintf(stderr, "bin_map2.size() == 0. Bye at %s %d\n", __FILE__, __LINE__);
		exit(1);
	}
	//printf("bin_map2 at %s %d:\n%s\n", __FILE__, __LINE__, bin_map2.c_str());

	for (uint32_t i=0; i < chrts_json.size(); i++) {
		replace_substr(chrts_json2[i], "'", "\\\\'", verbose);
	}

	std::vector <std::string> cd_str;
	unsigned char *src, *dst, *cd_b64, *sp_b64;
	for (uint32_t i=0; i < chrts_json.size(); i++) {
		src = (uint8_t *)malloc(chrts_json2[i].size()+1);
		dst = (uint8_t *)malloc(chrts_json2[i].size()+1);
		memcpy(src, chrts_json2[i].c_str(), chrts_json2[i].size());
		int cd_cmp_len = compress_string(dst, src, chrts_json2[i].size());
		int cd_b64_len = Base64encode_len(cd_cmp_len);
		cd_b64 = (uint8_t *)malloc(cd_b64_len);
		int cd_ob64_len = str_2_base64(cd_b64, dst, cd_cmp_len);
		cd_str.push_back(std::string((const char *)cd_b64));
		free(src);
		free(dst);
		free(cd_b64);
	}

#if 0
	std::string wb_tmp = options.web_file+".dat";
	std::ofstream ofile2;
	ofile2.open (wb_tmp.c_str(), std::ios::out|std::ofstream::binary);
	if (!ofile2.is_open()) {
		printf("messed up open of web_file tmp flnm= %s at %s %d\n", wb_tmp.c_str(), __FILE__, __LINE__);
		exit(1);
	}
	ofile2.write((const char *)dst, cd_cmp_len);
	ofile2.close();
#endif

	src = (uint8_t *)malloc(bin_map2.size());
	dst = (uint8_t *)malloc(bin_map2.size());
	memcpy(src, bin_map2.c_str(), bin_map2.size());
	int sp_cmp_len = compress_string(dst, src, bin_map2.size());
	int sp_b64_len = Base64encode_len(sp_cmp_len);
	sp_b64 = (uint8_t *)malloc(sp_b64_len);
	int sp_ob64_len = str_2_base64(sp_b64, dst, sp_cmp_len);
	free(src);
	free(dst);

#if 0
	wb_tmp = options.web_file+".b64";
	ofile2.open (wb_tmp.c_str(), std::ios::out);
	if (!ofile2.is_open()) {
		printf("messed up open of web_file tmp flnm= %s at %s %d\n", wb_tmp.c_str(), __FILE__, __LINE__);
		exit(1);
	}
	ofile2.write((const char *)b64, ob64_len);
	ofile2.close();
#endif

	file.open (in_file.c_str(), std::ios::in);
	if (!file.is_open()) {
		printf("messed up fopen of flnm= %s at %s %d\n", in_file.c_str(), __FILE__, __LINE__);
		exit(1);
	}
	std::string line;
	int i = 0;
	std::string scr_str = "<script src=";
	std::string style = "<link rel=\"stylesheet\" href=";
	std::string openSocket = "openSocket(window.location.port);";
	std::string cpu_diag_svg_str = "<div id='cpu_diagram_svg'>";
	bool write_it, first_style=true;

	while(!file.eof()){
		std::getline (file, line);
		write_it = true;
		size_t pos = line.find(scr_str);
		if (pos != std::string::npos) {
			write_it = false;
			std::string scr = line.substr(pos+1+scr_str.size());
			pos = scr.find("\"");
			if (pos == std::string::npos) {
				printf("missed closing quote for index.html file for string: '%s'. Bye at %s %d\n",
						line.c_str(), __FILE__, __LINE__);
				exit(1);
			}
			scr = scr.substr(0, pos);
			printf("got script name '%s' at %s %d\n", scr.c_str(), __FILE__, __LINE__);
			std::string nm = web_dir + ds + scr;
			replace_substr(nm, "\\", "/", verbose);
			replace_substr(nm, "//", "/", verbose);
			bool doing_main_js = false;
			if (nm.find("main.js") != std::string::npos) {
				doing_main_js = true;
			}
			ofile << "<script>" << std::endl;
			ofile << "/* inlined file: '" << nm << "' below: */" << std::endl;
			file2.open (nm.c_str(), std::ios::in);
			if (!file2.is_open()) {
				printf("messed up fopen of flnm= %s at %s %d\n", nm.c_str(), __FILE__, __LINE__);
				exit(1);
			}
			std::string line2;
			while(!file2.eof()){
				std::getline (file2, line2);
				if (doing_main_js) {
					pos = line2.find(openSocket);
					if (pos != std::string::npos) {
						ofile << "    let sp_data2='" << std::string((const char *)sp_b64) << "';" << std::endl;
						ofile << "    let ch_data2=[];" << std::endl;
						ofile << "    gjson = JSON.parse('" << chrts_cats << "');" << std::endl;
						if (cpu_diag_flds.size() > 0) {
							replace_substr(cpu_diag_flds, "\n", " ", verbose);
							replace_substr(cpu_diag_flds, "'", "\'", verbose);
							std::string st2 = cpu_diag_flds.substr(cpu_diag_flds_beg.size(), std::string::npos);
							ofile << "    g_cpu_diagram_flds = JSON.parse('"+st2+"');" << std::endl;
						}
						ofile << "    gjson.chrt_data_sz = " << std::to_string(cd_str.size()) << ";" << std::endl;
						for (uint32_t i=0; i < cd_str.size(); i++) {
							ofile << "    ch_data2.push('" << cd_str[i] << "');" << std::endl;
						}
						ofile << "    standalone(sp_data2, ch_data2);" << std::endl;
						doing_main_js = false;
						continue;
					}
				}
				ofile << line2 << std::endl;
			}
			file2.close();
			ofile << "</script>" << std::endl;
			continue;
		}
		if (options.cpu_diagram.size() > 0) {
			ofile << line << std::endl;
			write_it = false;
			pos = line.find(cpu_diag_svg_str);
			//ofile << "    document.getElementById('svg-object').contentDocument = g_cpu_diagram_svg; " << std::endl;
			if (pos != std::string::npos) {
				file2.open (options.cpu_diagram.c_str(), std::ios::in);
				if (!file2.is_open()) {
					printf("messed up fopen of flnm= %s at %s %d\n", options.cpu_diagram.c_str(), __FILE__, __LINE__);
					exit(1);
				}
				std::string line2;
				while(!file2.eof()){
					std::getline (file2, line2);
					ofile << line2 << std::endl;
				}
				file2.close();
			}
		}
		pos = line.find(style);
		if (pos != std::string::npos) {
			if (first_style) {
				first_style = false;
				std::string nm = web_dir + ds + "fonts"+ds+"glyphicons-halflings-regular.woff2.css";
				replace_substr(nm, "\\", "/", verbose);
				replace_substr(nm, "//", "/", verbose);
				ofile << "<style>" << std::endl;
				ofile << "/* inlined file: '" << nm << "' below: */" << std::endl;
				file2.open (nm.c_str(), std::ios::in);
				if (!file2.is_open()) {
					printf("messed up fopen of flnm= %s at %s %d\n", nm.c_str(), __FILE__, __LINE__);
					exit(1);
				}
				std::string line2;
				while(!file2.eof()){
					std::getline (file2, line2);
					ofile << line2 << std::endl;
				}
				file2.close();
				ofile << "</style>" << std::endl;
			}
			write_it = false;
			std::string scr = line.substr(pos+1+style.size());
			pos = scr.find("\"");
			if (pos == std::string::npos) {
				printf("missed closing quote for index.html file for string: '%s'. Bye at %s %d\n",
						line.c_str(), __FILE__, __LINE__);
				exit(1);
			}
			scr = scr.substr(0, pos);
			printf("got style filename '%s' at %s %d\n", scr.c_str(), __FILE__, __LINE__);
			std::string nm = web_dir + ds + scr;
			replace_substr(nm, "\\", "/", verbose);
			replace_substr(nm, "//", "/", verbose);
			ofile << "<style>" << std::endl;
			ofile << "/* inlined file: '" << nm << "' below: */" << std::endl;
			file2.open (nm.c_str(), std::ios::in);
			if (!file2.is_open()) {
				printf("messed up fopen of flnm= %s at %s %d\n", nm.c_str(), __FILE__, __LINE__);
				exit(1);
			}
			std::string line2;
			while(!file2.eof()){
				std::getline (file2, line2);
				ofile << line2 << std::endl;
			}
			file2.close();
			ofile << "</style>" << std::endl;
			continue;
		}
		if (write_it) {
			ofile << line << std::endl;
		}
	}
	file.close();
	ofile.close();
	bin_map2.clear();
	chrts_json2.clear();
	//printf("bye at %s %d\n", __FILE__, __LINE__);
	//exit(1);
	//fprintf(stderr, "read str_pool.sz= %d and chrts_json.sz= %d from file: %s at %s %d\n",
	//		(int)bin_map.size(), (int)chrts_json.size(), options.replay_filename.c_str(), __FILE__, __LINE__);
	uint64_t sz = get_file_size(options.web_file.c_str(), __FILE__, __LINE__, verbose);
	double tm_now = dclock(), dsz = sz;
	fprintf(stderr, "create_web_file took %f secs to write %f MBs to %s at %s %d\n",
			tm_now-tm_beg, 1.0e-6*dsz, options.web_file.c_str(), __FILE__, __LINE__);
}

static void do_load_replay(int verbose)
{
	std::ifstream file;
	//long pos = 0;
	file.open (options.replay_filename.c_str(), std::ios::in);
	if (!file.is_open()) {
		printf("messed up fopen of flnm= %s at %s %d\n", options.replay_filename.c_str(), __FILE__, __LINE__);
		exit(1);
	}
	std::string line;
	int i = 0;
	while(!file.eof()){
		if (i == 0) {
			std::getline (file, bin_map);
			//bin_map = line;
		}
		if (i == 1) {
			std::getline (file, line);
		}
		if (i == 2) {
			std::getline (file, line);
			chrts_cats = line;
			break;
		}
		if (i > 2) {
			std::getline (file, line);
			chrts_json.push_back(line);
			break;
		}
		i++;
	}
	file.close();
	if (verbose > 1) {
		printf("read bin_map from file:\n%s\n", bin_map.c_str());
		for (uint32_t i=0; i < chrts_json.size(); i++) {
			printf("read chrts_json[%d]: len= %d at %s %d. data:\n%s\n",
					i, (int)chrts_json[i].size(), __FILE__, __LINE__, chrts_json[i].c_str());
		}
	}
	fprintf(stderr, "read str_pool.sz= %d and chrts_json.sz= %d from file: %s at %s %d\n",
			(int)bin_map.size(), (int)chrts_json.size(), options.replay_filename.c_str(), __FILE__, __LINE__);
}

static int read_perf_event_list_dump(file_list_str &file_list)
{
	// read the list of perf events and their format.
	// Create perf_event_list_dump.txt with dump_all_perf_events.sh (on linux of course).
	std::string base_file = "perf_event_list_dump.txt";
	std::string flnm = file_list.path + DIR_SEP + base_file;
	int rc = ck_filename_exists(flnm.c_str(), __FILE__, __LINE__, options.verbose);
	if (rc != 0) {
		fprintf(stderr, "Didn't find file %s. bye at %s %d\n", flnm.c_str(), __FILE__, __LINE__);
		exit(1);
	}
	printf("found file %s using filename= %s at %s %d\n", base_file.c_str(), flnm.c_str(), __FILE__, __LINE__);

	tp_read_event_formats(file_list, flnm, options.verbose);
	return 0;
}


static int ck_for_markers(int file_tag_idx, int po_idx, std::vector <prf_obj_str> &prf_obj, std::vector <marker_str> &marker_vec)
{
	std::string evt_nm;
	bool got_marker_beg_num = false, got_marker_end_num = false;
	uint32_t Mark_idx = UINT32_M1;

	if (prf_obj[po_idx].file_type == FILE_TYP_ETW) {
		Mark_idx = prf_obj[po_idx].etw_evts_hsh["Mark"] -1;
		if (Mark_idx != UINT32_M1) {
			int sz = (int)prf_obj[po_idx].etw_evts_set[Mark_idx].size();
			prf_obj[po_idx].events[Mark_idx].evt_count = sz;
			printf("Mark[%d] sz= %d at %s %d\n", Mark_idx, sz, __FILE__, __LINE__);
			for (uint32_t i=0; i < prf_obj[po_idx].etw_evts_set[Mark_idx].size(); i++) {
				uint32_t data_idx = prf_obj[po_idx].etw_evts_set[Mark_idx][i].data_idx;
				double tm = prf_obj[po_idx].tm_beg + 1.0e-6 * (double)prf_obj[po_idx].etw_evts_set[Mark_idx][i].ts;
				printf("marker[%d]: tm= %f, str= %s at %s %d\n", i, tm, 
					prf_obj[po_idx].etw_data[data_idx][2].c_str(), __FILE__, __LINE__);
				marker_str ms;
				if (options.marker_beg_num == i) {
					printf("use this marker as --marker_beg_num %d at %s %d\n", i, __FILE__, __LINE__);
					got_marker_beg_num = true;
					options.tm_clip_beg = tm + options.tm_clip_beg;
					ms.beg = true;
				}
				ms.text   = prf_obj[po_idx].etw_data[data_idx][2];
				if (options.marker_end_num == i) {
					printf("use this marker as --marker_end_num %d at %s %d\n", i, __FILE__, __LINE__);
					got_marker_end_num = true;
					options.tm_clip_end = tm + options.tm_clip_end;
					ms.end = true;
				}
				ms.ts_abs = tm;
				ms.evt_name = "Mark";
				ms.prf_obj_idx = po_idx;
				ms.file_tag_idx = file_tag_idx;
				ms.evt_idx_in_po = Mark_idx;
				ms.prf_sample_idx = i;
				std::size_t pos = ms.text.find("begin phase");
				if (pos != std::string::npos) {
					//phase_vec.push_back(ms);
				} else {
					pos = ms.text.find("end phase");
					if (pos != std::string::npos) {
						std::string lkfor = " dura= ";
						pos = ms.text.find(lkfor);
						if (pos != std::string::npos) {
							ms.dura = atof(ms.text.substr(pos+lkfor.size(), std::string::npos).c_str());
							printf("got end phase: '%s' dura= %f at %s %d\n", ms.text.c_str(), ms.dura, __FILE__, __LINE__);
						}
						phase_vec.push_back(ms);
					}
				}
				marker_vec.push_back(ms);
			}
		}
	} else {
		for (uint32_t j=0; j < prf_obj[po_idx].events.size(); j++) {
			if (prf_obj[po_idx].events[j].event_name.find(":") != std::string::npos) {
				evt_nm = prf_obj[po_idx].events[j].event_name;
			} else {
				if (prf_obj[po_idx].events[j].event_area.size() > 0) {
					evt_nm = prf_obj[po_idx].events[j].event_area + ":" + prf_obj[po_idx].events[j].event_name;
				} else {
					evt_nm = prf_obj[po_idx].events[j].event_name;
				}
			}
			if (evt_nm == "ftrace:print" || evt_nm == "print") {
				printf("got marker event in prf_obj[%d] event[%d] count= %d smples= %d at %s %d\n",
						po_idx, j, prf_obj[po_idx].events[j].evt_count,
						(int)prf_obj[po_idx].samples.size(),
						__FILE__, __LINE__);
				printf("prf_obj[%d].event[%d] = %s at %s %d\n", po_idx, j, evt_nm.c_str(), __FILE__, __LINE__);
				uint32_t mrkr=0;
				for (uint32_t i=0; i < prf_obj[po_idx].samples.size(); i++) {
#if 0
					printf("smpl[%d] evt= %s idx= %d at %s %d\n",
						i, prf_obj[po_idx].samples[i].event.c_str(),
						prf_obj[po_idx].samples[i].evt_idx, __FILE__, __LINE__);
#endif

					if (prf_obj[po_idx].samples[i].evt_idx != j) {
						continue;
					}
					marker_str ms;
					//double tm = prf_obj[po_idx].tm_beg + 1.0e-9 * (double)prf_obj[po_idx].samples[i].ts;
					double tm = 1.0e-9 * (double)prf_obj[po_idx].samples[i].ts;
					printf("marker[%d]: tm= %f str= %s at %s %d\n", mrkr, tm,
						prf_obj[po_idx].samples[i].extra_str.c_str(), __FILE__, __LINE__);
					if (options.marker_beg_num == mrkr) {
						printf("use this marker as --marker_beg_num %d at %s %d\n", mrkr, __FILE__, __LINE__);
						got_marker_beg_num = true;
						options.tm_clip_beg = tm + options.tm_clip_beg;
						ms.beg = true;
					}
					if (options.marker_end_num == mrkr) {
						printf("use this marker as --marker_end_num %d at %s %d\n", mrkr, __FILE__, __LINE__);
						got_marker_end_num = true;
						options.tm_clip_end = tm + options.tm_clip_end;
						ms.end = true;
					}
					ms.ts_abs = tm;
					ms.text   = prf_obj[po_idx].samples[i].extra_str;
					ms.evt_name = evt_nm;
					ms.prf_obj_idx = po_idx;
					ms.prf_sample_idx = i;
					ms.file_tag_idx = file_tag_idx;
					ms.evt_idx_in_po = j;
					std::size_t pos = ms.text.find("begin phase");
					if (pos != std::string::npos) {
						//phase_vec.push_back(ms);
					} else {
						pos = ms.text.find("end phase");
						if (pos != std::string::npos) {
							pos = ms.text.find(" dura= ");
							if (pos != std::string::npos) {
								ms.dura = atof(ms.text.substr(pos+6, std::string::npos).c_str());
								printf("got end phase: '%s' dura= %f at %s %d\n", ms.text.c_str(), ms.dura, __FILE__, __LINE__);
							}
							phase_vec.push_back(ms);
						}
					}
					marker_vec.push_back(ms);
					mrkr++;
				}
			}
		}
	}
	return (int)marker_vec.size();
}

static int phase_parse_text(std::string options, prf_obj_str &prf_obj, uint32_t po_idx, uint32_t file_tag_idx)
{
	std::string str;
	uint32_t ts_idx   = prf_obj.lua_data.timestamp_idx.size();
	if (ts_idx != 1) {
		printf("expected lua phase data to have timestamp_idx sz to be 1. Got %d. Bye at %s %d\n",
			ts_idx, __FILE__, __LINE__);
		exit(1);
	}
	ts_idx   = prf_obj.lua_data.timestamp_idx[0];
	uint32_t dura_idx = prf_obj.lua_data.duration_idx.size();
	if (dura_idx != 1) {
		printf("expected lua phase data to have duration_idx sz to be 1. Got %d. Bye at %s %d\n",
			dura_idx, __FILE__, __LINE__);
		exit(1);
	}
	dura_idx = prf_obj.lua_data.duration_idx[0];
	printf("lua phase: ts_idx= %d, dura_idx= %d, sz= %d at %s %d\n",
		ts_idx, dura_idx, (uint32_t)prf_obj.lua_data.data_rows.size(), __FILE__, __LINE__);
	if (prf_obj.lua_data.col_names.size() != 1) {
		printf("expected lua phase col_names to have sz == 1. Got %d. Bye at %s %d\n",
			(uint32_t)prf_obj.lua_data.col_names.size(), __FILE__, __LINE__);
		exit(1);
	}
	uint32_t extra_str_idx = UINT32_M1;
	if (options.find("USE_EXTRA_STR") != std::string::npos) {
		for (uint32_t i=0; i < prf_obj.lua_data.col_names[0].size(); i++) {
			if (prf_obj.lua_data.col_names[0][i] == "extra_str") {
				extra_str_idx = i;
				break;
			}
		}
	}
	for (uint32_t i=0; i < prf_obj.lua_data.data_rows.size(); i++) {
		marker_str ms;
		ms.ts_abs = prf_obj.lua_data.data_rows[i][ts_idx].dval;
		ms.dura   = prf_obj.lua_data.data_rows[i][dura_idx].dval;
		if (extra_str_idx == UINT32_M1) {
			ms.text = "Phase " + std::to_string(i);
		} else {
			ms.text = prf_obj.lua_data.data_rows[i][extra_str_idx].str;
		}
		ms.evt_name = "Marker";
		ms.prf_obj_idx = po_idx;
		ms.prf_sample_idx = i;
		ms.file_tag_idx = file_tag_idx;
		ms.evt_idx_in_po = 0;
		phase_vec.push_back(ms);
	}
	return 0;
}

struct tot_line_str {
	double ddiv, ts0, full_usage, xmin, xmax;
	int nr_cpus;
	uint32_t file_tag_idx, evt_tbl_idx, evt_idx, chrt;
	std::vector <double> line;
	std::vector <double> line_follow;
	std::vector <std::vector <double>> follow;
	std::vector <std::vector <double>> cputm;
	std::string follow_proc;
};

static std::vector <tot_line_str> tot_line;

static int ck_phase_update_event_table(std::vector <file_list_str> &file_list, uint32_t file_list_idx,
		uint32_t evt_tbl_idx, uint32_t evt_idx,
		uint32_t chrt, evt_str &event_table, int verbose)
{
	uint32_t dura_idx=UINT32_M1, ts_idx=UINT32_M1, xtra_idx=UINT32_M1, area_idx=UINT32_M1, mrkr_idx=UINT32_M1;
	uint32_t fsz = (uint32_t)event_table.flds.size();
	std::string got_more_fields;
	for (uint32_t j=0; j < fsz; j++) {
		uint64_t flg = event_table.flds[j].flags;
		if (flg & (uint64_t)fte_enum::FLD_TYP_DURATION_BEF) {
			dura_idx = j;
		} else if (flg & (uint64_t)fte_enum::FLD_TYP_TIMESTAMP) {
			ts_idx = j;
		} else if (event_table.flds[j].lkup == "marker") {
			mrkr_idx = j;
		} else if (event_table.flds[j].lkup == "area") {
			area_idx = j;
		} else if (event_table.flds[j].lkup == "extra_str") {
			xtra_idx = j;
		} else {
			got_more_fields = event_table.flds[j].lkup;
		}
	}
	if (dura_idx == UINT32_M1) {
		fprintf(stderr, "ck_phase_update_event_table: Didn't find a evt_flds with the lkup_typ flag= TYP_DURATION_BEF in %s. Bye at %s %d\n",
				event_table.charts[chrt].chart_tag.c_str(), __FILE__, __LINE__);
		exit(1);
	}
	if (ts_idx == UINT32_M1) {
		fprintf(stderr, "ck_phase_update_event_table: Didn't find a evt_flds with the lkup_typ flag= TYP_DURATION_BEF in %s. Bye at %s %d\n",
				event_table.charts[chrt].chart_tag.c_str(), __FILE__, __LINE__);
		exit(1);
	}
	if (xtra_idx == UINT32_M1) {
		fprintf(stderr, "ck_phase_update_event_table: Didn't find a evt_flds with the lkup= \"extra_str\" in %s. Bye at %s %d\n",
				event_table.charts[chrt].chart_tag.c_str(), __FILE__, __LINE__);
		exit(1);
	}
	if (mrkr_idx == UINT32_M1) {
		fprintf(stderr, "ck_phase_update_event_table: Didn't find a evt_flds with the lkup= \"marker\" in %s. Bye at %s %d\n",
				event_table.charts[chrt].chart_tag.c_str(), __FILE__, __LINE__);
		exit(1);
	}
	if (area_idx == UINT32_M1) {
		fprintf(stderr, "ck_phase_update_event_table: Didn't find a evt_flds with the lkup= \"area\" in %s. Bye at %s %d\n",
				event_table.charts[chrt].chart_tag.c_str(), __FILE__, __LINE__);
		exit(1);
	}
	if (got_more_fields.size() > 0) {
		fprintf(stderr, "ck_phase_update_event_table: Got unexpected evt_flds with the lkup= \"%s\" in %s. Bye at %s %d\n",
				event_table.charts[chrt].chart_tag.c_str(), got_more_fields.c_str(), __FILE__, __LINE__);
		exit(1);
	}
	if (event_table.data.ts.size() != event_table.data.vals.size()) {
		fprintf(stderr, "ck yer code dude! expected timestamp array.size()= %d == vals.size()= %d. bye at %s %d\n",
			(uint32_t)event_table.data.ts.size(), (uint32_t)event_table.data.vals.size(), __FILE__, __LINE__);
		exit(1);
	}
	for (uint32_t i = 0; i < event_table.data.vals.size(); i++) {
		uint32_t area_val = event_table.data.vals[i][area_idx]-1;
		uint32_t xtra_val = event_table.data.vals[i][xtra_idx]-1;
		if (area_val >= event_table.vec_str.size()) {
			fprintf(stderr, "area_val= %d out of range(%d). bye at %s %d\n",
				area_val, (uint32_t)event_table.vec_str.size(), __FILE__, __LINE__);
			exit(1);
		}
		if (xtra_val >= event_table.vec_str.size()) {
			fprintf(stderr, "extra_val= %d out of range(%d). bye at %s %d\n",
				xtra_val, (uint32_t)event_table.vec_str.size(), __FILE__, __LINE__);
			exit(1);
		}
		printf("phase_chart[%d] ts= %.3f, dura= %.3f, mrkr= %.3f area= %s, xtra= %s at %s %d\n",
				i, event_table.data.ts[i].ts, event_table.data.ts[i].duration,
				event_table.data.vals[i][mrkr_idx],
				event_table.vec_str[area_val].c_str(),
				event_table.vec_str[xtra_val].c_str(),
				__FILE__, __LINE__);
	}
	event_table.data.ts.resize(0);
	event_table.data.vals.resize(0);
	event_table.data.prf_sample_idx.resize(0);
	for (uint32_t i = 0; i < phase_vec.size(); i++) {
		std::vector <double> dv;
		struct ts_str tss;
		tss.ts       = phase_vec[i].ts_abs;
		tss.duration = phase_vec[i].dura;
		for (uint32_t j=0; j < fsz; j++) {
			if (j == ts_idx) {
				dv.push_back(tss.ts);
			} else if (j == dura_idx) {
				dv.push_back(tss.duration);
			} else if (j == area_idx) {
				std::string str = "phase";
				uint32_t idx = hash_string(event_table.hsh_str, event_table.vec_str, str);
				dv.push_back(idx);
			} else if (j == mrkr_idx) {
				dv.push_back(i+1);
			} else if (j == xtra_idx) {
				std::string str = phase_vec[i].text;
				uint32_t idx = hash_string(event_table.hsh_str, event_table.vec_str, str);
				dv.push_back(idx);
			} 
		}
		int psi = phase_vec[i].prf_sample_idx;
		if (psi < 0) {
			psi = 0;
		}
		event_table.data.prf_sample_idx.push_back(psi);
		event_table.data.ts.push_back(tss);
		event_table.data.vals.push_back(dv);
	}
	return 0;
}

static int ck_phase_single_multi(std::vector <file_list_str> &file_list, uint32_t file_list_idx,
		uint32_t evt_tbl_idx, uint32_t evt_idx,
		uint32_t chrt, std::vector <evt_str> &event_table, int verbose)
{
	//printf("ch_lines.size()= %d at %s %d\n", (uint32_t)ch_lines.size(), __FILE__, __LINE__);
	std::string json;
	std::string file_tag = file_list[file_list_idx].file_tag;
	uint32_t file_tag_idx = event_table[evt_idx].file_tag_idx;
	uint32_t phase_fl_idx = UINT32_M1;
	bool got_USE_AS_PHASE = false;
	bool got_FIND_MULTI_PHASE = false;
	bool got_FIND_IDLE_PHASE = false;
	uint32_t ufile_tag_idx = UINT32_M1;
	std::string fl_options;
	for (uint32_t i=0; i < file_list.size(); i++) {
		ufile_tag_idx = file_tag_hash[file_list[i].file_tag] - 1;
		printf("file_tag[%d]= %s, uft_idx= %d at %s %d\n", i, file_list[i].file_tag.c_str(), ufile_tag_idx, __FILE__, __LINE__);
		if (ufile_tag_idx == file_tag_idx && file_list[i].options.find("USE_AS_PHASE") != std::string::npos) {
			got_USE_AS_PHASE = true;
			if (phase_vec.size() > 0 && phase_vec[0].file_tag_idx == UINT32_M1) {
				for (uint32_t i=0; i < phase_vec.size(); i++) {
					if (phase_vec[i].file_tag_idx == UINT32_M1) {
						phase_vec[i].file_tag_idx = file_tag_idx;
					}
				}
			}
			if (file_list[i].options.find("FIND_MULTI_PHASE") != std::string::npos) {
				got_FIND_MULTI_PHASE = true;
			}
			if (file_list[i].options.find("FIND_IDLE_PHASE") != std::string::npos) {
				got_FIND_IDLE_PHASE = true;
			}
			fl_options = file_list[i].options;
			break;
		}
	}
	printf("ck_phase_single_multi: got_USE_AS_PHASE= %d, got_FIND_MULTI_PHASE= %d, got_FIND_IDLE_PHASE= %d, phase_vec.size()= %d options= %s at %s %d\n",
		got_USE_AS_PHASE, got_FIND_MULTI_PHASE, got_FIND_IDLE_PHASE, (uint32_t)phase_vec.size(), fl_options.c_str(), __FILE__, __LINE__);
	if ((!got_FIND_MULTI_PHASE && !got_FIND_IDLE_PHASE) || phase_vec.size() == 0) {
		return 0;
	}
	printf("ck_phase_single_multi: x_rng= %f, %f, y_rng= %f, %f\n",
			ch_lines.range.x_range[0],
			ch_lines.range.x_range[1],
			ch_lines.range.y_range[0],
			ch_lines.range.y_range[1]);
	int nr_cpus = ch_lines.prf_obj->features_nr_cpus_online;
	double full_usage = (double)nr_cpus * 100.0;
	int divs = 1+ (ch_lines.range.x_range[1] - ch_lines.range.x_range[0]) * 1000.0;
	double ddiv = (ch_lines.range.x_range[1] - ch_lines.range.x_range[0]) / (double)(divs);

	if (tot_line.size() <= file_tag_idx) {
		tot_line.resize(file_tag_idx+1);
	}
	tot_line[file_tag_idx].xmin = ch_lines.range.x_range[0];
	tot_line[file_tag_idx].ts0  = ch_lines.range.ts0;
	tot_line[file_tag_idx].xmax = ch_lines.range.x_range[1];
	tot_line[file_tag_idx].ddiv = ddiv;
	tot_line[file_tag_idx].line.resize(divs);
	tot_line[file_tag_idx].line_follow.resize(divs);
	std::string follow;
	std::vector <double> follow_cputime;
	if (options.follow_proc.size() > file_tag_idx) {
		follow = options.follow_proc[file_tag_idx];
		tot_line[file_tag_idx].follow_proc = follow;
		tot_line[file_tag_idx].follow.resize(nr_cpus);
		follow_cputime.resize(nr_cpus);
		for (int32_t i=0; i < nr_cpus; i++) {
			tot_line[file_tag_idx].follow[i].resize(divs);
		}
	}
	tot_line[file_tag_idx].cputm.resize(nr_cpus);
	for (int32_t i=0; i < nr_cpus; i++) {
		tot_line[file_tag_idx].cputm[i].resize(divs);
	}
	tot_line[file_tag_idx].file_tag_idx = file_tag_idx;
	tot_line[file_tag_idx].evt_tbl_idx = evt_tbl_idx;
	tot_line[file_tag_idx].evt_idx = evt_idx;
	tot_line[file_tag_idx].chrt = chrt;
	tot_line[file_tag_idx].full_usage = full_usage;
	tot_line[file_tag_idx].nr_cpus = nr_cpus;
	printf("divs= %d at %s %d\n", divs, __FILE__, __LINE__);
	fflush(NULL);
	
	//cpt_idx = (int)hash_comm_pid_tid(comm_pid_tid_hash[file_tag_idx], comm_pid_tid_vec[file_tag_idx], comm, pid, tid) - 1;
	int fllw_comm=0, fllw_text=0, fllw_stk=0;
	double unk_cputm = 0.0, fllw_cputm3=0.0;
	double ck_tot = 0;
	bool got_follow_any = false;
	for (uint32_t i=0; i < ch_lines.line.size(); i++) {
		//if (i > 0) { json += ", "; }
		// order of ival array values must agree with IVAL_* variables in main.js
		uint32_t cpt_idx = ch_lines.line[i].cpt_idx;
		uint32_t cpu     = ch_lines.line[i].cpu;
		double tx0 = ch_lines.line[i].x[0];
		double tx1 = ch_lines.line[i].x[1];
		if (cpt_idx >= comm_pid_tid_vec[file_tag_idx].size()) {
			if (verbose > 1)
				printf("got cpt_idx = %d, sz= %d at %s %d\n",
					cpt_idx, (uint32_t)comm_pid_tid_vec[file_tag_idx].size(), __FILE__, __LINE__);
			unk_cputm += tx1 - tx0;
			continue;
			exit(1);
		}
		uint32_t pid = comm_pid_tid_vec[file_tag_idx][cpt_idx].pid;
		uint32_t tid = comm_pid_tid_vec[file_tag_idx][cpt_idx].tid;
		if (pid == 0 && tid == 0) {
			//printf("skipping idle[%d]\n", i);
			continue;
		}
		//printf("ln[%d] comm= %s, pid= %d tid= %d\n", i, comm_pid_tid_vec[file_tag_idx][cpt_idx].comm.c_str(), pid, tid);
		uint32_t j = (tx0 - ch_lines.range.x_range[0])/ddiv;
		uint32_t jmx = 2 + (tx1 - tx0)/ddiv;
		double xdiff = ch_lines.range.x_range[1] - ch_lines.range.x_range[0];
		double relx0 = tx0 - ch_lines.range.x_range[0]; // beg offset in secs from beg
		double relx1 = tx1 - ch_lines.range.x_range[0]; // end offset in secs from beg
		relx0 /= xdiff; // % offset from beg (0.0 to 1.0)
		relx1 /= xdiff;
		double nrx0 = (double)divs * relx0; // index of beg index into array
		double nrx1 = (double)divs * relx1; // index of end index into array
		double xbeg = floor(nrx0); // beg indx
		double xend = ceil(nrx1);  // end indx
		uint32_t ibeg = (int32_t)(xbeg); // int beg indx
		uint32_t iend = (int32_t)(xend); // int end indx
		if (ibeg >= tot_line[file_tag_idx].line.size()) {
			printf("hmmm.... ibeg= %d divs= %d at %s %d\n", ibeg, divs, __FILE__, __LINE__);
			exit(1);
		}
		if (iend >= tot_line[file_tag_idx].line.size()) {
			iend = tot_line[file_tag_idx].line.size()-1;
		}
		bool got_follow = false;
		if (follow.size() > 0) {
			if (ch_lines.line[i].text.size() > 0 && ch_lines.line[i].text.find(follow) != std::string::npos) {
				got_follow = true;
				fllw_text++;
			} else if (ch_lines.line[i].text_arr.size() > 0) {
				for (uint32_t kk=0; kk < ch_lines.line[i].text_arr.size(); kk++) {
					if (ch_lines.line[i].text_arr[kk] >= 0 && callstack_vec[ch_lines.line[i].text_arr[kk]].find(follow) != std::string::npos) {
						got_follow = true;
						fllw_stk++;
						break;
					}
				}
			}
			if (!got_follow && 
					comm_pid_tid_vec[file_tag_idx][cpt_idx].comm.find(follow) != std::string::npos) {
				got_follow = true;
				fllw_comm++;
			}
			if (!got_follow && ch_lines.line[i].callstack_str_idxs.size() > 0) {
				for (uint32_t kk=0; kk < ch_lines.line[i].callstack_str_idxs.size(); kk++) {
					if (callstack_vec[ch_lines.line[i].callstack_str_idxs[kk]].find(follow) != std::string::npos) {
						got_follow = true;
						fllw_stk++;
						break;
					}
				}
			}
		}
		if (got_follow) {
			got_follow_any = true;
			fllw_cputm3 += tx1 - tx0;
		}
		for (uint32_t j=ibeg; j < iend; j++) {
			double xcur0, xcur1;
			// xcur0 is j's rel val array (0.0 - 1.0)
			if (j == ibeg) {
				xcur0 = nrx0/(double)divs;
			} else {
				xcur0 = (double)j/(double)divs;
			}
			// xcur1 is (j+1)'s rel val array (0.0 - 1.0)
			if ((j+1) == iend) {
				xcur1 = nrx1/(double)divs; // nxt rel
			} else {
				xcur1 = (double)(j+1)/(double)divs;
			}
			// ck is 1 division's cputm
			double ck = (xcur1 - xcur0) * xdiff;
			ck_tot += ck;
			double val = ch_lines.line[i].y[0] * (xcur1 - xcur0) * (double)divs;
			tot_line[file_tag_idx].line[j] += val;
			if (cpu >= 0 && cpu < nr_cpus && j < divs) {
				if (got_follow) {
					tot_line[file_tag_idx].follow[cpu][j] += ck;
					tot_line[file_tag_idx].line_follow[j] += 100.0 * ck;
					follow_cputime[cpu] += ck;
				}
				tot_line[file_tag_idx].cputm[cpu][j] += ck;
			}
		}
	}
	double ck_usage = 100.0;
	if (nr_cpus >= 2) {
		ck_usage = 200.0;
	}
	if (nr_cpus >= 3) {
		ck_usage = 300.0;
	}
	if (follow.size() > 0) {
		double tot_cputm = 0.001*(double)(tot_line[file_tag_idx].line.size());
		if (tot_cputm == 0.0) {
			tot_cputm = 1.0;
		}
		printf("ck_phase_single_multi: follow= %s, txt= %d, cmm= %d, stk= %d, cputm= %.3f at %s %d\n",
				follow.c_str(), fllw_text, fllw_comm, fllw_stk, tot_cputm, __FILE__, __LINE__);
		double fllw_tot = 0.0;
		for (uint32_t i=0; i < follow_cputime.size(); i++) {
			double c_tm = follow_cputime[i];
			printf("follow_cputime[%d]= %.3f, pct= %.3f%%\n",
					i, c_tm, 100.0*follow_cputime[i]/tot_cputm);
			fllw_tot += c_tm;
		}
		std::vector <double> ck_cpus;
		ck_cpus.resize(nr_cpus);
		double tot_cputm2 = 0.0;
		for (int32_t j=0; j < nr_cpus; j++) {
			for (uint32_t i=0; i < tot_line[file_tag_idx].cputm[j].size(); i++) {
				if (tot_line[file_tag_idx].cputm[j][i] > 0.0) {
					ck_cpus[j] += tot_line[file_tag_idx].follow[j][i]/tot_line[file_tag_idx].cputm[j][i];
					tot_cputm2 += tot_line[file_tag_idx].cputm[j][i];
				}
			}
		}
		printf("fllw_cpu_tm= %.3f secs, fllw_cputm3= %.3f, tot_cputm= %.3f tot_cputm+unk= %.3f, ck_tot= %.3f\n",
				fllw_tot, fllw_cputm3, tot_cputm2, unk_cputm+tot_cputm2, ck_tot);
		for (int32_t i=0; i < nr_cpus; i++) {
			printf("follow_cputime[%d]  pct= %.3f%%\n", i, 100.0 * ck_cpus[i]/(double)(divs));
		}
		
	}

	std::vector <double> multi_beg, idle_beg;
	multi_beg.resize(phase_vec.size());
	idle_beg.resize(phase_vec.size());
	// so 1 cpu is less than half busy. This scale assumes if 1 cpu is 100% then val=100.0, if all 4 cpus are 100% then val=400.0
	double cpus_idle_pct = 40.0;
	double cpus_min_idle_pct = 90.0;

	for (uint32_t i=0; i < phase_vec.size(); i++) {
		if (got_FIND_IDLE_PHASE && phase_vec[i].dura == 0.0) {
			// this better be the placeholder for the idle phase. gb_phase.lua is supposed to put a 0 dura event in for the idle phase
			continue;
		}
		double ph_end = phase_vec[i].ts_abs;
		double ph_beg = ph_end - phase_vec[i].dura;
		double tbeg = ph_beg - ch_lines.range.ts0;
		double tend = ph_end - ch_lines.range.ts0;
		multi_beg[i] = -1.0;
		idle_beg[i] = -1.0;
		uint32_t ibeg = 1+ (tbeg / ddiv);
		uint32_t iend = tend / ddiv;
		uint32_t imulti= UINT32_M1;
		if (iend > divs) {
			iend = divs;
		}
		if (got_FIND_MULTI_PHASE) {
			for (uint32_t j=ibeg; j < iend; j++) {
				if ((tot_line[file_tag_idx].line[j]+1) >= ck_usage) {
					if (verbose) {
					printf("phase[%d] beg multi  tm_diff= %.3f, tm_left= %.3f usage[%d]= %.3f\n",
						i, (double)(j-ibeg)*ddiv, tend-((double)(j)*ddiv), j, tot_line[file_tag_idx].line[j]);
					}
					multi_beg[i] = (double)(j-ibeg)*ddiv; // store offset from beg of phase
					break;
				}
			}
		}
		if (got_FIND_IDLE_PHASE) {
			bool got_idle_yet = false;
			for (uint32_t j=iend; j >= ibeg; j--) {
#if 1
				double tm0, tm1;
				if (got_follow_any) {
					tm0 = tot_line[file_tag_idx].line_follow[j]/ddiv;
				} else {
					tm0 = tot_line[file_tag_idx].line[j]/ddiv;
				}
				if (!got_idle_yet &&  (j-1) >= ibeg) {
					double tm1;
					if (got_follow_any) {
						tm1 = tot_line[file_tag_idx].line_follow[j-1]/ddiv;
					} else {
						tm1 = tot_line[file_tag_idx].line[j-1]/ddiv;
					}
					if (tm0 <= cpus_idle_pct && tm1 <= cpus_idle_pct) {
						got_idle_yet = true;
					}
				}
#endif
#if 0
				if (i== 2) {
					printf("i= %d, j= %d, tm0= %f, got_idle_yet= %d cpus_idle_pct= %f ddiv= %f at %s %d\n",
							i, j, tm0, got_idle_yet, cpus_idle_pct, ddiv, __FILE__, __LINE__);
				}
#endif
				if (got_idle_yet && tm0 >= 100.0) {
					if (verbose) {
						printf("phase[%d] beg idle  tm_diff= %.3f, tm_left= %.3f usage[%d]= %.3f\n",
							i, (double)(j-ibeg)*ddiv, tend-((double)(j)*ddiv), j, tot_line[file_tag_idx].line[j]);
					}
					idle_beg[i] = (double)(j-ibeg)*ddiv; // store offset from beg of phase
					break;
				}
			}
		}
	}
	printf("phase_vec.sz beg= %d at %s %d\n", (uint32_t)(phase_vec.size()), __FILE__, __LINE__);
	if (verbose) {
		for (uint32_t i=0; i < phase_vec.size(); i++) {
			double ph_end = phase_vec[i].ts_abs;
			double ph_beg = ph_end - phase_vec[i].dura;
			printf("phase[%d] beg= %.3f end= %.3f text= %s\n", i, ph_beg, ph_end, phase_vec[i].text.c_str());
		}
	}
	printf("phase_vec.sz beg loop at %s %d\n", __FILE__, __LINE__);
	fflush(NULL);
	int32_t pv_sz_m1 = phase_vec.size()-1;
	int32_t mrkr_sz_m1 = marker_vec.size()-1;
	for (int32_t i=pv_sz_m1; i >= 0; i--) {
		if (got_FIND_MULTI_PHASE && multi_beg[i] != -1.0) {
			// copy cur phase after current entry
			// end tm of new entry is the same
			// dur tm of new entry needs to be reduced by multi_beg[i]
			// end tm of old entry is the old end tm - multi_beg[i]
			// dur tm of old entry needs to be set to multi_beg[i]
			if (phase_vec[i+1].text.find(" multi:") == std::string::npos) {
				// no multi core score so probably not really multi-core score
				continue;
			}
			phase_vec.insert(phase_vec.begin()+i+1, phase_vec[i]);
			printf("insert bef i+1= %d, phase_vec.sz beg= %d at %s %d\n", i+1, (uint32_t)(phase_vec.size()), __FILE__, __LINE__);
			phase_vec[i+1].dura -= multi_beg[i];
			phase_vec[i+1].text = "multi-core: " + phase_vec[i+1].text;
			phase_vec[i].ts_abs -= phase_vec[i].dura - multi_beg[i];
			phase_vec[i].dura    = multi_beg[i];
			if (mrkr_sz_m1 == pv_sz_m1) {
				marker_vec.insert(marker_vec.begin()+i+1, marker_vec[i]);
				marker_vec[i+1].dura -= multi_beg[i];
				marker_vec[i+1].text = "multi-core: " + marker_vec[i+1].text;
				marker_vec[i].ts_abs -= marker_vec[i].dura - multi_beg[i];
				marker_vec[i].dura    = multi_beg[i];
			}
		}
		if (got_FIND_IDLE_PHASE && idle_beg[i] != -1.0) {
			double ph_end = phase_vec[i].ts_abs;
			double ph_end_orig = phase_vec[i].ts_abs;
			double ph_dur_orig = phase_vec[i].dura;
			double ph_beg = ph_end - phase_vec[i].dura;
			//printf("find_idle: beg[%d] ph_beg= %f, ph_end= %f, dura= %f\n", i, ph_beg, ph_end, phase_vec[i].dura);
			//fflush(NULL);
			ph_end = ph_beg + idle_beg[i];
			if ((i+1) <= pv_sz_m1 && phase_vec[i+1].dura == 0) {
				phase_vec[i+1].dura = ph_dur_orig - idle_beg[i];
			}
			phase_vec[i].dura = idle_beg[i];
			phase_vec[i].ts_abs = ph_end;
			ph_end = phase_vec[i].ts_abs;
			ph_beg = ph_end - phase_vec[i].dura;
			//printf("find_idle: aft[%d] ph_beg= %f, ph_end= %f, dura= %f sz= %d\n",
			//		i, ph_beg, ph_end, phase_vec[i].dura, (uint32_t)(phase_vec.size()));
			//fflush(NULL);
		}
	}
	printf("phase_vec.sz aft= %d at %s %d\n", (uint32_t)(phase_vec.size()), __FILE__, __LINE__);
	fflush(NULL);
	for (uint32_t i=0; i < phase_vec.size(); i++) {
		double ph_end = phase_vec[i].ts_abs;
		double ph_beg = ph_end - phase_vec[i].dura;
		printf("phase[%d] beg= %.3f end= %.3f dura= %.3f text= %s\n", i, ph_beg, ph_end, phase_vec[i].dura, phase_vec[i].text.c_str());
	}
	printf("ck_phase_single_multi: finished at %s %d\n", __FILE__, __LINE__);
	fprintf(stderr, "ck_phase_single_multi: finished at %s %d\n", __FILE__, __LINE__);
	fflush(NULL);
	return 0;
}

static int read_cpu_diag_flds_file(std::string flds_file)
{
	std::ifstream file2;
	std::string line2;

	printf("flds_file= %s at %s %d\n", flds_file.c_str(), __FILE__, __LINE__);
	file2.open (flds_file.c_str(), std::ios::in);
	if (!file2.is_open()) {
		printf("messed up fopen of flnm= %s at %s %d\n", flds_file.c_str(), __FILE__, __LINE__);
		exit(1);
	}
	cpu_diag_flds = cpu_diag_flds_beg;
	std::string cpu_diag_json;
	while(!file2.eof()) {
		std::getline (file2, line2);
		cpu_diag_flds += line2 + "\n";
		cpu_diag_json += line2;
	}
	file2.close();
	nlohmann::json jobj;
	ck_json(cpu_diag_json, "check for valid json in cpu_diagram .flds file: "+flds_file, false, jobj, __FILE__, __LINE__, options.verbose);
	if (options.verbose > 0) {
		printf("cpu_diag_flds= '%s' at %s %d\n", cpu_diag_flds.c_str(), __FILE__, __LINE__);
	}
	return 0;
}

int main(int argc, char **argv)
{
	std::vector <std::vector <evt_str>> event_table, evt_tbl2;
	std::vector<std::thread> thrds_started;
	Queue<std::string> q_from_srvr_to_clnt;
	Queue<std::string> q_bin_from_srvr_to_clnt;
	Queue<std::string> q_from_clnt_to_srvr;

	signal(SIGABRT, &sighandler);
	signal(SIGTERM, &sighandler);
	signal(SIGINT, &sighandler);

	set_root_dir_of_exe(argv[0]);

	printf("root_dir_of_exe= %s at %s %d\n", get_root_dir_of_exe(), __FILE__, __LINE__);
#ifdef _WIN32
	// on windows, if the 1st arg is '--debug' then we launch the debugger with the cur processid
	// so that the user can attach the debugger to this process.
	if (argc > 1 && strcmp(argv[1], "--debug") == 0) {
		int pid = (int)GetCurrentProcessId();
		std::string cmd = "vsjitdebugger.exe -p " + std::to_string(pid);
		system(cmd.c_str());
		_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF | _CRTDBG_CHECK_ALWAYS_DF );
	}
#endif

	get_opt_main (argc, argv);

	if (options.j2x.size() > 0) {
		std::string fl = "json_table.json";

		int rc = ck_filename_exists(fl.c_str(), __FILE__, __LINE__, options.verbose);
		if (rc != 0) {
			fprintf(stderr, "you entered '--j2x %s' but didn't find input file %s. bye at %s %d\n",
					options.j2x.c_str(), fl.c_str(), __FILE__, __LINE__);
			exit(1);
		}
		std::ifstream file2;
		file2.open (fl.c_str(), std::ios::in);
		if (!file2.is_open()) {
			printf("messed up fopen of flnm= %s at %s %d\n", fl.c_str(), __FILE__, __LINE__);
			exit(1);
		}
		std::string json_table;
		std::string line2;
		while(!file2.eof()){
			std::getline (file2, line2);
			json_table += line2;
		}
		file2.close();
		json_2_xls(json_table, options.j2x, "json_table.json data", __FILE__, __LINE__, options.verbose);
		printf("finished converting %s to %s at %s %d\n", fl.c_str(), options.j2x.c_str(), __FILE__, __LINE__);
		exit(0);
	}
	int verbose = options.verbose;
	double tm_beg = dclock();
	int thrd_status = start_web_server_threads(q_from_srvr_to_clnt, q_bin_from_srvr_to_clnt, q_from_clnt_to_srvr,
			thrds_started, verbose);

	std::vector <file_list_str> file_list;
	std::vector <int> grp_list;

	if (options.load_replay_file) {
		do_load_replay(verbose);
		//goto load_replay_file;
	} else {


	std::string data_files = "input_data_files.json";
	std::string chart_file = "charts.json";


	std::vector <std::string> use_file_tags;

	if (options.file_tag_vec.size() > 0) {
		use_file_tags = options.file_tag_vec;
	}

	if (options.data_file.size() > 0) {
		data_files = options.data_file;
	}
	std::string flnm, base_file = data_files;
	std::vector <std::string> tried_names;
	if (options.file_list.size() > 0) {
		int file_mode = FILE_MODE_ALL|FILE_MODE_REPLACE_CUR_DIR_WITH_ROOT;
		std::vector <std::string> dummy_file_tags;
		for (uint32_t fls=0; fls < options.file_list.size(); fls++) {
			flnm = options.file_list[fls].path + DIR_SEP + options.file_list[fls].file;
			printf("try reading file_list[%d]= %s at %s %d\n", fls, flnm.c_str(), __FILE__, __LINE__);
			read_file_list_json(flnm, file_list, dummy_file_tags,
					file_mode, options.file_list[fls].path, verbose);
			printf("aft reading: file_list.sz()= %d path= %s at %s %d\n", (int)file_list.size(),
					options.file_list[fls].path.c_str(), __FILE__, __LINE__);
		}
	} else if (options.perf_bin.size() == 0 && options.tc_bin.size() == 0 && options.lua_wait.size() == 0 &&
			options.etw_txt.size() == 0) {
		// getopt ensures that if perf_bin.sz > 0 then perf_txt.sz > 0 too
		// getopt ensures that if tc_bin.sz > 0 then tc_txt.sz > 0 too
		// same for lua_energy, lua_energy2 and lua_wait too
		int rc = search_for_file(base_file, flnm, tried_names, __FILE__, __LINE__, verbose);
		if (rc != 0) {
			search_for_file(base_file, flnm, tried_names, __FILE__, __LINE__, 1);
			fprintf(stderr, "bye at %s %d\n", __FILE__, __LINE__);
			exit(1);
		}
		printf("found data file list %s using filename= %s at %s %d\n", base_file.c_str(), flnm.c_str(), __FILE__, __LINE__);
		data_files = flnm;
		std::string use_root;
		if (options.root_data_dirs.size() > 0) {
			use_root = options.root_data_dirs[0];
		}
		read_file_list_json(data_files, file_list, use_file_tags, options.file_mode, use_root, verbose);
	} else {
		// this is the case where each file is entered on the command line
		if (options.perf_bin.size() > 0) {
			file_list_str fls;
			fls.file_bin = options.perf_bin;
			fls.file_txt = options.perf_txt;
			if (options.file_tag_vec.size() > 0) {
				fls.file_tag = options.file_tag_vec[0];
			}
			fls.typ = FILE_TYP_PERF;
			file_list.push_back(fls);
		}
		if (options.tc_bin.size() > 0) {
			file_list_str fls;
			fls.file_bin = options.tc_bin;
			fls.file_txt = options.tc_txt;
			if (options.file_tag_vec.size() > 0) {
				fls.file_tag = options.file_tag_vec[0];
			}
			fls.typ = FILE_TYP_TRC_CMD;
			file_list.push_back(fls);
		}
		if (options.lua_wait.size() > 0) {
			file_list_str fls;
			fls.file_bin = options.lua_energy;
			fls.file_txt = options.lua_energy2;
			fls.wait_txt = options.lua_wait;
			if (options.file_tag_vec.size() > 0) {
				fls.file_tag = options.file_tag_vec[0];
			}
			fls.typ = FILE_TYP_LUA;
			file_list.push_back(fls);
		}
		if (options.etw_txt.size() > 0) {
			file_list_str fls;
			fls.file_txt = options.etw_txt;
			if (options.file_tag_vec.size() > 0) {
				fls.file_tag = options.file_tag_vec[0];
			}
			fls.typ = FILE_TYP_ETW;
			file_list.push_back(fls);
		}
	}

	for (uint32_t ii=0; ii < file_list.size(); ii++) {
		file_list[ii].idx = ii;
	}
	if (options.chart_file.size() > 0) {
		chart_file = options.chart_file;
	}
	base_file = chart_file;
	int rc = search_for_file(base_file, flnm, tried_names, __FILE__, __LINE__, verbose);
	if (rc != 0) {
		search_for_file(base_file, flnm, tried_names, __FILE__, __LINE__, 1);
		fprintf(stderr, "bye at %s %d\n", __FILE__, __LINE__);
		exit(1);
	}
	printf("found chart.json file %s using filename= %s at %s %d\n", base_file.c_str(), flnm.c_str(), __FILE__, __LINE__);
	chart_file = flnm;

	if (file_list.size() == 0) {
		if (use_file_tags.size() > 0) {
			for (uint32_t ft=0; ft < use_file_tags.size(); ft++) {
				printf("selected input_data_files with tag= '%s' but no files matched in %s. Bye at %s %d\n",
					use_file_tags[ft].c_str(), data_files.c_str(), __FILE__, __LINE__);
				fprintf(stderr, "selected input_data_files with tag= '%s' but no files matched in %s. Bye at %s %d\n",
					use_file_tags[ft].c_str(), data_files.c_str(), __FILE__, __LINE__);
			}
			exit(1);
		}
		printf("Found no files in input data file %s. Bye at %s %d\n", data_files.c_str(), __FILE__, __LINE__);
		fprintf(stderr, "Found no files in input data file %s. Bye at %s %d\n", data_files.c_str(), __FILE__, __LINE__);
		exit(1);
	}

	std::vector <prf_obj_str> prf_obj;
	int grps = 0;
	int prv_grp = -2, grp_min, grp_max;
	printf("file_list.sz= %d at %s %d\n", (int)file_list.size(), __FILE__, __LINE__);
	for (uint32_t i=0; i < file_list.size(); i++) {
		if (i==0) {
			grp_min = file_list[i].grp;
			grp_max = file_list[i].grp;
		}
		if (grp_min > file_list[i].grp) {
			grp_min = file_list[i].grp;
		}
		if (grp_max < file_list[i].grp) {
			grp_max = file_list[i].grp;
		}
		file_list[i].grp = (int)i; // eventually I'd like to have files grouped together but now just set it to filenumber.
		if (file_list[i].grp != prv_grp) {
			prv_grp = file_list[i].grp;
			grp_list.push_back(prv_grp);
			printf("grp[%d] grp_list[%d]= %d at %s %d\n", i, i, prv_grp, __FILE__, __LINE__);
			grps++;
		}
	}

	std::string json_evt_chrt_str = rd_json(chart_file);

#if 0
	// read the list of perf events and their format.
	// Create perf_event_list_dump.txt with dump_all_perf_events.sh (on linux of course).
	base_file = "perf_event_list_dump.txt";
	if (options.root_data_dirs.size() > 0) {
		flnm = options.root_data_dirs[0] + DIR_SEP + base_file;
		rc = ck_filename_exists(flnm.c_str(), __FILE__, __LINE__, options.verbose);
		if (rc != 0) {
			rc = search_for_file(base_file, flnm, tried_names, __FILE__, __LINE__, verbose);
			if (rc != 0) {
				search_for_file(base_file, flnm, tried_names, __FILE__, __LINE__, 1);
				fprintf(stderr, "bye at %s %d\n", __FILE__, __LINE__);
				exit(1);
			}
		}
	}
	printf("found file %s using filename= %s at %s %d\n", base_file.c_str(), flnm.c_str(), __FILE__, __LINE__);

	tp_read_event_formats(flnm, options.verbose);
#endif

	prf_obj.resize(file_list.size());

	prf_obj_str *prf_obj_prv = NULL;

	uint32_t ck_num_events = do_json_evt_chrts_defaults(chart_file, json_evt_chrt_str, options.verbose);
	evt_tbl2.resize(1);
	for (uint32_t i=0; i <  ck_num_events; i++) {
		std::string evt_nm;
		do_json(i, evt_nm, chart_file, json_evt_chrt_str, evt_tbl2[0], "", options.verbose);
	}
	printf("chart_file has %d events. At %s %d\n", ck_num_events, __FILE__, __LINE__);
	std::cout << "evt_tbl2[0].size() " << evt_tbl2[0].size() << std::endl;
	if (verbose > 0) {
		for (uint32_t i=0; i <  evt_tbl2[0].size(); i++) {
			printf("evt_tbl2[0].event_name[%d]= %s, type= %s\n",
					i, evt_tbl2[0][i].event_name.c_str(), evt_tbl2[0][i].event_type.c_str());
			if (evt_tbl2[0][i].evt_derived.evts_used.size() > 0) {
				for (uint32_t j=0; j <  evt_tbl2[0][i].evt_derived.evts_used.size(); j++) {
					printf("\tevt_derived.evts_used[%d]= %s\n", j, evt_tbl2[0][i].evt_derived.evts_used[j].c_str());
				}
				printf("\tevt_derived.evt_trigger= %s\n", evt_tbl2[0][i].evt_derived.evt_trigger.c_str());
				printf("\tevt_derived.lua_file= %s rtn= %s\n",
						evt_tbl2[0][i].evt_derived.lua_file.c_str(), evt_tbl2[0][i].evt_derived.lua_rtn.c_str());
			}
		}
	}

	std::vector <int> file_list_1st;
	uint32_t file_tag_idx_prev = UINT32_M1, use_i=0;
	bool need_perf_event_list_file = false;
	for (uint32_t i=0; i < file_list.size(); i++) {
		if (file_list[i].typ == FILE_TYP_TRC_CMD ||
			file_list[i].typ == FILE_TYP_PERF) {
			need_perf_event_list_file = true;
			break;
		}
	}

	for (uint32_t i=0; i < file_list.size(); i++) {
		int v_tmp = options.verbose;
		std::string file_tag = file_list[i].file_tag;
		uint32_t file_tag_idx = hash_string(file_tag_hash, file_tag_vec, file_tag) - 1;
		if (file_tag_idx != UINT32_M1 && file_tag_idx >= flnm_evt_hash.size()) {
			flnm_evt_hash.resize(file_tag_idx+1);
			flnm_evt_vec.resize(file_tag_idx+1);
			comm_pid_tid_hash.resize(file_tag_idx+1);
			comm_pid_tid_vec.resize(file_tag_idx+1);
			if (need_perf_event_list_file) {
				read_perf_event_list_dump(file_list[i]);
			}
			printf("read_perf_event_list_dump(file_list[%d]) at %s %d\n", file_tag_idx, __FILE__, __LINE__);
		}
		if (file_list[i].use_line != "y") {
			printf("skipping file_list line[%d] due to use:%s in at %s %d\n",
					i, file_list[i].use_line.c_str(), __FILE__, __LINE__);
			continue;
		}
		if (file_tag_idx_prev != file_tag_idx) {
			file_tag_idx_prev = file_tag_idx;
			use_i = i;
		}
		file_list_1st.push_back(use_i);
		prf_obj[i].file_type = file_list[i].typ;
		prf_obj[i].file_tag_idx = file_tag_idx;
		if (file_list[i].file_bin.size() > 0) {
			replace_substr(file_list[i].file_bin, "\\", "/", verbose);
		}
		if (file_list[i].file_txt.size() > 0) {
			replace_substr(file_list[i].file_txt, "\\", "/", verbose);
		}
		if (file_list[i].wait_txt.size() > 0) {
			replace_substr(file_list[i].wait_txt, "\\", "/", verbose);
		}

		if (file_list[i].typ == FILE_TYP_TRC_CMD) {
			tc_read_data_bin(file_list[i].file_bin, v_tmp, prf_obj[i], tm_beg, prf_obj_prv, file_list[file_list_1st[i]]);
			fprintf(stderr, "after tc_read_data_bin: elap= %.3f, tc_read_data_bin(%s) at %s %d\n", dclock()-tm_beg, file_list[i].file_bin.c_str(), __FILE__, __LINE__);
			tc_parse_text(file_list[i].file_txt, prf_obj[i], tm_beg, v_tmp, evt_tbl2[0]);
			fprintf(stderr, "after tc_parse_text: elap= %.3f, tc_read_data_txt(%s) at %s %d\n", dclock()-tm_beg, file_list[i].file_txt.c_str(), __FILE__, __LINE__);
		} else if (file_list[i].typ == FILE_TYP_PERF) {
			if (verbose)
				fprintf(stderr, "begin prf_read_data_bin(i=%d) elap= %f at %s %d\n", i, dclock()-tm_beg, __FILE__, __LINE__);
			prf_read_data_bin(file_list[i].file_bin, v_tmp, prf_obj[i], tm_beg, file_list[file_list_1st[i]]);
			prf_obj_prv = &prf_obj[i];
			if (verbose)
				fprintf(stderr, "begin prf_parse_text(i=%d) elap= %f at %s %d\n", i, dclock()-tm_beg, __FILE__, __LINE__);
			fprintf(stderr, "befor prf_parse_text(i=%d) elap= %f flnm= %s at %s %d\n",
					i, dclock()-tm_beg, file_list[i].file_txt.c_str(), __FILE__, __LINE__);
			prf_parse_text(file_list[i].file_txt, prf_obj[i], tm_beg, v_tmp, evt_tbl2[0]);
			fprintf(stderr, "after prf_parse_text(i=%d) elap= %f flnm= %s at %s %d\n",
					i, dclock()-tm_beg, file_list[i].file_txt.c_str(), __FILE__, __LINE__);
		} else if (file_list[i].typ == FILE_TYP_LUA) {
			std::string lua_file = file_list[i].lua_file;
			std::string lua_rtn  = file_list[i].lua_rtn;
			if (lua_file == "") {
				lua_file = "test_01.lua";
			}
			if (lua_rtn == "") {
				lua_rtn = "do_tst";
			}
			if (verbose)
				fprintf(stderr, "begin lua_read__data(i=%d) elap= %.3f lua_file= %s lua_rtn= %s at %s %d\n",
						i, dclock()-tm_beg, lua_file.c_str(), lua_rtn.c_str(), __FILE__, __LINE__);
			lua_read_data(file_list[i].file_bin, file_list[i].file_txt,
					file_list[i].wait_txt, prf_obj[i], lua_file, lua_rtn, verbose); // need to pass lua script filename and lua routine name
			if (prf_obj[i].filename_bin == "" && file_list[i].file_bin != "") {
				prf_obj[i].filename_bin = file_list[i].file_bin;
			}
			if (prf_obj[i].filename_text == "" && file_list[i].file_txt != "") {
				prf_obj[i].filename_text = file_list[i].file_txt;
			}
			if (file_list[i].wait_txt != "") {
				prf_obj[i].filename_text += (prf_obj[i].filename_text != "" ? " " : "") + file_list[i].wait_txt;
			}
			prf_obj[i].filename_text += (prf_obj[i].filename_text != "" ? " " : "") + lua_file;
			prf_obj[i].filename_text += (prf_obj[i].filename_text != "" ? " " : "") + lua_rtn;
			fprintf(stderr, "begin lua_read_data(i=%d) elap= %f at %s %d\n", i, dclock()-tm_beg, __FILE__, __LINE__);
			if (file_list[i].options.find("USE_AS_PHASE") != std::string::npos) {
				phase_parse_text(file_list[i].options, prf_obj[i], i, file_tag_idx);
				printf("\nafter phase_read_data_txt(%s) at %s %d\n", file_list[i].file_txt.c_str(), __FILE__, __LINE__);
			}
			//fprintf(stderr, "after prf_parse_text(i=%d) elap= %f at %s %d\n", i, dclock()-tm_beg, __FILE__, __LINE__);
		} else if (file_list[i].typ == FILE_TYP_ETW) {
			if (verbose)
				fprintf(stderr, "begin etw_parse_text(i=%d) elap= %f at %s %d\n", i, dclock()-tm_beg, __FILE__, __LINE__);
			etw_parse_text(file_list[i].file_txt, prf_obj[i], tm_beg, v_tmp, evt_tbl2[0]);
			fprintf(stderr, "after etw_parse_text(i=%d) elap= %f at %s %d\n", i, dclock()-tm_beg, __FILE__, __LINE__);
		} else {
			fprintf(stderr, "unknown file typ(%d) at %s %d\n", file_list[i].typ, __FILE__, __LINE__);
			exit(1);
		}
		if (get_signal() == 1) {
			fprintf(stderr, "got control-c or 'quit' command from browser. Bye at %s %d\n", __FILE__, __LINE__);
			exit(1);
		}
		printf("prf_obj[%d].sample.size()= %d at %s %d\n", i, (int)prf_obj[i].samples.size(), __FILE__, __LINE__);
		ck_for_markers(file_tag_idx, i, prf_obj, marker_vec);
	}
	if (options.marker_beg_num != -1 && marker_vec.size() < (options.marker_beg_num+1)) {
		fprintf(stderr, "you enter '--marker_beg_num %d' but marker array size= %d. Bye at %s %d\n",
				options.marker_beg_num, (int)marker_vec.size(), __FILE__, __LINE__);
		exit(1);
	}
	if (options.marker_end_num != -1 && marker_vec.size() < (options.marker_end_num+1)) {
		fprintf(stderr, "you enter '--marker_end_num %d' but marker array size= %d. Bye at %s %d\n",
				options.marker_end_num, (int)marker_vec.size(), __FILE__, __LINE__);
		exit(1);
	}

	event_table.resize(grp_list.size());

	fprintf(stderr, "before fill_data_table: grp_list.size()= %d elap_tm= %.3f at %s %d\n",
			(int)grp_list.size(), dclock() - tm_beg, __FILE__, __LINE__);

	double tmc = 0.0;
	double tmc2 = 0.0;
	for (uint32_t g=0; g < grp_list.size(); g++) {
		for (uint32_t k=0; k < file_list.size(); k++) {
			//if (verbose)
				printf("bef fill_data_table: prf_obj[%d].events.size()= %d grp_list[%d]= %d at %s %d\n",
						k, (int)prf_obj[k].events.size(), g, grp_list[g], __FILE__, __LINE__);
			if (file_list[k].grp != grp_list[g]) {
				continue;
			}
			std::string evt_nm, evt_area;
			double tt0a = dclock();
			for (uint32_t j=0; j < prf_obj[k].events.size(); j++) {
				if (prf_obj[k].file_type == FILE_TYP_ETW) {
					evt_nm = prf_obj[k].events[j].event_name;
				} else {
					size_t pos = prf_obj[k].events[j].event_name.find(":");
				   	if (pos == std::string::npos) {
						evt_nm   = prf_obj[k].events[j].event_name;
					} else {
						evt_area = prf_obj[k].events[j].event_name.substr(0, pos);
						if (prf_obj[k].events[j].event_area.size() == 0) {
							prf_obj[k].events[j].event_area = evt_area;
						}
						prf_obj[k].events[j].event_name = prf_obj[k].events[j].event_name.substr(pos+1,
							prf_obj[k].events[j].event_name.size());
					}
					if (prf_obj[k].events[j].event_area.size() > 0) {
						evt_nm = prf_obj[k].events[j].event_area + ":" + prf_obj[k].events[j].event_name;
					} else {
						evt_nm = prf_obj[k].events[j].event_name;
					}
				}
				prf_obj[k].events[j].event_name_w_area = evt_nm;
				std::string str = prf_obj[k].filename_bin + " " + prf_obj[k].filename_text + " " + evt_nm;
				uint32_t file_tag_idx = prf_obj[k].file_tag_idx;
				int fe_idx = (int)hash_flnm_evt(flnm_evt_hash[file_tag_idx], flnm_evt_vec[file_tag_idx],
					str, prf_obj[k].filename_bin, prf_obj[k].filename_text,  evt_nm, k, j, -1) - 1;
				//printf("fe_idx= %d, str= '%s' at %s %d\n", fe_idx, str.c_str(), __FILE__, __LINE__);
				flnm_evt_vec[file_tag_idx][fe_idx].evt_tbl_idx = (uint32_t)grp_list[g];
				int sz_max = chart_defaults.drop_event_if_samples_exceed;
				if (sz_max > 0 && prf_obj[k].events[j].evt_count > sz_max) {
					fprintf(stderr, "skipping event %s due to event count (%d) exceeding max allowed %d at %s %d\n",
						evt_nm.c_str(), prf_obj[k].events[j].evt_count, sz_max, __FILE__, __LINE__);
				} else {
					do_json(UINT32_M1, evt_nm, chart_file, json_evt_chrt_str, event_table[grp_list[g]], 
						prf_obj[k].features_cpuid, options.verbose);
				}
				if (get_signal() == 1) {
					fprintf(stderr, "got control-c or 'quit' command from browser. Bye at %s %d\n", __FILE__, __LINE__);
					exit(1);
				}
			}
			double tt1a = dclock();
			tmc2 += tt1a - tt0a;
			for (uint32_t j=0; j < prf_obj[k].events.size(); j++) {
				uint32_t file_tag_idx = prf_obj[k].file_tag_idx;
				if (file_tag_idx == UINT32_M1) {
					printf("ummmm. ck yer code dude... file_tag_idx= %d at %s %d\n", file_tag_idx, __FILE__, __LINE__);
					exit(1);
				}
				evt_nm = prf_obj[k].events[j].event_name_w_area;
				std::string str = prf_obj[k].filename_bin + " " + prf_obj[k].filename_text + " " + evt_nm;
				int fe_idx = flnm_evt_hash[file_tag_idx][str] - 1;
				if (options.verbose > 0) {
					printf("bef fill_data_table: evt[%d].event_name= %s evt= %s, at %s %d\n",
							j, prf_obj[k].events[j].event_name.c_str(), evt_nm.c_str(), __FILE__, __LINE__);
				}
				for (uint32_t i=0; i < event_table[grp_list[g]].size(); i++) {
					if (event_table[grp_list[g]][i].event_name == evt_nm) {
						event_table[grp_list[g]][i].event_name_w_area = evt_nm;
						event_table[grp_list[g]][i].file_tag_idx = prf_obj[k].file_tag_idx;
						flnm_evt_vec[file_tag_idx][fe_idx].evt_tbl_idx = (uint32_t)grp_list[g];
						flnm_evt_vec[file_tag_idx][fe_idx].evt_tbl_evt_idx = i;
						event_table[grp_list[g]][i].event_idx_in_file = j;
						event_table[grp_list[g]][i].prf_obj_idx = k;
						double tm0= 0.0, tm1=0.0;
						tm0 = dclock();
						if (verbose) {
							fprintf(stdout, "file_grp= %d, match prf_feat[%d] and event_table[%d][%d] %s\n",
								k, j, grp_list[g], i, event_table[grp_list[g]][i].event_name.c_str());
						}
						int added_evts = fill_data_table(j, i, k, prf_obj[k], event_table[grp_list[g]][i], file_list[file_list_1st[k]], verbose);
						if (verbose) {
							printf("after fill_data_table: evt= %s, event_table[%d][%d].data.vals.size()= %d at %s %d\n",
								event_table[grp_list[g]][i].event_name.c_str(),
								grp_list[g], i, (int)event_table[grp_list[g]][i].data.vals.size(), __FILE__, __LINE__);
						}
						tm1 = dclock();
						tmc += tm1-tm0;
						if (verbose) {
							//std::string ch_ttl = file_list[file_list_1st[k]].file_tag + " " + event_table[grp_list[g]][i].charts[0].title;
							std::string ch_ttl = event_table[grp_list[g]][i].charts[0].title;
							fprintf(stderr, "fill_data_table: j= %d, i= %d, k= %d, grp_lst= %d, added= %d, tm= %.3f tot= %.3f evt= %s, ttl0= %s at %s %d\n", 
								j, i, k, grp_list[g], added_evts, tm1-tm0, tmc, evt_nm.c_str(), ch_ttl.c_str(), __FILE__, __LINE__);
						}
					}
				}
				if (get_signal() == 1) {
					fprintf(stderr, "got control-c or 'quit' command from browser. Bye at %s %d\n", __FILE__, __LINE__);
					exit(1);
				}
			}
		}
	}
	fflush(NULL);
	fprintf(stderr, "fill_data_table: after, loop part1= %.3f, tm_elap= %.3f at %s %d\n", tmc2, dclock()-tm_beg, __FILE__, __LINE__);
	for (uint32_t g=0; g < grp_list.size(); g++) {
		for (uint32_t k=0; k < file_list.size(); k++) {
			if (file_list[k].grp != grp_list[g]) {
				continue;
			}
			std::string evt_nm;
			for (uint32_t j=0; j < prf_obj[k].events.size(); j++) {
				evt_nm = prf_obj[k].events[j].event_name_w_area;
				for (uint32_t i=0; i < event_table[grp_list[g]].size(); i++) {
					if (event_table[grp_list[g]][i].event_name == evt_nm) {
						if (verbose)
							printf("ck after fill_data_table: evt= %s, event_table[%d][%d].data.vals.size()= %d at %s %d\n",
							event_table[grp_list[g]][i].event_name.c_str(),
							grp_list[g], i, (int)event_table[grp_list[g]][i].data.vals.size(), __FILE__, __LINE__);
					}
				}
			}
		}
	}


	fprintf(stderr, "before report_chart_data: elap_tm= %.3f at %s %d\n",
			dclock() - tm_beg, __FILE__, __LINE__);
	for (uint32_t g=0; g < grp_list.size(); g++) {
		for (uint32_t i=0; i < event_table[grp_list[g]].size(); i++) {
			printf("before build_chart_data:: evt= %s, event_table[%d][%d].data.vals.size()= %d at %s %d\n",
				event_table[grp_list[g]][i].event_name.c_str(),
				grp_list[g], i, (int)event_table[grp_list[g]][i].data.vals.size(), __FILE__, __LINE__);
			for (uint32_t j=0; j < event_table[grp_list[g]][i].charts.size(); j++) {
				if (!event_table[grp_list[g]][i].charts[j].use_chart) {
					continue;
				}
				if (get_signal() == 1) {
					fprintf(stderr, "got control-c or 'quit' command from browser. Bye at %s %d\n", __FILE__, __LINE__);
					exit(1);
				}
				int rc = build_chart_data(i, j, event_table[grp_list[g]][i], verbose);
				if (rc == 0) {
					report_chart_data(i, j, event_table[grp_list[g]], grp_list[g], verbose);
				}
			}
		}
	}
	fprintf(stderr, "after  report_chart_data, elap_tm= %.3f at %s %d\n",
			dclock() - tm_beg, __FILE__, __LINE__);

	{
		std::unordered_map<std::string, uint32_t> hsh_str_chrt_already_done_for_file_tag;
		std::vector <std::string> vec_str_chrt_already_done_for_file_tag;
		for (uint32_t g=0; g < grp_list.size(); g++) {
			for (uint32_t k=0; k < file_list.size(); k++) {
				if (file_list[k].grp != grp_list[g]) {
					continue;
				}
				for (uint32_t i=0; i < event_table[grp_list[g]].size(); i++) {
					for (uint32_t j=0; j < event_table[grp_list[g]][i].charts.size(); j++) {
						int myg = grp_list[g];
						if (ck_events_okay_for_this_chart(myg, i, j, event_table[myg]) == 0) {
							continue;
						}
						// multiplexed events can appear more than once in the event list.
						// This check avoids redoing the chart.
						std::string file_tag_and_title = file_list[k].file_tag + " " + event_table[grp_list[g]][i].charts[j].title;
						bool already_did_this_chart_for_this_file_tag = false;
						if (hsh_str_chrt_already_done_for_file_tag[file_tag_and_title] == 0) {
							hash_string(hsh_str_chrt_already_done_for_file_tag,
									vec_str_chrt_already_done_for_file_tag, 
									file_tag_and_title);
						} else {
							already_did_this_chart_for_this_file_tag = true;
						}
						if (!event_table[grp_list[g]][i].charts[j].use_chart ||
							already_did_this_chart_for_this_file_tag) {
							continue;
						}
						// this resets everything. We probably don't want to reset everything for the case of multiple files in the same group.
						if (get_signal() == 1) {
							fprintf(stderr, "got control-c or 'quit' command from browser. Bye at %s %d\n", __FILE__, __LINE__);
							exit(1);
						}
						chart_lines_reset();
						double tt0=0.0, tt1=0.0, tt2=0.0;
						std::string this_chart_json = "";
						double js_sz = 0.0;
						if (verbose)
							printf("using event_table[%d][%d]= %s at %s %d\n",
								grp_list[g], j, event_table[grp_list[g]][i].event_name.c_str(), __FILE__, __LINE__);
						tt0 = dclock();
						if (prf_obj[k].file_type == FILE_TYP_LUA &&
							event_table[grp_list[g]][i].charts[j].chart_tag == "PHASE_CHART" && phase_vec.size() > 0) {
							ck_phase_update_event_table(file_list, k, grp_list[g], i, j, event_table[grp_list[g]][i], verbose);
						}
						int rc = build_chart_lines(i, j, prf_obj[k], event_table[grp_list[g]], verbose);
#if 1
						if (event_table[grp_list[g]][i].charts[j].chart_tag == "PCT_BUSY_BY_SYSTEM") {
							printf("got pct_busy_by_system chart at %s %d\n", __FILE__, __LINE__);
							ck_phase_single_multi(file_list, k, grp_list[g], i, j, event_table[grp_list[g]], verbose);
						}
#endif
						tt1 = dclock();
						if (rc == 0) {
							build_shapes_json(file_list[k].file_tag, grp_list[g], i, j, event_table[grp_list[g]], this_chart_json, verbose);
							chrts_json.push_back(this_chart_json);
							if (event_table[grp_list[g]][i].charts[j].chart_tag == "PHASE_CHART") {
								printf("for chart_tag= %s, json_str= %s at %s %d\n", 
									event_table[grp_list[g]][i].charts[j].chart_tag.c_str(), this_chart_json.c_str(), __FILE__, __LINE__);
							}
							js_sz = this_chart_json.size();
							//printf("this_chart_json: %s at %s %d\n", this_chart_json.c_str(), __FILE__, __LINE__);
						}
						tt2 = dclock();
						fprintf(stderr, "tm build_chart_lines(): %.3f, build_shapes_json: %.3f elap= %.3f str_sz= %.3f MBs, title= %s at %s %d\n",
								tt1-tt0, tt2-tt1, tt2-tm_beg, 1.0e-6 * js_sz,
								event_table[grp_list[g]][i].charts[j].title.c_str(),
								__FILE__, __LINE__);
					}
				}
			}
		}
	}
	if (verbose) {
		printf("ch_lines.range.subcat.size()= %d\n", (int)ch_lines.range.subcat_rng.size());
		for (uint32_t i=0; i < ch_lines.range.subcat_rng.size(); i++) {
			printf("ch_lines.range.subcat[%d].size()= %d\n", (int)i, (int)ch_lines.range.subcat_rng[i].size());
		}
	}
	std::string cats = "{ \"categories\":[";
	for (uint32_t i=0; i < chart_categories_vec.size(); i++) {
		int priority;
		priority = 0;
		for (uint32_t j=0; j < chart_category.size(); j++) {
			if (chart_categories_vec[i] == chart_category[j].name) {
				priority = chart_category[j].priority;
				break;
			}
		}
		cats += (i > 0 ? "," : "");
		cats += "{\"name\":\"" + chart_categories_vec[i] + "\", \"priority\":"+std::to_string(priority) + "}";
	}
	cats += "]";
	printf("categories str= %s at %s %d\n", cats.c_str(), __FILE__, __LINE__);
	cats += ", \"pixels_high_default\":" + std::to_string(chart_defaults.pixels_high_default);
	cats += ", \"flamegraph_by_comm_pid_tid_default\":\"" + chart_defaults.flamegraph_by_comm_pid_tid_default + "\"";
	cats += ", \"do_flamegraphs\":" + std::to_string(chart_defaults.do_flamegraphs);
	std::string phase;
	if (phase_vec.size() > 0) {
		uint32_t got_zoom_to = UINT32_M1, got_zoom_end = UINT32_M1;
		for (uint32_t i=0; i < phase_vec.size(); i++) {
			for (uint32_t j=0; j < options.phase.size(); j++) {
				if (j == phase_vec[i].file_tag_idx) {
					if (got_zoom_to == UINT32_M1 && phase_vec[i].text.find(options.phase[j]) != std::string::npos) {
						phase_vec[i].zoom_to = 1;
						got_zoom_to = i;
						printf("Got match on options.phase '%s' zoom_beg= %d on phase text '%s' at %s %d\n",
								options.phase[j].c_str(), phase_vec[i].zoom_to, phase_vec[i].text.c_str(), __FILE__, __LINE__);
						break;
					}
				}
			}
		}
		for (int32_t i=phase_vec.size()-1; i >= 0; i--) {
			for (int32_t j=(int32_t)(options.phase_end.size())-1; j >= 0; j--) {
				if (j == phase_vec[i].file_tag_idx) {
					if (got_zoom_end == UINT32_M1 && phase_vec[i].text.find(options.phase_end[j]) != std::string::npos) {
						phase_vec[i].zoom_end = 1;
						got_zoom_end = i;
						printf("Got match on options.phase_end '%s' zoom_end= %d on phase text '%s' at %s %d\n",
								options.phase_end[j].c_str(), phase_vec[i].zoom_end, phase_vec[i].text.c_str(), __FILE__, __LINE__);
						break;
					}
				}
			}
		}
		if (got_zoom_to == UINT32_M1 && options.phase.size() > 0) {
				fprintf(stderr, "missed phase to zoom to. looked for options.phase[0]= '%s'. problem at %s %d\n",
						options.phase[0].c_str(), __FILE__, __LINE__);
				printf("didn't find string '%s' in phases: at %s %d\n", options.phase[0].c_str(), __FILE__, __LINE__);
				for (uint32_t j=0; j < phase_vec.size(); j++) {
					printf("phase[%d]= '%s'\n", j, phase_vec[j].text.c_str());
				}
				printf("bye at %s %d\n", __FILE__, __LINE__);
				exit(1);
		}
		if (got_zoom_end == UINT32_M1 && options.phase_end.size() > 0) {
				fprintf(stderr, "missed phase_end for zoom end. looked for options.phase_end[0]= '%s'. problem at %s %d\n",
						options.phase_end[0].c_str(), __FILE__, __LINE__);
				printf("didn't find string '%s' in phases: at %s %d\n", options.phase_end[0].c_str(), __FILE__, __LINE__);
				for (uint32_t j=0; j < phase_vec.size(); j++) {
					printf("phase[%d]= '%s'\n", j, phase_vec[j].text.c_str());
				}
				printf("bye at %s %d\n", __FILE__, __LINE__);
				exit(1);
		}
		if (got_zoom_to != UINT32_M1 && got_zoom_end != UINT32_M1 && got_zoom_to > got_zoom_end) {
				fprintf(stderr, "It seems that the cmdline --phase0 beg_time '%s' is > --phase1 end_end '%s'. problem at %s %d\n",
						options.phase[0].c_str(), options.phase_end[0].c_str(), __FILE__, __LINE__);
				printf("begin phase0 indx= %d and end phase1 indx= %d. phases: at %s %d\n", got_zoom_to, got_zoom_end, __FILE__, __LINE__);
				for (uint32_t j=0; j < phase_vec.size(); j++) {
					printf("phase[%d]= '%s'\n", j, phase_vec[j].text.c_str());
				}
				printf("bye at %s %d\n", __FILE__, __LINE__);
				exit(1);
		}
		if (got_zoom_to == UINT32_M1 && got_zoom_end != UINT32_M1) {
				fprintf(stderr, "You entered --phase1 (the end phase) but didn't enter --phase0 (the begin phase). problem at %s %d\n",
						__FILE__, __LINE__);
				printf("end phase indx= %d. phases: at %s %d\n", got_zoom_end, __FILE__, __LINE__);
				for (uint32_t j=0; j < phase_vec.size(); j++) {
					printf("phase[%d]= '%s'\n", j, phase_vec[j].text.c_str());
				}
				printf("bye at %s %d\n", __FILE__, __LINE__);
				exit(1);
		}
		phase += ", \"phase\":[";
		int phs_lines_done = 0;
		for (uint32_t i=0; i < phase_vec.size(); i++) {
			if (phase_vec[i].dura <= 0.0) {
				printf("skip phase[%d] with dura= %f at %s %d\n", i, phase_vec[i].dura, __FILE__, __LINE__);
				continue;
			}
			if (phs_lines_done > 0) {
				phase += ",";
			}
			phs_lines_done++;
			phase += "{\"ts_abs\":"+std::to_string(phase_vec[i].ts_abs)+
				", \"dura\":"+ std::to_string(phase_vec[i].dura)+
				", \"file_tag_idx\":"+std::to_string(phase_vec[i].file_tag_idx)+
				", \"file_tag\":\""+file_list[phase_vec[i].file_tag_idx].file_tag+"\""+
				", \"zoom_to\":"+std::to_string(phase_vec[i].zoom_to)+
				", \"zoom_end\":"+std::to_string(phase_vec[i].zoom_end)+
				", \"text\":\""+phase_vec[i].text+"\"}";
			printf("phase[%d].zoom_to= %d, zoom_end= %d phs_line= %d, dura= %.5f, end= %.5f str= %s at %s %d\n",
					i, phase_vec[i].zoom_to, phase_vec[i].zoom_end, phs_lines_done, phase_vec[i].dura,
					phase_vec[i].ts_abs, phase_vec[i].text.c_str(), __FILE__, __LINE__);
		}
		phase += "]";
		if (options.ph_step_int.size() > 0) {
			if (got_zoom_to == UINT32_M1) {
				fprintf(stderr, "missed phase to zoom to so we can't do --ph_step_int %s at %s %d\n",
						options.ph_step_int[0].c_str(), __FILE__, __LINE__);
				exit(1);
			} else {
				phase += ", \"ph_step_int\":[" + options.ph_step_int[0] + "]";
			}
		}
		if (options.by_phase.size() > 0) {
			if (got_zoom_to == UINT32_M1) {
				fprintf(stderr, "missed phase to zoom to so we can't do --by_phase option at %s %d\n",
						__FILE__, __LINE__);
				exit(1);
			} else {
				phase += ", \"by_phase\":[\"" + options.by_phase[0] + "\"]";
			}
		}
		if (options.skip_phases_with_string.size() > 0) {
			phase += ", \"skip_phases_with_string\":[\"" + options.skip_phases_with_string[0] + "\"]";
		}
		if (options.ph_image.size() > 0) {
			if (got_zoom_to == UINT32_M1) {
				fprintf(stderr, "missed phase to zoom to so we can't do --ph_image %s at %s %d\n",
						options.ph_image[0].c_str(), __FILE__, __LINE__);
				exit(1);
			} else {
				phase += ", \"ph_image\":[1]";
			}
			if (options.img_wxh_pxls.size() > 0) {
				phase += ", \"img_wxh_pxls\":[\""+options.img_wxh_pxls[0] + "\"]";
			}
		}
		cats += phase;
		printf("phase= '%s' at %s %d\n", phase.c_str(), __FILE__, __LINE__);
	}
	cats += "}";
	chrts_cats = cats;
	//chrts_json += "]" + cats + "}";
	printf("chrts_cats=\n%s\n at %s %d\n", chrts_cats.c_str(), __FILE__, __LINE__);
	if (options.show_json > 1) {
		for (uint32_t i=0; i < chrts_json.size(); i++) {
			printf("chrts_json[%d]=\n%s\n at %s %d\n", i, chrts_json[i].c_str(), __FILE__, __LINE__);
		}
	}
	fflush(NULL);
	double tm_end = dclock();
	fprintf(stderr, "after reading files and before sending data: elapsed= %f secs at %s %d\n", tm_end - tm_beg, __FILE__, __LINE__);
	uint64_t callstack_vec_len=0;
	std::string bin_str;
	bin_map = "{\"str_pool\": [{ \"pool\":0, \"strs\":[ ";
	for (uint32_t i=0; i < callstack_vec.size(); i++) {
		uint32_t slen = (uint32_t)callstack_vec[i].size();
		callstack_vec_len += slen;
		if (i > 0) {
			bin_map += ",";
		}
		bin_map += "\""+callstack_vec[i] + "\"";
	}
	bin_map += "]}]}";
	if (options.show_json > 1) {
		printf("bin_map=\n%s\n", bin_map.c_str());
	}
	if (options.cpu_diagram.size() > 0) {
		cpu_diag_str = "cpu_diagram=";
		std::ifstream file2;
		std::string svg_file = options.cpu_diagram;
		std::size_t pos = 0;
		pos = svg_file.find_last_of(".");
		if (pos == std::string::npos) {
			printf("messed up fopen of flnm= %s. didn't find '.' in flnm. bye at %s %d\n", options.cpu_diagram.c_str(), __FILE__, __LINE__);
			exit(1);
		}
		std::string sfx = svg_file.substr(pos+1, svg_file.size());
		printf("svg_file sfx= %s at %s %d\n", sfx.c_str(), __FILE__, __LINE__);
		if (sfx != "svg") {
			printf("svg_file sfx != 'svg', got %s, changing it to svg at %s %d\n", sfx.c_str(), __FILE__, __LINE__);
			svg_file = svg_file.substr(0, pos) + ".svg";
		}
		file2.open (svg_file.c_str(), std::ios::in);
		if (!file2.is_open()) {
			printf("messed up fopen of flnm= %s at %s %d\n", svg_file.c_str(), __FILE__, __LINE__);
			exit(1);
		}
		std::string line2;
		while(!file2.eof()){
			std::getline (file2, line2);
			cpu_diag_str += line2 + "\n";
		}
		file2.close();
		if (options.verbose > 0) {
			printf("cpu_diag= '%s' at %s %d\n", cpu_diag_str.c_str(), __FILE__, __LINE__);
		}
		std::string flds_file = options.cpu_diagram;
		pos = flds_file.find_last_of(".");
		if (pos == std::string::npos) {
			printf("messed up fopen of flnm= %s at %s %d\n", options.cpu_diagram.c_str(), __FILE__, __LINE__);
			exit(1);
		}
		flds_file = flds_file.substr(0, pos) + ".flds";
		cpu_diag_flds_filenm = flds_file;
		read_cpu_diag_flds_file(cpu_diag_flds_filenm);
	}
	fflush(NULL);
	fprintf(stderr, "callstack_vec num_strings= %d, sum of strings len= %d tm_elap= %.3f\n",
			(int)callstack_vec.size(), (int)callstack_vec_len, dclock()-tm_beg);
	fprintf(stderr, "charts_json.size()= %d, sz= %f MBs at %s %d\n", (int)chrts_json.size(), (double)(1.0e-6*(double)chrts_json.size()), __FILE__, __LINE__);
	fprintf(stderr, "str_pool.size()= %d, sz= %f MBs at %s %d\n", (int)bin_map.size(), (double)(1.0e-6*(double)bin_map.size()), __FILE__, __LINE__);
	printf("time to read files and send it= %f secs at %s %d\n", tm_end - tm_beg, __FILE__, __LINE__);
	if (options.replay_filename.size() > 0) {
		std::ofstream file;
		//long pos = 0;
		file.open (options.replay_filename.c_str(), std::ios::out);
		if (!file.is_open()) {
			printf("messed up fopen of flnm= %s at %s %d\n", options.replay_filename.c_str(), __FILE__, __LINE__);
			exit(1);
		}
		file << bin_map << std::endl;
		file << "," << std::endl;
		file << chrts_cats << std::endl;
		for (uint32_t i=0; i < chrts_json.size(); i++) {
			file << chrts_json[i] << std::endl;
		}
		file.close();
		printf("wrote str_pool and chrts_json to file: %s at %s %d\n", options.replay_filename.c_str(), __FILE__, __LINE__);
	}

	}


	fprintf(stderr, "before ck_json: tm_elap= %.3f at %s %d\n", dclock()-tm_beg, __FILE__, __LINE__);
	nlohmann::json jobj;
	bool ck = ck_json(bin_map, "check for valid json in str_pool", false, jobj, __FILE__, __LINE__, options.verbose);
	if (!ck) {
		fprintf(stderr, "bye at %s %d\n", __FILE__, __LINE__);
		exit(1);
	}
	fprintf(stderr, "after  ck_json: bin_map: tm_elap= %.3f at %s %d\n", dclock()-tm_beg, __FILE__, __LINE__);
	ck = ck_json(chrts_cats, "check for valid json in chrts_cats", false, jobj, __FILE__, __LINE__, options.verbose);
	if (!ck) {
		fprintf(stderr, "bye at %s %d\n", __FILE__, __LINE__);
		exit(1);
	}
	fprintf(stderr, "before ck_json: chrts_json: tm_elap= %.3f at %s %d\n", dclock()-tm_beg, __FILE__, __LINE__);
	for (uint32_t i=0; i < chrts_json.size(); i++) {
		std::string hdr = "check for valid json in chrts_json[" + std::to_string(i) + "]";
		ck = ck_json(chrts_json[i], hdr.c_str(), false, jobj, __FILE__, __LINE__, options.verbose);
		if (!ck) {
			fprintf(stderr, "bye at %s %d\n", __FILE__, __LINE__);
			exit(1);
		}
	}

	if (options.web_file.size() > 0) {
		create_web_file(options.verbose);
		if (options.web_file_quit) {
			fprintf(stderr, "quitting after creating web_file %s due to using web_fl_quit option. Bye at %s %d\n",
					options.web_file.c_str(), __FILE__, __LINE__);
			exit(1);
		}
	}
	fprintf(stderr, "entering oppat loop at elap_tm %.3f to wait for browser to request data. Connect browser to http://localhost:%d\n",
		dclock()-tm_beg, options.web_port);

	{
		static std::string svg_str;
		static std::string sz_str;
		static std::string str_cats;
		int i=0;
		sz_str = "chrts_json_sz= "+ std::to_string(chrts_json.size());
		str_cats = "chrt_cats= " + chrts_cats;
		while(thrd_status == THRD_RUNNING && get_signal() == 0) {
			if (!q_from_clnt_to_srvr.is_empty()) {
				std::string msg = q_from_clnt_to_srvr.pop();
				if (msg.size() < 100) {
					printf("msg[%d] from clnt= %s at %s %d\n", i, msg.c_str(), __FILE__, __LINE__);
				} else {
					printf("msg[%d] from clnt= %s at %s %d\n", i, msg.substr(0, 100).c_str(), __FILE__, __LINE__);
				}
				i++;
				uint32_t msg_len = msg.size();
				if (msg == "Ready") {
					double tm_bef = dclock();
					uint64_t tot_sz = 0, tot_ch_sz=0;
					q_from_srvr_to_clnt.push("re_init");
					if (cpu_diag_str.size() > 0) {
						read_cpu_diag_flds_file(cpu_diag_flds_filenm);
						q_from_srvr_to_clnt.push(cpu_diag_flds);
						q_from_srvr_to_clnt.push(cpu_diag_str);
					}
					tot_sz += bin_map.size();
					tot_sz += str_cats.size();
					tot_sz += sz_str.size();
					q_from_srvr_to_clnt.push(bin_map);
					q_from_srvr_to_clnt.push(str_cats);
					q_from_srvr_to_clnt.push(sz_str);
					for (uint32_t j=0; j < chrts_json.size(); j++) {
						q_from_srvr_to_clnt.push(chrts_json[j]);
						tot_ch_sz += chrts_json[j].size();
					}
					double tm_aft = dclock();
					double ftot_sz = tot_sz;
					double ftot_ch_sz = tot_ch_sz;
					fprintf(stderr, "q_from_srvr_to_clnt.push(chrts_json) size= %d txt_sz= %d non_ch_MBs= %.3f, ch_MBs= %.3f push_tm= %f at %s %d\n",
						(int)chrts_json.size(), (int)callstack_sz, 1.0e-6*ftot_sz, 1.0e-6*ftot_ch_sz, tm_aft-tm_bef, __FILE__, __LINE__);
				} else if ((msg_len > 6 &&  msg.substr(0, 6) == "image,") ||
							(msg_len > 6 &&  msg.substr(0, 6) == "imagd,")) {
					fprintf(stderr, "got image msg %s %d", __FILE__, __LINE__);
					bool image_dash = false;
					if (msg.substr(0, 6) == "imagd,") {
						image_dash = true;
					}
					std::string img_str = msg.substr(6, msg.size());
					size_t pos = img_str.find("lp=");
					pos = img_str.find("imagedata:");
					if (pos != std::string::npos) {
						fprintf(stderr, "image %s\n", img_str.substr(0, pos).c_str());
						std::string hdr0 = img_str.substr(0, pos);
						size_t pos1 = hdr0.find("lp=");
						int lp = atoi(hdr0.substr(pos1+3,hdr0.size()).c_str());
						char str[64];
						if (lp == -2) {
							sprintf(str, "_base");
						} else {
							sprintf(str, "%.5d", lp);
						}
						std::string flnm_pfx = "image_";
						if (options.ph_image.size() > 0 && options.ph_image[0].size() > 0) {
							flnm_pfx = options.ph_image[0];
						}
						if (image_dash) {
							flnm_pfx += "_dash";
						}
						std::string flnm = flnm_pfx + std::string(str) + ".png";
						std::string lkfor = ";base64,";
						size_t pos2 = img_str.find(lkfor);
						if (pos2 == std::string::npos) {
							fprintf(stderr, "missed str= '%s' in image data file %s. bye at %s %d\n",
									lkfor.c_str(), flnm.c_str(), __FILE__, __LINE__);
						}
						std::string data = img_str.substr(pos2+lkfor.size(), img_str.size());

						int ln = Base64decode_len(data.c_str());
						char *dst = (char *)malloc(ln+1);
						int rc = Base64decode(dst, data.c_str());
						fprintf(stderr, "lp= %d at %s %d\n", lp, __FILE__, __LINE__);
						{
							std::ofstream file;
							//long pos = 0;
							file.open (flnm.c_str(), std::ios::out | std::ios::binary);
							if (!file.is_open()) {
								fprintf(stderr, "messed up fopen of flnm= %s at %s %d\n", flnm.c_str(), __FILE__, __LINE__);
								exit(1);
							}
							//file << img_str.substr(pos+10, img_str.size());
							file.write(dst,ln);
							file.close();
							fprintf(stderr, "wrote flnm= %s, tm_elap= %f at %s %d\n",
									flnm.c_str(), dclock()-tm_beg, __FILE__, __LINE__);
						}
					}
					fflush(stdout);
					//ck_json(svg_str, "check parse_svg json str", __FILE__, __LINE__, options.verbose);
				} else if (msg_len > 11 &&  msg.substr(0, 11) == "json_table=") {
					fprintf(stderr, "got json_table msg %s %d", __FILE__, __LINE__);
					std::ofstream file;
					std::string flnm = "json_table.json";
					file.open (flnm.c_str(), std::ios::out);
					if (!file.is_open()) {
						fprintf(stderr, "messed up fopen of flnm= %s at %s %d\n", flnm.c_str(), __FILE__, __LINE__);
						exit(1);
					}
					file << msg.substr(11, msg.size()) << std::endl;
					file.close();
					fprintf(stderr, "wrote flnm= %s at %s %d\n", flnm.c_str(), __FILE__, __LINE__);
					fflush(stdout);
					int xls_rc = json_2_xls(msg.substr(11, msg.size()), "pat", "json_table.json data", __FILE__, __LINE__, options.verbose);
					std::string rc_str = (xls_rc == 0 ? "no errors" : "got errors");
					fprintf(stderr, "finished json_2_xls rc= %d (%s) at %s %d\n",
							xls_rc, rc_str.c_str(),  __FILE__, __LINE__);
					fflush(stdout);
					
					//ck_json(svg_str, "check parse_svg json str", __FILE__, __LINE__, options.verbose);
				} else if (msg_len >= 9 &&  msg.substr(0, 9) == "parse_svg") {
					fprintf(stderr, "got svg msg %s %d\n", __FILE__, __LINE__);
					svg_str = msg.substr(10, msg.size());
					if (verbose) {
						fprintf(stdout, "%s\n", svg_str.c_str());
						fflush(stdout);
					}
					nlohmann::json jobj;
					ck_json(svg_str, "check parse_svg json str", false, jobj, __FILE__, __LINE__, options.verbose);
				}

			}
#if defined(__linux__) || defined(__APPLE__)
			usleep(10*1000);
#else
			Sleep(10);
#endif
		}
	}

	for (auto& thrds : thrds_started){
		thrds.join();
	}

	if (get_signal() == 1) {
		fprintf(stderr, "got control-c or 'quit' command from browser. Bye at %s %d\n", __FILE__, __LINE__);
	}

	printf("bye at %s %d\n", __FILE__, __LINE__);
	return 0;
}
