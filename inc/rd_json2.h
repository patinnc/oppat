/* Copyright (c) 2018 Patrick Fay
 *
 * License http://opensource.org/licenses/mit-license.php MIT License
 */


#pragma once

struct comm_pid_tid_str {
	std::string comm;
	int pid, tid;
	double total;
	comm_pid_tid_str(): pid(-1), tid(-1), total(0.0) {}
};


