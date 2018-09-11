/* Copyright (c) 2018 Patrick Fay
 *
 * License http://opensource.org/licenses/mit-license.php MIT License
 */

#ifdef _WIN32
#include <windows.h>
#define pid_t int
#endif

#include <iostream>
#include <time.h>
#include <stdlib.h>
//#include <arm_neon.h>
#include <ctime>
#include <cstdio>
#include <thread>
#include <mutex>
#include <iostream>
#include <vector>
#include <sys/types.h>
//#define _GNU_SOURCE         /* See feature_test_macros(7) */

#ifdef __linux__
#include <unistd.h>
#include <sys/syscall.h>   /* For SYS_xxx definitions */
#endif

#include "utils.h"


pid_t mygettid(void)
{
	pid_t tid = -1;
	//pid_t tid = (pid_t) syscall (SYS_gettid);
#ifdef __linux__
	tid = (pid_t) syscall (__NR_gettid);
#endif
#ifdef _WIN32
    tid = (int)GetCurrentThreadId();
#endif
	return tid;
}

#if 0
//get from utils.cpp now
double dclock(void)
{
	struct timespec tp;
	clock_gettime(CLOCK_MONOTONIC, &tp);
	return (double)(tp.tv_sec) + 1.0e-9 * (double)(tp.tv_nsec);
}
#endif

#ifdef __linux__
double dclock_vari(clockid_t clkid)
{
	struct timespec tp;
	clock_gettime(clkid, &tp);
	return (double)(tp.tv_sec) + 1.0e-9 * (double)(tp.tv_nsec);
}
#endif

struct args_str {
	std::string work;
	double spin_tm;
	unsigned long rezult, loops, adder;
	int id;
	args_str(): spin_tm(0.0), rezult(0), loops(0), adder(0), id(-1) {}
};

uint64_t do_scale(uint64_t loops, uint64_t rez, uint64_t adder, uint64_t &ops)
{
	uint64_t rezult = rez;
	for (uint64_t j = 0; j < loops; j++) {
		rezult += j;
	}
	ops += loops;
	return rezult;
}

std::vector <args_str> args;

int array_write(char *buf, int ar_sz, int strd)
{
	int res=0;
	for (int i=0; i < ar_sz-strd; i += strd) {
		buf[i] = i;
	}
	return res;
}

int array_read(char *buf, int ar_sz, int strd)
{
	int res=0;
	for (int i=0; i < ar_sz-strd; i += strd) {
		res += buf[i];
	}
	return res;
}

float mem_bw(unsigned int i)
{
	double tm_to_run = args[i].spin_tm;
	int cpu =  args[i].id;
	//unsigned int i;
	uint64_t ops = 0;
	uint64_t j, loops;
	uint64_t rezult = 0;
	double tm_end, tm_beg, bytes=0.0;
	loops = args[i].loops;
	uint64_t adder = args[i].adder;
	char *buf;
	int strd=64, arr_sz = 80*1024*1024;
	buf = (char *)malloc(arr_sz);
	array_write(buf, arr_sz, strd);
	tm_end = tm_beg = dclock();
	while((tm_end - tm_beg) < tm_to_run) {
		rezult += array_read(buf, arr_sz, strd);
		bytes += arr_sz;
		tm_end = dclock();
	}
	double dura = tm_end - tm_beg;
	printf("cpu[%d]: tid= %d, beg/end= %f,%f, dura= %f, Gops= %f, GB/sec= %f\n",
		cpu, mygettid(), tm_beg, tm_end, dura, 1.0e-9 * (double)ops, 1.0e-9 * (double)(bytes)/(dura));
	args[i].rezult = rezult;
	return rezult;
}

float simd_dot0(unsigned int i)
{
	double tm_to_run = args[i].spin_tm;
	int cpu =  args[i].id;
	//unsigned int i;
	uint64_t ops = 0;
	uint64_t j, loops;
	uint64_t rezult = 0;
	double tm_end, tm_beg;
	loops = args[i].loops;
	uint64_t adder = args[i].adder;
	tm_end = tm_beg = dclock();
	while((tm_end - tm_beg) < tm_to_run) {
#if 1
		rezult = do_scale(loops, rezult, adder, ops);
#else
		for (j = 0; j < loops; j++) {
			rezult += j;
		}
		ops += loops;
#endif
		tm_end = dclock();
	}
	double dura = tm_end - tm_beg;
	printf("cpu[%d]: tid= %d, beg/end= %f,%f, dura= %f, Gops= %f, Gops/sec= %f\n",
		cpu, mygettid(), tm_beg, tm_end, dura, 1.0e-9 * (double)ops, 1.0e-9 * (double)(ops)/(dura));
	args[i].rezult = rezult;
	return rezult;
}

float dispatch_work(int  i)
{
	float res=0.0;

	if (args[i].work == "spin") {
		res = simd_dot0(i);
	} else if (args[i].work == "mem_bw") {
		res = mem_bw(i);
	}
	return res;
}

int main(int argc, char **argv)
{

	double t_first = dclock();
	unsigned num_cpus = std::thread::hardware_concurrency();
	double spin_tm = 2.0;
	std::mutex iomutex;
	std::vector<std::thread> threads(num_cpus);
	printf("t_first= %.9f\n", t_first);
#ifdef __linux__
	printf("t_raw= %.9f\n", dclock_vari(CLOCK_MONOTONIC_RAW));
	printf("t_coarse= %.9f\n", dclock_vari(CLOCK_MONOTONIC_COARSE));
	printf("t_boot= %.9f\n", dclock_vari(CLOCK_BOOTTIME));
#endif
	std::cout << "Start Test 1 CPU" << std::endl; // prints !!!Hello World!!!
	double t_start, t_end;
	time_t c_start, c_end;
	unsigned long adder=1, loops = 0xffffff;

	int i=1;
	if (argc > i) {
		spin_tm = atof(argv[i]);
	}
	i++; //2
	std::string work = "spin";
	if (argc > i) {
		work = std::string(argv[i]);
		if (work != "spin" && work != "mem_bw") {
			printf("supported work types = 'spin' and 'mem_bw' at %s %d\n", __FILE__, __LINE__);
			exit(1);
		}
	}
	i++; //3
	if (argc > i) {
		loops = atoi(argv[i]);
	}
	i++; //4
	if (argc > i) {
		adder = atoi(argv[i]);
	}

	args.resize(num_cpus);
	for (unsigned i=0; i < num_cpus; i++) {
		args[i].spin_tm = spin_tm;
		args[i].id = i;
		args[i].loops = loops;
		args[i].adder = adder;
		args[i].work = work;
	}
	t_start = dclock();
	dispatch_work(0);
	t_end = dclock();
	t_start = dclock();

	for (unsigned i = 0; i < num_cpus; ++i) {
		threads[i] = std::thread([&iomutex, i] {
			dispatch_work(i);
		});
	}

	for (auto& t : threads) {
		t.join();
	}

	t_end = dclock();
	std::cout << "\nExecution time on 4 CPUs: " << t_end - t_start << " secs" << std::endl;
	if (loops < 100) {
		for (unsigned i=0; i < num_cpus; i++) {
			printf("rezult[%d]= %lu\n", i, args[i].rezult);
		}
	}
	return 0;
}

