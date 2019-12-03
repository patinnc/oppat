/* Copyright (c) 2018 Patrick Fay
 *
 * License http://opensource.org/licenses/mit-license.php MIT License
 */

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <assert.h>
#define pid_t int
#else
#include <wordexp.h>
#endif

#include <iostream>
#include <time.h>
#include <stdlib.h>
#include <fstream>
#include <iostream>
#include <string>
#include <cstddef>
#include <inttypes.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
//#include <arm_neon.h>
#include <ctime>
#include <cstdio>
#include <string>
#include <thread>
#include <mutex>
#include <iostream>
#include <vector>
#include <sys/types.h>
//#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include <utils.h>

#ifdef __linux__
#include <sys/time.h>
#include <unistd.h>
#include <sys/syscall.h>   /* For SYS_xxx definitions */
#endif



#ifdef __linux__
double dclock_vari(clockid_t clkid)
{
	struct timespec tp;
	clock_gettime(clkid, &tp);
	return (double)(tp.tv_sec) + 1.0e-9 * (double)(tp.tv_nsec);
}

#if 0
double dclock(void)
{
	struct timespec tp;
	clock_gettime(CLOCK_MONOTONIC, &tp);
	return (double)(tp.tv_sec) + 1.0e-9 * (double)(tp.tv_nsec);
}
#endif

double mygettimeofday(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	double x = (double)tv.tv_sec + 1.0e-6 * (double)(tv.tv_usec);
	return x;
}


#endif

int main(int argc, char **argv)
{

	std::vector <std::vector <std::string>> argvs;
	std::string loops_str, adder_str;
	double t_first = dclock();
	printf("t_first= %.9f\n", t_first);
#ifdef __linux__
	printf("t_raw= %.9f\n", dclock_vari(CLOCK_MONOTONIC_RAW));
	printf("t_coarse= %.9f\n", dclock_vari(CLOCK_MONOTONIC_COARSE));
	printf("t_boot= %.9f\n", dclock_vari(CLOCK_BOOTTIME));
	printf("t_mono= %.9f\n", dclock_vari(CLOCK_MONOTONIC));
	printf("gettimeofday= %.9f\n", mygettimeofday());
#endif
	exit(0);
}
