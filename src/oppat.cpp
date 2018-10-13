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
#include "tc_read_data.h"

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
		{"beg_tm",      required_argument,   0, 'b', "begin time (absolute) to clip"},
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

		c = mygetopt_long(argc, argv, "b:c:d:e:hm:pr:su:v",
                       long_options, &option_index);

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
			if (load_long_opt_val(long_options[option_index].name, "web_file", options.web_file, optarg) > 0) {
				break;
			}
			if (load_long_opt_val(long_options[option_index].name, "web_fl_quit", options.web_file, optarg) > 0) {
				options.web_file_quit = true;
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
		  options.tm_clip_beg_valid++;
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
		  options.tm_clip_end_valid++;
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

struct shape_str {
	double x, y;
	std::string text;
	int pid, typ, cat, subcat;
	shape_str(): pid(-1), typ(-1) {}
};

struct lines_str {
	double x[2], y[2], period;
	std::string text;
	int cpt_idx, fe_idx, pid, prf_idx, typ, cat, subcat, cpu;
	std::vector <int> callstack_str_idxs;
	lines_str(): period(0.0), cpt_idx(-1), fe_idx(-1), pid(-1), prf_idx(-1), typ(-1), cat(-1), subcat(-1), cpu(-1) {}
};

// order of SHAPE_* enum values must agree with SHAPE_* variables in main.js
enum {
	SHAPE_LINE,
	SHAPE_RECT,
};

struct min_max_str {
	double x0, x1, y0, y1, total;
	int fe_idx;
	bool initd;
	min_max_str(): x0(-1.0), x1(-1.0), y0(-1.0), y1(-1.0), total(0.0), fe_idx(-1), initd(false) {}
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
	prf_obj_str *prf_obj;
	ch_lines_str(): tm_beg_offset_due_to_clip(0.0), prf_obj(0) {}
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

uint32_t hash_comm_pid_tid(std::unordered_map<std::string, int> &hsh_str, std::vector <comm_pid_tid_str> &vec_str, std::string comm, int pid, int tid)
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
static uint32_t get_by_var_idx(std::unordered_map <double, uint32_t> hsh_dbl, double by_var_dbl, int line)
{
	uint32_t idx = hsh_dbl[by_var_dbl];
	if (idx == 0) {
		printf("got by_var_idx == 0. Shouldn't happen if we call this after build_chart_data(). called from line %d. Bye at %s %d\n", line, __FILE__, __LINE__);
		exit(1);
	}
	return idx-1;
}

static void run_actions(double &val, std::vector <action_str> actions, bool &use_value)
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
		}
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
		if (flg & FLD_TYP_DURATION_PREV_TS_SAME_BY_VAL) {
			if (prv_ts_idx != (uint32_t)-1) {
				printf("can only handle 1 TYP_DURATION_PREV_TS_SAME_BY_VAL attribute in evt_flds. fld= %s event= %s, Chart= %s. bye at %s %d\n",
					event_table.flds[j].name.c_str(),
					event_table.event_name_w_area.c_str(),
					event_table.charts[chrt].title.c_str(), __FILE__, __LINE__);
				exit(1);
			}
			prv_ts_idx = j;
		}
		if (flg & FLD_TYP_DIV_BY_INTERVAL2) {
			prv_ts_div_req_idx = j;
		}
		if (event_table.flds[j].next_for_name_idx != -1 &&
			event_table.flds[j].next_for_name_idx == var_idx) {
			next_idx = j;
		}
		if (flg & FLD_TYP_STATE_AFTER) {
			after_idx = j;
		}
		if (flg & FLD_TYP_DURATION_BEF) {
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
			idx = (int)hash_dbl( event_table.charts[chrt].by_var_hsh,
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
		if (idx >= (int)last_val_for_by_var_idx.size()) {
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
			other_idx = (int)hash_dbl( event_table.charts[chrt].by_var_hsh,
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
		last_val_for_by_var_idx[idx] = i;
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
					event_table.flds[prv_ts_div_req_idx].actions_stage2, use_value);
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
				hash_dbl( event_table.charts[chrt].by_var_hsh,
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
			if (by_var_flags & FLD_TYP_STR) {
				by_var_str = event_table[evt_idx].vec_str[(uint32_t)(by_var_val-1)];
			} else if (by_var_flags & FLD_TYP_INT) {
				by_var_str = std::to_string((int)by_var_val);
			} else if (by_var_flags & FLD_TYP_DBL) {
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
	ch_lines.prf_obj = 0;
}

void chart_lines_ck_rng(double x, double y, double ts0, int cat, int subcat, double cur_val, int fe_idx)
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
	ch_lines.range.subcat_rng[cat][subcat].total += cur_val;
	//printf("subcat[%d][%d]\n", cat, subcat);
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
			"\", \"total\":"+std::to_string(ntot)+"}";
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
	if (options.show_json > 0) {
		printf("flnm_evt: %s\n", str.c_str());
	}
	return str;
}

static uint64_t callstack_sz = 0;

void prf_mk_callstacks(prf_obj_str &prf_obj, int prf_idx,
		std::vector <int> &callstacks, int line, std::vector <std::string> prefx)
{
	std::string cs_new, cs_prv;
	for (uint32_t k=0; k < prf_obj.samples[prf_idx].callstack.size(); k++) {
		if (prf_obj.samples[prf_idx].callstack[k].mod == "[unknown]" &&
				prf_obj.samples[prf_idx].callstack[k].rtn == "[unknown]") {
			continue;
		}
		cs_new = prf_obj.samples[prf_idx].callstack[k].mod + " " + prf_obj.samples[prf_idx].callstack[k].rtn;
		if (cs_new == cs_prv) {
			continue;
		}
		cs_prv = cs_new;
		int cs_idx = (int)hash_string(callstack_hash, callstack_vec, cs_new) - 1;
		callstacks.push_back(cs_idx);
	}
	for (uint32_t k=0; k < prefx.size(); k++) {
		int cs_idx = (int)hash_string(callstack_hash, callstack_vec, prefx[k]) - 1;
		callstacks.push_back(cs_idx);
	}
	callstack_sz += callstacks.size();
}

void etw_mk_callstacks(int set_idx, prf_obj_str &prf_obj, int i,
				int stk_mod_rtn_idx, std::vector <int> &callstacks, int line,
				std::vector <std::string> prefx)
{
	uint32_t cs_beg = prf_obj.etw_evts_set[set_idx][i].cs_idx_beg;
	if (cs_beg == UINT32_M1) {
		return;
	}
	uint32_t cs_end = prf_obj.etw_evts_set[set_idx][i].cs_idx_end;
	std::string cs_prv;
	for (uint32_t k=cs_beg; k <= cs_end; k++) {
		std::string str = prf_obj.etw_data[k][stk_mod_rtn_idx];
		std::string cs_new, mod, rtn;
		size_t pos = str.find("!");
		mod = str.substr(0, pos);
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
		int cs_idx = (int)hash_string(callstack_hash, callstack_vec, cs_new) - 1;
		callstacks.push_back(cs_idx);
	}
	for (uint32_t k=0; k < prefx.size(); k++) {
		int cs_idx = (int)hash_string(callstack_hash, callstack_vec, prefx[k]) - 1;
		callstacks.push_back(cs_idx);
	}
	callstack_sz += callstacks.size();
}

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
	if (event_table[evt_idx].flds[var_idx].flags & FLD_TYP_EXCL_PID_0) {
		if (options.verbose)
			fprintf(stdout, "skip idle= true, var_idx= %d at %s %d\n", var_idx, __FILE__, __LINE__);
		skip_idle = true;
	}	
	if (event_table[evt_idx].charts[chrt].chart_tag == "PCT_BUSY_BY_CPU") {
		doing_pct_busy_by_cpu = 1;
	}
	if (event_table[evt_idx].charts[chrt].chart_type == "line") {
		chart_type = CHART_TYPE_LINE;
	}
	else if (event_table[evt_idx].charts[chrt].chart_type == "stacked") {
		chart_type = CHART_TYPE_STACKED;
	}
	//uint32_t wait_ts_idx= UINT32_M1, wait_dura_idx= UINT32_M1;
	for (uint32_t j=0; j < fsz; j++) {
		uint64_t flg = event_table[evt_idx].flds[j].flags;
		//fprintf(stderr, "fld[%d] flgs= 0x%x, name= %s at %s %d\n", j, flg, event_table[evt_idx].flds[j].name.c_str(), __FILE__, __LINE__);
		if (flg & FLD_TYP_EXCL_PID_0 && verbose) {
			fprintf(stderr, "skip idle= true, var_idx= %d j= %d at %s %d\n", var_idx, j, __FILE__, __LINE__);
		}
		if (flg & FLD_TYP_SYS_CPU) {
			var_cpu_idx = (int)j;
		}
		if (flg & FLD_TYP_COMM_PID_TID) {
			var_comm_idx = (int)j;
		}
		if (flg & FLD_TYP_PID) {
			var_pid_idx = (int)j;
		}
	}
	bool have_filter_regex = false;
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
			have_filter_regex = true;
		}
	}
	double ts0 = prf_obj.tm_beg;
	ch_lines.tm_beg_offset_due_to_clip = prf_obj.tm_beg_offset_due_to_clip;
	ch_lines.prf_obj = &prf_obj;
	if (verbose)
		printf("build_chart_lines(%d, %d): ts0= %f chart= '%s' by_var_idx= %d by_var_str.size()= %d by_var_vals.sz= %d at %s %d\n",
		evt_idx, chrt, ts0, event_table[evt_idx].charts[chrt].title.c_str(), (int)by_var_idx,
		(int)event_table[evt_idx].charts[chrt].by_var_strs.size(),
		(int)event_table[evt_idx].charts[chrt].by_var_vals.size(),
		__FILE__, __LINE__);
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
			//printf("by_var_idx= %d, subcats[%d][%d] size= %d at %s %d\n",
			//	(int)by_var_idx, i, (int)ch_lines.subcats[i].size(), (int)ch_lines.subcats[i][0].size(), __FILE__, __LINE__);
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
	double delta= 0.25, lo=0.0, hi=1.0;
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
			if (by_var_idx >= 0) {
				int by_var_val = (int)event_table[evt_idx].data.vals[i][by_var_idx];
				by_var_idx_val = get_by_var_idx(event_table[evt_idx].charts[chrt].by_var_hsh, by_var_val, __LINE__);
			}
			if (by_var_idx >= 0) {
				int by_var_val = (int)event_table[evt_idx].data.vals[i][other_by_var_idx];
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
			if ((flg & FLD_TYP_CSW_STATE) && !(flg & FLD_TYP_LAG)) {
				cur_state_idx = i;
			}
			if ((flg & FLD_TYP_CSW_REASON) && !(flg & FLD_TYP_LAG)) {
				cur_reason_idx = i;
			}
			if ((flg & FLD_TYP_CSW_STATE) && (flg & FLD_TYP_LAG)) {
				lag_state_idx = i;
			}
			if ((flg & FLD_TYP_CSW_REASON) && (flg & FLD_TYP_LAG)) {
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
#if 1
		bool line_ck_overlap;
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
			if (options.tm_clip_beg_valid == 2 && (fx1+ts0) < options.tm_clip_beg) {
				cur_idx--;
				continue;
			}
			if (options.tm_clip_end_valid == 2 && (fx0+ts0) > options.tm_clip_end) {
				cur_idx--;
				continue;
			}
			if (have_filter_regex) {
				bool skip = false;
				for (uint32_t ii=0; ii < event_table[evt_idx].charts[chrt].actions.size(); ii++) {
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
					printf("what's going on here. fx1= %f, sx0= %f, fx0= %f i= %d, cur_idx= %d. bye at %s %d\n",
							fx1, sx0, fx0, i, cur_idx, __FILE__, __LINE__);
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
					run_actions(new_val, event_table[evt_idx].charts[chrt].actions, use_value);
				}
				if (new_val < 0.0 && var_val >= 0.0 && ((x1 - x0) < 0.0 || (fx1 - sx0) < 0.0)) {
					printf("screw up here, new_val= %f, var_val= %f, x0= %f, x1= %f, dff= %f sx0= %f, fx1= %f dff= %f at %s %d\n",
						new_val, var_val, x0, x1, (x1 - x0), sx0, fx1, (fx1 - sx0), __FILE__, __LINE__);
				}
				y_val[by_var_idx_val] += new_val;
			}
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
			lines_str *ls0p = &last_by_var_lines_str[by_var_idx_val].ls0;
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
			bool use_this_line = true;
			//ls0p->cpt_idx = event_table[evt_idx].data.comm_pid_tid_idx[i];
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
								str != "R" && str != "R+") {
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
							ls0p->x[0] = x0;
							ls0p->x[1] = x1;
							y_val[by_var_idx_val] = x1-x0;
							prf_mk_callstacks(prf_obj, prf_idx2, callstacks, __LINE__, prefx);
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
			} else {
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
					etw_mk_callstacks(prf_evt_idx, prf_obj, prv, stk_mod_rtn_idx, callstacks, __LINE__, prefx);
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
						etw_mk_callstacks(prf_evt_idx, prf_obj, prv, stk_mod_rtn_idx, callstacks, __LINE__, prefx);
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

			if (chart_type != CHART_TYPE_STACKED) {
				ls0p->y[0] = y_val[by_var_idx_val];
				ls0p->y[1] = y_val[by_var_idx_val];
				if (use_this_line) {
					chart_lines_ck_rng(ls0p->x[0], ls0p->y[0], ts0, ls0p->cat, ls0p->subcat, 0.0, ls0p->fe_idx);
					chart_lines_ck_rng(ls0p->x[1], ls0p->y[1], ts0, ls0p->cat, ls0p->subcat, y_val[by_var_idx_val], ls0p->fe_idx);
					ch_lines.line.push_back(*ls0p);
				}
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
				cur_idx--;
				continue;
			}

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
				chart_lines_ck_rng(ls1.x[0], ls1.y[0], ts0, ls1.cat, ls1.subcat, 0.0, ls1.fe_idx);
				chart_lines_ck_rng(ls1.x[1], ls1.y[1], ts0, ls1.cat, ls1.subcat, yv, ls1.fe_idx);
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
		if (options.tm_clip_beg_valid == 2 && (x+dura+ts0) < options.tm_clip_beg) {
			continue;
		}
		if (options.tm_clip_end_valid == 2 && (x+ts0) > options.tm_clip_end) {
			continue;
		}
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
			        ls1.cpu    = event_table[evt_idx].data.vals[i][var_cpu_idx];
				} else {
			        ls1.cpu    = -1;
				}
			    ls1.cpt_idx    = cpt_idx;
				ls1.period  = 1.0;
			}
			ls1.period = dura;
			if (prf_obj.file_type != FILE_TYP_ETW) {
				std::string ostr;
				prf_sample_to_string(prf_idx, ostr, prf_obj);
				ls1.text = ostr;
				ls1.text += "<br>" + prf_obj.samples[prf_idx].extra_str;
				if (prf_obj.samples[prf_idx].args.size() > 0) {
					ls1.text += "<br>";
					for (uint32_t k=0; k < prf_obj.samples[prf_idx].args.size(); k++) {
						ls1.text += " " + prf_obj.samples[prf_idx].args[k];
					}
				}
				ls1.text += ", line " + std::to_string(prf_obj.samples[prf_idx].line_num);
				if (prf_obj.samples[prf_idx].callstack.size() > 0) {
					std::vector <int> callstacks;
					std::vector <std::string> prefx;
						prf_mk_callstacks(prf_obj, prf_idx, callstacks, __LINE__, prefx);
						ls1.callstack_str_idxs = callstacks;
				}
			} else if (stk_idx != UINT32_M1) {
				uint32_t prf_evt_idx = event_table[evt_idx].event_idx_in_file;
				int jj = prv_nxt[i].prv;
				if(jj > -1 && prf_obj.etw_evts_set[prf_evt_idx][jj].cs_idx_beg != UINT32_M1) {
					std::vector <int> callstacks;
					std::vector <std::string> prefx;
					etw_mk_callstacks(prf_evt_idx, prf_obj, jj, stk_mod_rtn_idx, callstacks, __LINE__, prefx);
					ls1.callstack_str_idxs = callstacks;
				}
				ls1.text = "line " + std::to_string(prf_obj.etw_evts_set[prf_evt_idx][i].txt_idx);
			}
			ls1.x[1] = ls1.x[0];
			ls1.y[1] = ls1.y[0]+delta;
			ls1.cat = (int)by_var_idx_val;
			ls1.subcat = 1;
			ls1.prf_idx = prf_idx;
			ls1.typ = SHAPE_LINE;
			chart_lines_ck_rng(ls1.x[0], ls1.y[0], ts0, ls1.cat, ls1.subcat, 0.0, ls1.fe_idx);
			chart_lines_ck_rng(ls1.x[1], ls1.y[1], ts0, ls1.cat, ls1.subcat, 1.0, ls1.fe_idx);
			ch_lines.line.push_back(ls1);
		}
		if (chart_type == CHART_TYPE_BLOCK && doing_pct_busy_by_cpu == 1 && idle_pid) {
			continue;
		}
		lines_str ls0;
		ls0.x[0] = x;
		ls0.x[1] = event_table[evt_idx].data.ts[i].ts - ts0;
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
		ls0.text = comm;
		ls0.pid  = pid_num;
		if (prf_obj.file_type != FILE_TYP_ETW) {
			ls0.fe_idx  = prf_obj.samples[prf_idx].fe_idx;
			ls0.cpt_idx = event_table[evt_idx].data.cpt_idx[0][i];
			ls0.period  = (double)prf_obj.samples[prf_idx].period;
			ls0.cpu     = (int)prf_obj.samples[prf_idx].cpu;
			ls0.text += ", line " + std::to_string(prf_obj.samples[prf_idx].line_num);
			ls0.text += "<br>" + prf_obj.samples[prf_idx].extra_str;
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
			ls0.cpt_idx = cpt_idx;
		}
		ls0.prf_idx = prf_idx;
		ls0.cat     = (int)by_var_idx_val;
		ls0.subcat  = 0;
		chart_lines_ck_rng(ls0.x[0], ls0.y[0], ts0, ls0.cat, ls0.subcat, 0.0, ls0.fe_idx);
		chart_lines_ck_rng(ls0.x[1], ls0.y[1], ts0, ls0.cat, ls0.subcat, 1.0, ls0.fe_idx);
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
			hash_dbl( event_table[evt_idx].charts[chrt].by_var_hsh,
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
		for (uint32_t i=0; i < prf_obj.samples.size(); i++) {
			if (prf_obj.samples[i].evt_idx == event_table[evt_idx].event_idx_in_file) {
				// already did these events so skip
				continue;
			}
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
			double ts = 1.0e-9 * (double)prf_obj.samples[i].ts;
			if (options.tm_clip_beg_valid == 2 && (ts+ts0) < options.tm_clip_beg) {
				continue;
			}
			if (options.tm_clip_end_valid == 2 && (ts+ts0) > options.tm_clip_end) {
				continue;
			}
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
			ls0.text = ostr;
			if (prf_obj.samples[i].args.size() > 0) {
				ls0.text += "<br>";
				for (uint32_t k=0; k < prf_obj.samples[i].args.size(); k++) {
					ls0.text += " " + prf_obj.samples[i].args[k];
				}
			}
			ls0.text += "<br>" + prf_obj.samples[i].extra_str;
			ls0.text += ", line " + std::to_string(prf_obj.samples[i].line_num);
			if (prf_obj.samples[i].callstack.size() > 0) {
				std::vector <int> callstacks;
				std::vector <std::string> prefx;
				prf_mk_callstacks(prf_obj, i, callstacks, __LINE__, prefx);
				ls0.callstack_str_idxs = callstacks;
			}
			//printf("evt_nm= %s cpu= %f at %s %d\n", event_table[evt_idx].event_name_w_area.c_str(), cpu, __FILE__, __LINE__);
			int by_var_idx_val2 = (int)get_by_var_idx(event_table[evt_idx].charts[chrt].by_var_hsh, cpu, __LINE__);
			ls0.cat = by_var_idx_val2;
			ls0.subcat = 1;
			chart_lines_ck_rng(ls0.x[0], ls0.y[0], ts0, ls0.cat, ls0.subcat, 0.0, ls0.fe_idx);
			chart_lines_ck_rng(ls0.x[1], ls0.y[1], ts0, ls0.cat, ls0.subcat, 1.0, ls0.fe_idx);
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
					etw_mk_callstacks(set_idx, prf_obj, i, stk_mod_rtn_idx, callstacks, __LINE__, prefx);
					ls0.callstack_str_idxs = callstacks;
				}
				int by_var_idx_val2 = (int)get_by_var_idx(event_table[evt_idx].charts[chrt].by_var_hsh, cpu, __LINE__);
				ls0.cat = by_var_idx_val2;
				ls0.subcat = 1;
				chart_lines_ck_rng(ls0.x[0], ls0.y[0], ts0, ls0.cat, ls0.subcat, 0.0, ls0.fe_idx);
				chart_lines_ck_rng(ls0.x[1], ls0.y[1], ts0, ls0.cat, ls0.subcat, 1.0, ls0.fe_idx);
				ch_lines.line.push_back(ls0);
			}
		}
	}
	return 0;
}

static std::unordered_map<std::string, uint32_t> chart_categories_hash;
static std::vector <std::string> chart_categories_vec;

static std::string build_shapes_json(std::string file_tag, uint32_t evt_tbl_idx, uint32_t evt_idx, uint32_t chrt, std::vector <evt_str> event_table, int verbose)
{
	int var_idx = (int)event_table[evt_idx].charts[chrt].var_idx;
	std::string json = "{ ";
	json += "\"title\": \"" + event_table[evt_idx].charts[chrt].title + "\"";
	json += ", \"x_label\": \"Time(secs)\"";
	json += ", \"y_fmt\": \""+ event_table[evt_idx].charts[chrt].y_fmt + "\"";
	json += ", \"pixels_high\": "+ std::to_string(event_table[evt_idx].charts[chrt].pixels_high);
	json += ", \"file_tag\": \"" + file_tag + "\"";
	json += ", \"chart_tag\": \"" + event_table[evt_idx].charts[chrt].chart_tag + "\"";
	if (event_table[evt_idx].charts[chrt].y_label.size() == 0) {
		json += ", \"y_label\": \"" + event_table[evt_idx].flds[var_idx].name + "\"";
	} else {
		json += ", \"y_label\": \"" + event_table[evt_idx].charts[chrt].y_label + "\"";
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
	json += ", \"ts_initial\": { \"ts\":" + do_string_with_decimals(ch_lines.range.ts0, 9) +
		", \"ts0x\":" + do_string_with_decimals(0.0, 9) +
		", \"tm_beg_offset_due_to_clip\":" + do_string_with_decimals(ch_lines.tm_beg_offset_due_to_clip, 9) + "}";
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
	json += ", \"myxs\": {";
	for (uint32_t i=0; i < by_sz; i++) {
		if (i > 0) {
			var_strs += ", ";
		}
		std::string use_str = event_table[evt_idx].charts[chrt].by_var_strs[i];
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
	for (uint32_t i=0; i < ch_lines.line.size(); i++) {
		if (i > 0) { json += ", "; }
		// order of ival array values must agree with IVAL_* variables in main.js
		json += std::string("{\"ival\":[") + std::to_string(ch_lines.line[i].typ) +
				"," + std::to_string(ch_lines.line[i].cpt_idx) +
				"," + std::to_string(ch_lines.line[i].fe_idx) +
				"," + std::to_string(ch_lines.line[i].cat) +
				"," + std::to_string(ch_lines.line[i].subcat) +
				"," + std::to_string(ch_lines.line[i].period) +
				"," + std::to_string(ch_lines.line[i].cpu) +
				"],\"pts\":[" + std::to_string(ch_lines.line[i].x[0]) +
				"," + (ch_lines.line[i].y[0] == 0.0 ? zero : std::to_string(ch_lines.line[i].y[0])) +
				"," + std::to_string(ch_lines.line[i].x[1]) +
				"," + (ch_lines.line[i].y[1] == 0.0 ? zero : std::to_string(ch_lines.line[i].y[1])) +
				"]" +
				",\"text\":\"" + ch_lines.line[i].text + "\"";
		std::string cs_txt;
		for (uint32_t j=0; j < ch_lines.line[i].callstack_str_idxs.size(); j++) {
			if (j > 0) {
				cs_txt += ",";
			}
			cs_txt += std::to_string(ch_lines.line[i].callstack_str_idxs[j]);
		}
		if (cs_txt.size() > 0) {
			cs_txt = ",\"cs_strs\":[" + cs_txt + "]";
		}
		json += cs_txt + "}";
	}
	json += "]";
	json += "," + build_proc_string(evt_idx, chrt, event_table);
	json += "," + build_flnm_evt_string(evt_tbl_idx, evt_idx, event_table, verbose);
	if (verbose)
		printf("title= %s\n",  event_table[evt_idx].charts[chrt].title.c_str());
	json += ", \"subcat_rng\":[";
	bool did_line=false;
	for (uint32_t i=0; i < ch_lines.range.subcat_rng.size(); i++) {
		for (uint32_t j=0; j < ch_lines.range.subcat_rng[i].size(); j++) {
			if (did_line) { json += ", "; }
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
			if (verbose > 0)
			printf("subcat_rng[%d][%d].x0= %f, x1= %f, y0= %f, y1= %f, fe_idx= %d, ev= %s total= %f, txt[%d]= %s, initd= %d\n", i, j,
				ch_lines.range.subcat_rng[i][j].x0,
				ch_lines.range.subcat_rng[i][j].x1,
				ch_lines.range.subcat_rng[i][j].y0,
				ch_lines.range.subcat_rng[i][j].y1,
				ch_lines.range.subcat_rng[i][j].fe_idx,
				ev.c_str(),
				ch_lines.range.subcat_rng[i][j].total,
				(int)i, legnd.c_str(), 
				ch_lines.range.subcat_rng[i][j].initd
				);
			json += "{ \"x0\":" + std::to_string(ch_lines.range.subcat_rng[i][j].x0) +
				", \"x1\":" + std::to_string(ch_lines.range.subcat_rng[i][j].x1) +
				", \"y0\":" + std::to_string(ch_lines.range.subcat_rng[i][j].y0) +
				", \"y1\":" + std::to_string(ch_lines.range.subcat_rng[i][j].y1) +
				", \"fe_idx\":" + std::to_string(ch_lines.range.subcat_rng[i][j].fe_idx) +
				", \"event\":\"" + ev + "\""+
				", \"total\":" + std::to_string(ch_lines.range.subcat_rng[i][j].total) +
				", \"cat\":" + std::to_string(i) +
				", \"subcat\":" + std::to_string(j) +
				", \"cat_text\":\"" + legnd + "\"" +
				", \"subcat_text\":\"" + sbcat + "\""+
				"}";
			did_line = true;
		}
	}
	json += "]";
	json += "}";
	// below can print lots
	if (options.show_json > 0) {
		printf("did build_chart_json(): '%s'\n", json.c_str());
	}
	
	return json;
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
	if (flg & FLD_TYP_STR) {
		uint32_t idx = hash_string(event_table.hsh_str, event_table.vec_str, str);
		dv.push_back(idx);
	} else if (flg & FLD_TYP_DBL) {
		double x = atof(str.c_str());
		dv.push_back(x);
	} else if (flg & FLD_TYP_INT) {
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

	//run_heapchk("top of fill_data_table:", __FILE__, __LINE__, 1);
	ts_0   = prf_obj.tm_beg;
	ts_end = prf_obj.tm_end;

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
				if (event_table.flds[j].lag_prev_by_var_using_fld ==
					event_table.flds[k].name) {
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
		if (flg & FLD_TYP_DURATION_BEF) {
			dura_idx = (int)j;
		}
		if (flg & FLD_TYP_BY_VAR0) {
			if (how_many_by_var_flags & 1) {
				printf("got more than 1 FLD_TYP_BY_VAR0 flags. Bye at %s %d\n", __FILE__, __LINE__);
				exit(1);
			}
			how_many_by_var_flags |= 1;
		}
		if (flg & FLD_TYP_BY_VAR1) {
			if (how_many_by_var_flags & 2) {
				printf("got more than 1 FLD_TYP_BY_VAR1 flags. Bye at %s %d\n", __FILE__, __LINE__);
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
		printf("ETW event= %s at %s %d\n", prf_obj.events[prf_idx].event_name.c_str(), __FILE__, __LINE__);
		if (prf_idx >= prf_obj.etw_evts_set.size()) {
			printf("out of bounds prf_idx(%d) >= etw_evts_set.sz(%d). Bye at %s %d\n",
				(int)prf_idx, (int)prf_obj.etw_evts_set.size(), __FILE__, __LINE__);
			exit(1);
		}
		samples_sz = prf_obj.etw_evts_set[prf_idx].size();
		for (uint32_t j=0; j < fsz; j++) {
			uint64_t flg = event_table.flds[j].flags;
			if ((flg & FLD_TYP_SYS_CPU) && event_table.flds[j].lkup.size() > 0) {
				cpu_idx = j;
			} else if ((flg & FLD_TYP_TID) && event_table.flds[j].lkup.size() > 0) {
				tid_idx = j;
			} else if ((flg & FLD_TYP_TID2) && event_table.flds[j].lkup.size() > 0) {
				tid_idx2 = j;
			} else if ((flg & FLD_TYP_ETW_COMM_PID) && event_table.flds[j].lkup.size() > 0) {
				comm_pid_idx = j;
			} else if ((flg & FLD_TYP_ETW_COMM_PID2) && event_table.flds[j].lkup.size() > 0) {
				comm_pid_idx2 = j;
			}
		}

		// lets do the column mapping
		etw_col_map.resize(fsz, -1);
		etw_col_strs.resize(fsz, "");
		//printf("fsz= %d, prf_idx= %d cols.sz= %d at %s %d\n",
		//		fsz, prf_idx, (int)prf_obj.events[prf_idx].etw_cols.size(), __FILE__, __LINE__);
		for (uint32_t j=0; j < fsz; j++) {
			uint32_t k;
			for (k=0; k < prf_obj.events[prf_idx].etw_cols.size(); k++) {
				//printf("ck   etw_col_map[%d](%s) == %d (%s) at %s %d\n", j,
				 //       event_table.flds[j].lkup.c_str(), k, prf_obj.events[prf_idx].etw_cols[k].c_str(),
				  //      __FILE__, __LINE__);
				if (event_table.flds[j].lkup == prf_obj.events[prf_idx].etw_cols[k]) {
					//printf("mtch etw_col_map[%d] = %d at %s %d\n", j, k, __FILE__, __LINE__);
					etw_col_map[j] = k;
					break;
				}
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
	for (uint32_t i=0; i < samples_sz; i++) {
		int cpt_idx, cpt_idx2, prf_evt_idx, fe_idx, cpu;
		uint32_t pid, tid, pid2, tid2;
		double ts;
		std::string comm, comm2, fl_evt;

		cpt_idx = cpt_idx2 = prf_evt_idx = fe_idx = cpu = -1;
		ts = dura = 0.0;
		pid = tid = pid2 = tid2 = UINT32_M1;
		comm  = "";
		comm2 = "";

		if (prf_obj.file_type != FILE_TYP_ETW) {
			// events in perf, trace-cmd and my lua files have common header data (comm, pid, tid, cpu, ts)
			comm = prf_obj.samples[i].comm;
			pid  = prf_obj.samples[i].pid;
			tid  = prf_obj.samples[i].tid;

			prf_evt_idx = (int)prf_obj.samples[i].evt_idx;
			if (prf_evt_idx == -1) {
				printf("messed up prf event indx number of event[%d] name='%s' at %s %d\n",
						i, prf_obj.samples[i].event.c_str(), __FILE__, __LINE__);
				exit(1);
			}
			fl_evt = flnm + prf_obj.events[prf_evt_idx].event_name_w_area;
			uint32_t file_tag_idx = prf_obj.file_tag_idx;
			fe_idx = flnm_evt_hash[file_tag_idx][fl_evt] - 1;
			if (fe_idx == -1) {
				printf("messed up hash of str='%s', evt_nm= %s, prf_evt_idx= %d, evt_nm_w_area= %s at %s %d\n",
					fl_evt.c_str(),
					prf_obj.samples[i].event.c_str(),
					prf_evt_idx,
					prf_obj.events[prf_evt_idx].event_name_w_area.c_str(),
					__FILE__, __LINE__);
				exit(1);
			}
			prf_obj.samples[i].fe_idx = fe_idx;
			flnm_evt_vec[file_tag_idx][fe_idx].total++;
			if (prf_obj.samples[i].callstack.size() > 0) {
				flnm_evt_vec[file_tag_idx][fe_idx].has_callstacks = true;
			}
			if (prf_obj.samples[i].evt_idx != prf_idx) {
				continue;
			}
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
			uint32_t file_tag_idx = prf_obj.file_tag_idx;
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
			//abcd
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
		uint32_t file_tag_idx = prf_obj.file_tag_idx;
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
		for (uint32_t j=0; j < fsz; j++) {
			did_actions_already[j] = false;
			uint64_t flg = event_table.flds[j].flags;
			if (flg & FLD_TYP_TM_CHG_BY_CPU) {
				double ts_delta = ts - ts_cpu[cpu];
				if (flg & FLD_TYP_DURATION_BEF) {
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
					uint32_t file_tag_idx = prf_obj.file_tag_idx;
					uint32_t tcpt_idx = (int)hash_comm_pid_tid(comm_pid_tid_hash[file_tag_idx], comm_pid_tid_vec[file_tag_idx],
										cmm, prf_obj.comm[pid_i].pid, prf_obj.comm[pid_i].tid) - 1;
					if (flg & FLD_TYP_BY_VAR0) {
						cpt_idx = tcpt_idx;
					}
					if (flg & FLD_TYP_BY_VAR1) {
						cpt_idx2 = tcpt_idx;
					}
					dv.push_back(idx);
					continue;
				}
			}
			if (flg & FLD_TYP_NEW_VAL) {
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
				if ((flg & FLD_TYP_DBL) ||(flg & FLD_TYP_INT)) {
					double val = atof(prf_obj.samples[i].new_vals[got_it].c_str());
					if (flg & FLD_TYP_DURATION_BEF) {
						//printf("NEW_VAL duration val= %f, ts= %f at %s %d\n", val, ts, __FILE__, __LINE__);
						dura_idx = j;
						dura = val;
					}
					dv.push_back(val);
				}
				if (flg & FLD_TYP_STR) {
					std::string str = prf_obj.samples[i].new_vals[got_it];
					//printf("NEW_VAL str= %s at %s %d\n", str.c_str(), __FILE__, __LINE__);
					uint32_t idx = hash_string(event_table.hsh_str, event_table.vec_str, str);
					dv.push_back(idx);
					continue;
				}
				continue;
			}
			if (flg & FLD_TYP_DIV_BY_INTERVAL) {
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

				if ((flg & FLD_TYP_TM_CHG_BY_CPU) && (flg & FLD_TYP_DURATION_BEF)) {
					continue; // already pushed it
				}
				if (flg & FLD_TYP_TIMESTAMP || event_table.flds[j].lkup == "_TIMESTAMP_" ||
				    event_table.flds[j].lkup == "TimeStamp") {
					dv.push_back(ts);
					continue;
				}
				str = "";
				use_u64 = false;
				uint64_t hex_u64_val=0;
				if (k != UINT32_M1) {
					str = prf_obj.etw_data[data_idx][k];
					if (flg & FLD_TYP_HEX_IN) {
						hex_u64_val = strtoll(str.c_str(), NULL, 16);
#if 0
						printf("hex str= '%s', val= %" PRId64 ", hx= %" PRIx64 " at %s %d\n",
								str.c_str(), hex_u64_val, hex_u64_val, __FILE__, __LINE__);
#endif
						use_u64 = true;
					}
				}
#if 0
				if (flg & FLD_TYP_DURATION_BEF || event_table.flds[j].lkup == "_DURATION_") {
					//dura = prf_obj.lua_data.data_rows[i][k].dval;
					dv.push_back(dura);
					continue;
				}
#endif
				if ((flg & FLD_TYP_STR) && k != UINT32_M1) {
					replace_substr(str, "\"", "", verbose);
					uint32_t idx = hash_string(event_table.hsh_str, event_table.vec_str, str);
					dv.push_back(idx);
					continue;
				}
				if ((flg & FLD_TYP_DBL) && k != UINT32_M1) {
					double x;
					if (use_u64) {
						x = hex_u64_val;
					} else {
				   		x = atof(str.c_str());
					}
					if (!(flg & FLD_TYP_TM_CHG_BY_CPU) && (flg & FLD_TYP_DURATION_BEF)) {
						if (event_table.flds[j].actions.size() > 0) {
							did_actions_already[j] = true;
							run_actions(x, event_table.flds[j].actions, use_value);
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
				if ((flg & FLD_TYP_DBL) && k == UINT32_M1 && !(flg & FLD_TYP_TM_CHG_BY_CPU) && (flg & FLD_TYP_DURATION_BEF)) {
					double x = 0.0;
					if (event_table.flds[j].actions.size() > 0) {
						did_actions_already[j] = true;
						run_actions(x, event_table.flds[j].actions, use_value);
					}
					dura = x;
					if (j < dv.size()) {
						dv[j] = x;
					}
					dv.push_back(x);
					continue;
				}
				if ((flg & FLD_TYP_INT) && k != UINT32_M1) {
					double x;
					if (use_u64) {
						x = hex_u64_val;
					} else {
				   		x = (double)strtoll(str.c_str(), NULL, 0);
					}
					dv.push_back(x);
					if (!(flg & FLD_TYP_TM_CHG_BY_CPU) && (flg & FLD_TYP_DURATION_BEF)) {
						dura = x;
					}
					continue;
				}
				if ((flg & FLD_TYP_SYS_CPU)) {
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
				} else if (flg & FLD_TYP_PERIOD) {
						printf("event looking for TYP_PERIOD field but it isn't defined for event %s. bye at %s %d\n",
								prf_obj.events[prf_idx].event_name.c_str(), __FILE__, __LINE__);
						exit(1);
					continue;
				} else if (flg & FLD_TYP_PID) {
					dv.push_back(pid);
					continue;
				} else if (flg & FLD_TYP_TID) {
					dv.push_back(tid);
					continue;
				} else if (flg & FLD_TYP_TID2) {
					dv.push_back(tid2);
					continue;
				} else if (flg & FLD_TYP_TIMESTAMP) {
					dv.push_back(ts);
					continue;
				} else if (flg & FLD_TYP_COMM) {
					uint32_t idx = hash_string(event_table.hsh_str, event_table.vec_str, comm);
					dv.push_back(idx);
					continue;
				} else if (flg & FLD_TYP_COMM_PID) {
					str = comm + " " + std::to_string(pid);
					uint32_t idx = hash_string(event_table.hsh_str, event_table.vec_str, str);
					dv.push_back(idx);
					continue;
				} else if (flg & FLD_TYP_COMM_PID_TID) {
					str = comm + " " + std::to_string(pid) + "/" + std::to_string(tid);
					uint32_t idx = hash_string(event_table.hsh_str, event_table.vec_str, str);
					dv.push_back(idx);
					continue;
				} else if (flg & FLD_TYP_COMM_PID_TID2) {
					str = comm2 + " " + std::to_string(pid2) + "/" + std::to_string(tid2);
					uint32_t idx = hash_string(event_table.hsh_str, event_table.vec_str, str);
					dv.push_back(idx);
					continue;
				}
				continue;
			}
			if (flg & FLD_TYP_TRC_FLD_PFX) {
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
						prf_obj.samples[i].tm_str.c_str(), i,
						flnm.c_str(), __FILE__, __LINE__);
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
			if (flg & FLD_TYP_TRC_BIN) {
				int prf_evt_idx2 = (int)prf_obj.samples[i].evt_idx;
				if (prf_obj.events[prf_evt_idx2].lst_ft_fmt_idx < 0) {
					printf("lst_ft_fmt_idx should be >= 0. Bye at %s %d\n", __FILE__, __LINE__);
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
						if (!(flg & FLD_TYP_STR)) {
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

				if (flg & FLD_TYP_TIMESTAMP || event_table.flds[j].lkup == "_TIMESTAMP_") {
					ts = prf_obj.lua_data.data_rows[i][k].dval; // this is the sec.ns ts already
					dv.push_back(ts);
					continue;
				}
				if (flg & FLD_TYP_DURATION_BEF || event_table.flds[j].lkup == "_DURATION_") {
					dura = prf_obj.lua_data.data_rows[i][k].dval;
					dv.push_back(dura);
					continue;
				}
				if (flg & FLD_TYP_STR) {
					str = prf_obj.lua_data.data_rows[i][k].str;
					uint32_t idx = hash_string(event_table.hsh_str, event_table.vec_str, str);
					dv.push_back(idx);
					continue;
				}
				if (flg & FLD_TYP_DBL) {
					double x = prf_obj.lua_data.data_rows[i][k].dval;
					dv.push_back(x);
					continue;
				}
				if (flg & FLD_TYP_INT) {
					double x = prf_obj.lua_data.data_rows[i][k].ival;
					dv.push_back(x);
					continue;
				}
				continue;
			}
			if (flg & FLD_TYP_SYS_CPU) {
				dv.push_back(prf_obj.samples[i].cpu);
				continue;
			} else if (flg & FLD_TYP_PERIOD) {
				dv.push_back(prf_obj.samples[i].period);
				continue;
			} else if (flg & FLD_TYP_PID) {
				dv.push_back(prf_obj.samples[i].pid);
				continue;
			} else if (flg & FLD_TYP_TID) {
				dv.push_back(prf_obj.samples[i].tid);
				continue;
			} else if (flg & FLD_TYP_TIMESTAMP) {
				double ts2 = 1.0e-9 * (double)prf_obj.samples[i].ts;
				dv.push_back(ts2);
				continue;
			} else if (flg & FLD_TYP_COMM) {
				str = prf_obj.samples[i].comm;
				uint32_t idx = hash_string(event_table.hsh_str, event_table.vec_str, str);
				dv.push_back(idx);
				continue;
			} else if (flg & FLD_TYP_COMM_PID) {
				str = prf_obj.samples[i].comm + " " + std::to_string(prf_obj.samples[i].pid);
				uint32_t idx = hash_string(event_table.hsh_str, event_table.vec_str, str);
				dv.push_back(idx);
				continue;
			} else if (flg & FLD_TYP_COMM_PID_TID) {
				str = prf_obj.samples[i].comm + " " + std::to_string(prf_obj.samples[i].pid) + "/" + std::to_string(prf_obj.samples[i].tid);
				uint32_t idx = hash_string(event_table.hsh_str, event_table.vec_str, str);
				dv.push_back(idx);
				continue;
			} else if (flg & FLD_TYP_STR) {
				str = "unknwn2"; // just a dummy string. value will probably be filled in later.
				uint32_t idx = hash_string(event_table.hsh_str, event_table.vec_str, str);
				dv.push_back(idx);
				continue;
			}
		}
		if (try_skipping_record) {
			try_skipping_record = false;
			continue;
		}
		if (dv.size() != fsz) {
			printf("mismatch dv.size()= %d, fsz= %d at %s %d\n", (int)dv.size(), (int)fsz, __FILE__, __LINE__);
			exit(1);
		}
		if (div_by_interval_idx.size() > 0) {
			if (dura_idx == -1) {
				printf("mess up here. If you use 'TYP_DIV_BY_INTERVAL' in (chart json file) then you must must have a field with typ_lkup= 'TYP_TM_CHG_BY_CPU|TYP_DURATION_BEF|TYP_DBL' at %s %d\n", __FILE__, __LINE__);
				exit(1);
			}
			for (uint32_t k=0; k < div_by_interval_idx.size(); k++) {
				if (dv[dura_idx] > 0.0) {
					dv[div_by_interval_idx[k]] /= dv[dura_idx];
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
			run_actions(dv[j], event_table.flds[j].actions, use_value);
			uint64_t flg = event_table.flds[j].flags;
			if (flg & FLD_TYP_STATE_AFTER) {
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
			uint32_t file_tag_idx = event_table.file_tag_idx;
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
			event_table.data.cpt_idx[0].push_back(cpt_idx);
			if (event_table.data.cpt_idx.size() == 2) {
				event_table.data.cpt_idx[1].push_back(cpt_idx2);
			}
			event_table.data.prf_sample_idx.push_back(i);
		}
		if (cpu >= 0) {
			ts_cpu[cpu] = ts;
		}
	}
	//run_heapchk("bot of fill_data_table:", __FILE__, __LINE__, 1);
	if (verbose) {
		printf("ts_cumu %f, ts_idle= %f\n", ts_cumu, ts_idle);
		printf("event= %s, event_table[%d].data.vals.size()= %d\n",
			event_table.event_name_w_area.c_str(),
			evt_idx, (int)event_table.data.vals.size());
	}
	fflush(NULL);
	return 0;
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

	printf("start civetweb thrad at %s %d\n", __FILE__, __LINE__);
	//std::vector<std::thread> thrds_started;
	{
		std::thread thrd(std::bind(&web_srvr_start, std::ref(q_from_srvr_to_clnt), std::ref(q_bin_from_srvr_to_clnt), std::ref(q_from_clnt_to_srvr), &thrd_status, verbose));
		thrds_started.push_back(std::move(thrd));
	}
	//web_push_from_srvr_to_clnt(q_from_srvr_to_clnt, 20);

	printf("waiting for thrd_status= %d at %s %d\n", thrd_status, __FILE__, __LINE__);
	while(thrd_status < THRD_RUNNING) {
#ifdef __linux__
		usleep(100000);
#else
		Sleep(100);
#endif
	}
	printf("waited for thrd_status= %d at %s %d\n", thrd_status, __FILE__, __LINE__);
	return thrd_status;
}

static std::string bin_map, bin_map2;
static std::string chrts_json, chrts_json2;

void create_web_file(int verbose)
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
  	//file << bin_map << std::endl;
  	//file << "," << std::endl;
  	//file << chrts_json << std::endl;

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
	bool write_it, first_style=true;
	bin_map2 = bin_map;
	replace_substr(bin_map2, "'", "\\'", verbose);
	chrts_json2 = chrts_json;
	replace_substr(chrts_json, "'", "\\'", verbose);

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
						ofile << "  let st_pool='" << bin_map2 << "';" << std::endl;
						ofile << "  let ch_data='" << chrts_json << "';" << std::endl;
						ofile << "  parse_str_pool(st_pool);" << std::endl;
						ofile << "  parse_chart_data(ch_data);" << std::endl;
						ofile << "  start_charts();" << std::endl;
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

void do_load_replay(int verbose)
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
			std::getline (file, chrts_json);
			//chrts_json = line;
			break;
		}
		i++;
	}
  	file.close();
	if (verbose > 1) {
		printf("read bin_map from file:\n%s\n", bin_map.c_str());
		printf("read chrts_json from file:\n%s\n", chrts_json.c_str());
	}
	fprintf(stderr, "read str_pool.sz= %d and chrts_json.sz= %d from file: %s at %s %d\n",
			(int)bin_map.size(), (int)chrts_json.size(), options.replay_filename.c_str(), __FILE__, __LINE__);
}

int read_perf_event_list_dump(file_list_str &file_list)
{
	// read the list of perf events and their format.
	// Create perf_event_list_dump.txt with dump_all_perf_events.sh (on linux of course).
	std::string base_file = "perf_event_list_dump.txt";
	std::string flnm = file_list.path + DIR_SEP + base_file;
	int rc = ck_filename_exists(flnm.c_str(), __FILE__, __LINE__, options.verbose);
	if (rc != 0) {
		fprintf(stderr, "bye at %s %d\n", __FILE__, __LINE__);
		exit(1);
	}
	printf("found file %s using filename= %s at %s %d\n", base_file.c_str(), flnm.c_str(), __FILE__, __LINE__);

	tp_read_event_formats(file_list, flnm, options.verbose);
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

	int verbose = options.verbose;
	double tm_beg = dclock();
	int thrd_status = start_web_server_threads(q_from_srvr_to_clnt, q_bin_from_srvr_to_clnt, q_from_clnt_to_srvr,
			thrds_started, verbose);

	if (options.load_replay_file) {
		do_load_replay(verbose);
		//goto load_replay_file;
	} else {

	std::vector <file_list_str> file_list;

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
	std::vector <int> grp_list;
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
		do_json(i, evt_nm, chart_file, json_evt_chrt_str, evt_tbl2[0], options.verbose);
	}
	printf("chart_file has %d events. At %s %d\n", ck_num_events, __FILE__, __LINE__);
	std::cout << "evt_tbl2[0].size() " << evt_tbl2[0].size() << std::endl;
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

	std::vector <int> file_list_1st;
	uint32_t file_tag_idx_prev = UINT32_M1, use_i=0;
	for (uint32_t i=0; i < file_list.size(); i++) {
		int v_tmp = 0;
		std::string file_tag = file_list[i].file_tag;
		uint32_t file_tag_idx = hash_string(file_tag_hash, file_tag_vec, file_tag) - 1;
		if (file_tag_idx != UINT32_M1 && file_tag_idx >= flnm_evt_hash.size()) {
			flnm_evt_hash.resize(file_tag_idx+1);
			flnm_evt_vec.resize(file_tag_idx+1);
			comm_pid_tid_hash.resize(file_tag_idx+1);
			comm_pid_tid_vec.resize(file_tag_idx+1);
			read_perf_event_list_dump(file_list[i]);
			printf("read_perf_event_list_dump(file_list[%d]) at %s %d\n", file_tag_idx, __FILE__, __LINE__);
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
			tc_parse_text(file_list[i].file_txt, prf_obj[i], tm_beg, v_tmp, evt_tbl2[0]);
			printf("\nafter tc_read_data_bin(%s) at %s %d\n", file_list[i].file_bin.c_str(), __FILE__, __LINE__);
		}
		else if (file_list[i].typ == FILE_TYP_PERF) {
			if (verbose)
				fprintf(stderr, "begin prf_read_data_bin(i=%d) elap= %f at %s %d\n", i, dclock()-tm_beg, __FILE__, __LINE__);
			prf_read_data_bin(file_list[i].file_bin, v_tmp, prf_obj[i], tm_beg, file_list[file_list_1st[i]]);
			prf_obj_prv = &prf_obj[i];
			if (verbose)
				fprintf(stderr, "begin prf_parse_text(i=%d) elap= %f at %s %d\n", i, dclock()-tm_beg, __FILE__, __LINE__);
			prf_parse_text(file_list[i].file_txt, prf_obj[i], tm_beg, v_tmp, evt_tbl2[0]);
			fprintf(stderr, "after prf_parse_text(i=%d) elap= %f flnm= %s at %s %d\n",
					i, dclock()-tm_beg, file_list[i].file_bin.c_str(), __FILE__, __LINE__);
		}
		else if (file_list[i].typ == FILE_TYP_LUA) {
			if (verbose)
				fprintf(stderr, "begin lua_read__data(i=%d) elap= %f at %s %d\n", i, dclock()-tm_beg, __FILE__, __LINE__);
			lua_read_data(file_list[i].file_bin, file_list[i].file_txt,
					file_list[i].wait_txt, prf_obj[i], file_list[i].lua_file, file_list[i].lua_rtn, verbose); // need to pass lua script filename and lua routine name
			fprintf(stderr, "begin lua_read_data(i=%d) elap= %f at %s %d\n", i, dclock()-tm_beg, __FILE__, __LINE__);
			//fprintf(stderr, "after prf_parse_text(i=%d) elap= %f at %s %d\n", i, dclock()-tm_beg, __FILE__, __LINE__);
		}
		else if (file_list[i].typ == FILE_TYP_ETW) {
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
	}

	event_table.resize(grp_list.size());

	printf("\nbefore fill_data_table: grp_list.size()= %d at %s %d\n", (int)grp_list.size(), __FILE__, __LINE__);
	for (uint32_t g=0; g < grp_list.size(); g++) {
		for (uint32_t k=0; k < file_list.size(); k++) {
			if (verbose)
				printf("bef fill_data_table: prf_obj[%d].events.size()= %d at %s %d\n",
						k, (int)prf_obj[k].events.size(), __FILE__, __LINE__);
			if (file_list[k].grp != grp_list[g]) {
				continue;
			}
			std::string evt_nm;
			for (uint32_t j=0; j < prf_obj[k].events.size(); j++) {
				if (prf_obj[k].file_type == FILE_TYP_ETW) {
					evt_nm = prf_obj[k].events[j].event_name;
				} else if (prf_obj[k].events[j].event_name.find(":") != std::string::npos) {
					evt_nm = prf_obj[k].events[j].event_name;
				} else {
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
				do_json(UINT32_M1, evt_nm, chart_file, json_evt_chrt_str, event_table[grp_list[g]], options.verbose);
				if (get_signal() == 1) {
					fprintf(stderr, "got control-c or 'quit' command from browser. Bye at %s %d\n", __FILE__, __LINE__);
					exit(1);
				}
			}
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
						if (verbose)
							printf("file_grp= %d, match prf_feat[%d] and event_table[%d][%d] %s\n",
								k, j, grp_list[g], i, event_table[grp_list[g]][i].event_name.c_str());
						fill_data_table(j, i, k, prf_obj[k], event_table[grp_list[g]][i], file_list[file_list_1st[k]], verbose);
						if (verbose)
							printf("after fill_data_table: evt= %s, event_table[%d][%d].data.vals.size()= %d at %s %d\n",
								event_table[grp_list[g]][i].event_name.c_str(),
								grp_list[g], i, (int)event_table[grp_list[g]][i].data.vals.size(), __FILE__, __LINE__);
					}
				}
				if (get_signal() == 1) {
					fprintf(stderr, "got control-c or 'quit' command from browser. Bye at %s %d\n", __FILE__, __LINE__);
					exit(1);
				}
			}
		}
	}
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


	printf("\nbefore report_chart_data:\n");
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
	printf("\nafter  report_chart_data\n");

	int chrt_num = 0;
	chrts_json = "{\"chart_data\":[";
	std::string report;
	int did_chrts = 0;
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
					if (!event_table[grp_list[g]][i].charts[j].use_chart) {
						continue;
					}
					// this resets everything. We probably don't want to reset everything for the case of multiple files in the same group.
					if (get_signal() == 1) {
						fprintf(stderr, "got control-c or 'quit' command from browser. Bye at %s %d\n", __FILE__, __LINE__);
						exit(1);
					}
					chart_lines_reset();
					double tt0=0.0, tt1=0.0, tt2=0.0;
					if (event_table[grp_list[g]][i].charts[j].chart_tag == "PCT_BUSY_BY_CPU") {
						printf("using event_table[%d][%d]= %s at %s %d\n",
							grp_list[g], j, event_table[grp_list[g]][i].event_name.c_str(), __FILE__, __LINE__);
						tt0 = dclock();
						int rc = build_chart_lines(i, j, prf_obj[k], event_table[grp_list[g]], verbose);
						tt1 = dclock();
						std::string this_chart_json = "";
						if (rc == 0) {
						if (did_chrts > 0) {
							chrts_json += ", ";
						}
						this_chart_json += build_shapes_json(file_list[k].file_tag, grp_list[g], i, j, event_table[grp_list[g]], verbose);
						did_chrts = 1;
						}
						tt2 = dclock();
						chrts_json += this_chart_json;
					} else {
						if (verbose)
							printf("using event_table[%d][%d]= %s at %s %d\n",
								grp_list[g], j, event_table[grp_list[g]][i].event_name.c_str(), __FILE__, __LINE__);
						tt0 = dclock();
						int rc = build_chart_lines(i, j, prf_obj[k], event_table[grp_list[g]], verbose);
						tt1 = dclock();
						std::string this_chart_json = "";
						if (rc == 0) {
						if (did_chrts > 0) {
							chrts_json += ", ";
						}
						this_chart_json += build_shapes_json(file_list[k].file_tag, grp_list[g], i, j, event_table[grp_list[g]], verbose);
						did_chrts = 1;
						}
						tt2 = dclock();
						chrts_json += this_chart_json;
					}
					fprintf(stderr, "tm build_chart_lines(): %f, build_shapes_json: %f title= %s at %s %d\n",
							tt1-tt0, tt2-tt1,
							event_table[grp_list[g]][i].charts[j].title.c_str(),
							__FILE__, __LINE__);
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
	std::string cats = ", \"categories\":[";
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
	chrts_json += "]" + cats + "}";
	if (options.show_json > 1) {
		printf("chrts_json=\n%s\n at %s %d\n", chrts_json.c_str(), __FILE__, __LINE__);
	}
	fflush(NULL);
	chrt_num++;
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
	fflush(NULL);
	fprintf(stderr, "callstack_vec num_strings= %d, sum of strings len= %d\n", (int)callstack_vec.size(), (int)callstack_vec_len);
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
  		file << chrts_json << std::endl;
  		file.close();
		printf("wrote str_pool and chrts_json to file: %s at %s %d\n", options.replay_filename.c_str(), __FILE__, __LINE__);
	}

	}
	
	ck_json(bin_map, "check for valid json in str_pool", __FILE__, __LINE__, options.verbose);
	ck_json(chrts_json, "check for valid json in chrts_json", __FILE__, __LINE__, options.verbose);

	if (options.web_file.size() > 0) {
		create_web_file(options.verbose);
		if (options.web_file_quit) {
			fprintf(stderr, "quitting after creating web_file %s due to using web_fl_quit option. Bye at %s %d\n",
					options.web_file.c_str(), __FILE__, __LINE__);
			exit(1);
		}
	}
	fprintf(stderr, "entering oppat loop to wait for browser to request data. Connect browser to http://localhost:%d\n",
			options.web_port);

	{
		int i=0;
		while(thrd_status == THRD_RUNNING && get_signal() == 0) {
			if (!q_from_clnt_to_srvr.is_empty()) {
				std::string j = q_from_clnt_to_srvr.pop();
				printf("msg[%d] from clnt= %s at %s %d\n", i, j.c_str(), __FILE__, __LINE__);
				i++;
				if (j == "Ready") {
					double tm_bef = dclock();
					q_from_srvr_to_clnt.push(bin_map);
					//q_bin_from_srvr_to_clnt.push(bin_str);
					q_from_srvr_to_clnt.push(chrts_json);
					double tm_aft = dclock();
					fprintf(stderr, "q_from_srvr_to_clnt.push(chrts_json) size= %d txt_sz= %d push_tm= %f at %s %d\n",
						(int)chrts_json.size(), (int)callstack_sz, tm_aft-tm_bef, __FILE__, __LINE__);
				}
			}
#ifdef __linux__
			usleep(10000);
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
