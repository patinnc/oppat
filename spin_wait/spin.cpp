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
#define _GNU_SOURCE             /* See feature_test_macros(7) */
#include <sched.h>
#include <time.h>
#endif



#include <iostream>
#include <iomanip>
#include <time.h>
#include <stdlib.h>
#include <fstream>
#include <iostream>
#include <string>
#include <cstddef>
#include <inttypes.h>
#include <stdint.h>
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
#include <sstream>
#include <vector>
#include <algorithm> 
#include <sys/types.h>
//#define _GNU_SOURCE         /* See feature_test_macros(7) */

#ifdef __linux__
#include <unistd.h>
#include <sys/syscall.h>   /* For SYS_xxx definitions */
#endif
#ifdef _WIN32
#include <intrin.h>
#pragma intrinsic(__rdtsc)
#pragma intrinsic(__rdtscp)
#else
#include <x86intrin.h>
#endif

#ifdef __APPLE__
#include <unistd.h>
#include <mach/thread_act.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <time.h>
#include "pthread_barrier_apple.h"
#endif

#include "mygetopt.h"
#include "trace_marker.h"
#include "utils.h"
#include "utils2.h"

#define MSR_TSC        0x0010
#define MSR_IA32_MPERF 0x00e7
#define MSR_IA32_APERF 0x00e8

#define CLOCK_USE_TSC 1
#define CLOCK_USE_SYS 2

#if defined(__linux__) || defined(__APPLE__)
pthread_barrier_t mybarrier;
#else
SYNCHRONIZATION_BARRIER mybarrier;
#endif
volatile int mem_bw_threads_up=0;

std::vector <std::vector <int>> nodes_cpulist;
std::vector <int> nodes_index_into_cpulist;
std::vector <int> cpu_belongs_to_which_node;
double frq=0.0, ifrq=1.0;

enum { // below enum has to be in same order as wrk_typs
	WRK_SPIN,
	WRK_FREQ2,
	WRK_FREQ,
	WRK_FREQ_SML,
	WRK_MEM_BW,
	WRK_MEM_BW_RDWR,
	WRK_MEM_BW_2RD,
	WRK_MEM_BW_2RDWR,
	WRK_MEM_BW_REMOTE,
	WRK_MEM_BW_2RD2WR,
	WRK_DISK_RD,
	WRK_DISK_WR,
	WRK_DISK_RDWR,
	WRK_DISK_RD_DIR,
	WRK_DISK_WR_DIR,
	WRK_DISK_RDWR_DIR,
};

static std::vector <std::string> wrk_typs = {
	"spin",
	"freq2",
	"freq",
	"freq_sml",
	"mem_bw",
	"mem_bw_rdwr",
	"mem_bw_2rd",
	"mem_bw_2rdwr",
	"mem_bw_remote",
	"mem_bw_2rd2wr",
	"disk_rd",
	"disk_wr",
	"disk_rdwr",
	"disk_rd_dir",
	"disk_wr_dir",
	"disk_rdwr_dir"
};

//abcd
struct sla_str {
	int level, warn, critical;
	sla_str(): level(-1), warn(-1), critical(-1) {}
};

struct options_str {
	int verbose, help, wrk_typ, cpus, clock, nopin, yield;
	std::string work, phase, bump_str, size_str, loops_str;
	uint64_t bump, size, loops;
	double spin_tm, spin_tm_multi;
	std::vector <sla_str> sla;
	options_str(): verbose(0), help(0), wrk_typ(-1), cpus(-1), clock(CLOCK_USE_SYS), nopin(0),
		yield(0), bump(0), size(0), loops(0), spin_tm(2.0), spin_tm_multi(0) {}

} options;

static unsigned int my_getcpu(void)
{
	unsigned int cpu;
	__rdtscp(&cpu);
	return cpu;
}

static uint64_t get_tsc_and_cpu(unsigned int *cpu)
{
	uint64_t tsc = __rdtscp(cpu);
	return tsc;
}




static double mclk(uint64_t t0)
{
	uint64_t t1;
	double x;
        t1 = __rdtsc();
	x = (double)(t1 - t0);
	return x*ifrq;
	
}

void do_trace_marker_write(std::string str)
{
#ifndef __APPLE__
	trace_marker_write(str);
#endif
}


void do_barrier_wait(void)
{
#if defined(__linux__) || defined(__APPLE__)
	pthread_barrier_wait(&mybarrier);
#else
	EnterSynchronizationBarrier(&mybarrier, 0);
#endif
}

int my_msleep(int msecs)
{
#if defined(__linux__) || defined(__APPLE__)
	struct timespec ts;
	uint64_t ms=msecs;

	ts.tv_sec = ms / 1000;
	ts.tv_nsec = (ms % 1000) * 1000000;
	
	return nanosleep(&ts, NULL);
#else
	Sleep(msecs);
	return 0;
#endif
}

double get_cputime(void)
{
#if defined(__linux__) || defined(__APPLE__)
	double x;
	struct timespec tp;
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &tp);
	x = (double)tp.tv_sec + 1.0e-9 * (double)tp.tv_nsec;
	return x;
#else
	// not really cpu time... need to fix this
	return dclock();
#endif
}

#if defined(__APPLE__)
// from https://yyshen.github.io/2015/01/18/binding_threads_to_cores_osx.html
// don't know license
// alternative method https://gist.github.com/Coneko/4234842
#define SYSCTL_CORE_COUNT   "machdep.cpu.core_count"

typedef struct cpu_set {
  uint32_t    count;
} cpu_set_t;

static inline void
CPU_ZERO(cpu_set_t *cs) { cs->count = 0; }

static inline void
CPU_SET(int num, cpu_set_t *cs) { cs->count |= (1 << num); }

static inline int
CPU_ISSET(int num, cpu_set_t *cs) { return (cs->count & (1 << num)); }

int sched_getaffinity(pid_t pid, size_t cpu_size, cpu_set_t *cpu_set)
{
  int32_t core_count = 0;
  size_t  len = sizeof(core_count);
  int ret = sysctlbyname(SYSCTL_CORE_COUNT, &core_count, &len, 0, 0);
  if (ret) {
    printf("error while get core count %d\n", ret);
    return -1;
  }
  cpu_set->count = 0;
  for (int i = 0; i < core_count; i++) {
    cpu_set->count |= (1 << i);
  }

  return 0;
}

int pthread_setaffinity_np(pthread_t thread, size_t cpu_size,
                           cpu_set_t *cpu_set)
{
  mach_port_t mach_thread;
  int core = 0;

  for (core = 0; core < 8 * cpu_size; core++) {
    if (CPU_ISSET(core, cpu_set)) break;
  }
  printf("binding to core %d\n", core);
  thread_affinity_policy_data_t policy = { core };
  mach_thread = pthread_mach_thread_np(thread);
  thread_policy_set(mach_thread, THREAD_AFFINITY_POLICY,
                    (thread_policy_t)&policy, 1);
  return 0;
}
#endif

void pin(int cpu)
{
   if (options.nopin == 1) {
      return;
   }
#if defined(__linux__) || defined(__APPLE__)
	int i = cpu;

   // Create a cpu_set_t object representing a set of CPUs. Clear it and mark
   // only CPU i as set.
   cpu_set_t cpuset;
   CPU_ZERO(&cpuset);
   CPU_SET(i, &cpuset);
   int rc = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
   if (rc != 0) {
     std::cerr << "Error calling pthread_setaffinity_np: " << rc << "\n";
   }
#endif
}

pid_t mygettid(void)
{
	pid_t tid = -1;
	//pid_t tid = (pid_t) syscall (SYS_gettid);
#if defined(__APPLE__)
	mach_port_t mid = pthread_mach_thread_np(pthread_self());
	tid = (int)mid;
#endif
#if defined(__linux__)
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

#if defined(__linux__) || defined(__APPLE__)
double dclock_vari(clockid_t clkid)
{
	struct timespec tp;
	clock_gettime(clkid, &tp);
	return (double)(tp.tv_sec) + 1.0e-9 * (double)(tp.tv_nsec);
}
#endif

struct rt_str {
	double cpu_time, elap_time;
};

struct args_str {
	std::string work, filename, phase;
	double spin_tm;
	unsigned long rezult, loops;
	uint64_t size, bump, tsc_initial;
	double tm_beg, tm_end, perf, dura, cpu_time;
	int id, wrk_typ, clock, got_SLA;
	std::vector <rt_str> rt_vec;
	char *dst;
	std::string bump_str, loops_str, size_str, units;
	args_str(): spin_tm(0.0), rezult(0), loops(0), dst(0), size(0), bump(0), tsc_initial(0),
		tm_beg(0.0), tm_end(0.0), perf(0.0), dura(0.0), id(-1), wrk_typ(-1), clock(CLOCK_USE_SYS),
		got_SLA(false), cpu_time(0) {}
};

struct phase_str {
	std::string work, filename, phase;
	double tm_end, perf, dura;
	phase_str(): tm_end(0.0), perf(0.0), dura(0.0) {}
};

uint64_t do_scale(uint64_t loops, uint64_t rez, uint64_t &ops)
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

int array_read_write(char *buf, int ar_sz, int strd)
{
	int res=0;
	for (int i=0; i < ar_sz-strd; i += strd) {
		buf[i] += i;
	}
	return res;
}

int array_2read_write(char *dst, char *src, int ar_sz, int strd)
{
	int res=0;
	for (int i=0; i < ar_sz-strd; i += strd) {
		dst[i] += src[i];
	}
	return res;
}

int array_2read_2write(char *dst, char *src, int ar_sz, int strd)
{
	int res=0;
	for (int i=0; i < ar_sz-strd; i += strd) {
		dst[i] += res;
		src[i] += res;
		res++;
	}
	return res;
}

int array_2read(char *dst, char *src, int ar_sz, int strd)
{
	int res=0;
	for (int i=0; i < ar_sz-strd; i += strd) {
		res += dst[i] + src[i];
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

static size_t sys_getpagesize(void)
{
	size_t pg_sz = 4096; // sane? default
#if defined(__linux__) || defined(__APPLE__)
	pg_sz = sys_getpagesize();
#else
	SYSTEM_INFO siSysInfo;
	// Copy the hardware information to the SYSTEM_INFO structure.
	GetSystemInfo(&siSysInfo);
	pg_sz = siSysInfo.dwPageSize;
#endif
	return pg_sz;
}

static int alloc_pg_bufs(int num_bytes, char **buf_ptr, int pg_chunks)
{
	size_t pg_sz = sys_getpagesize();
	size_t use_bytes = num_bytes;
	if ((use_bytes % pg_sz) != 0) {
		int num_pgs = use_bytes/pg_sz;
		use_bytes = (num_pgs+1) * pg_sz;
	}
	if ((use_bytes % (pg_chunks * pg_sz)) != 0) {
		int num_pgs = use_bytes/(pg_chunks*pg_sz);
		use_bytes = (num_pgs+1) * (pg_chunks*pg_sz);
	}
	int rc=0;
#if defined(__linux__) || defined(__APPLE__)
	rc = posix_memalign((void **)buf_ptr, pg_sz, use_bytes);
#else
	*buf_ptr = (char *)_aligned_malloc(use_bytes, pg_sz);
#endif
	if (rc != 0 || *buf_ptr == NULL) {
		printf("failed to malloc %" PRId64 " bytes on alignment= %d at %s %d\n",
			(int64_t)use_bytes, (int)pg_sz, __FILE__, __LINE__);
		exit(1);
	}
	return pg_sz;
}

#ifndef O_BINARY
#define O_BINARY 0
#endif

#ifndef O_DIRECT
#define O_DIRECT 0
#endif

static size_t disk_write_dir(int myi, int ar_sz)
{
	char *buf;
	size_t byts = 0;
	int val=0, loops = args[myi].loops, pg_chunks=16;

	int pg_sz = alloc_pg_bufs(ar_sz, &buf, pg_chunks);
#ifdef _WIN32
	HANDLE fd = CreateFile(args[myi].filename.c_str(), GENERIC_WRITE,
		0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_NO_BUFFERING, NULL);
#else
	int fd = open(args[myi].filename.c_str(), O_WRONLY|O_CREAT|O_BINARY|O_DIRECT, 0x666);
#endif
	if (!fd)
	{
		printf("Unable to open file %s at %s %d\n", args[myi].filename.c_str(), __FILE__, __LINE__);
		exit(1);
	}
	int num_pages = ar_sz / pg_sz;
	for (int i=0; i < loops; i++) {
		for (int j= 0; j < ar_sz; j+= 256) {
			buf[j] += j + val;
		}
		val++;
		for (int j=0; j < num_pages; j += pg_chunks) {
#ifdef _WIN32
			DWORD dwWritten=0;
			WriteFile(fd, buf+(j*pg_sz), pg_chunks*pg_sz, &dwWritten, NULL);
			byts += dwWritten;
#else
			byts += write(fd, buf+(j*pg_sz), pg_chunks*pg_sz);
#endif
		}
	}
	args[myi].rezult = val;
#ifdef _WIN32
	CloseHandle(fd);
#else
	close(fd);
#endif
	return byts;
}

static size_t disk_read_dir(int myi, int ar_sz)
{
	int val=0, pg_chunks=16;
	char *buf;
	size_t ret;
	size_t byts = 0;
	int loops = args[myi].loops;

	int pg_sz = alloc_pg_bufs(ar_sz, &buf, pg_chunks);
#ifdef _WIN32
	HANDLE fd = CreateFile(args[myi].filename.c_str(), GENERIC_READ,
		0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_NO_BUFFERING, NULL);
#else
	int fd = open(args[myi].filename.c_str(), O_RDONLY | O_BINARY | O_DIRECT, 0);
#endif
	if (!fd)
	{
		printf("Unable to open file %s at %s %d\n", args[myi].filename.c_str(), __FILE__, __LINE__);
		exit(1);
	}
	int num_pages = ar_sz / pg_sz;
	for (int i=0; i < loops; i++) {
		for (int j=0; j < num_pages; j += pg_chunks) {
#ifdef _WIN32
			DWORD dwGot=0;
			ReadFile(fd, buf+(j*pg_sz), pg_chunks*pg_sz, &dwGot, NULL);
			byts += dwGot;
#else
			byts += read(fd, buf+(j*pg_sz), pg_chunks*pg_sz);
#endif
		}
		for (int j= 0; j < ar_sz; j+= 256) {
			val += buf[j];
		}
	}
#ifdef _WIN32
	CloseHandle(fd);
#else
	close(fd);
#endif
	args[myi].rezult = val;
	return byts;
}

static size_t disk_write(int myi, int ar_sz)
{
	FILE *fp;
	int val=0, pg_chunks=16;
	char *buf;
	size_t byts=0;
	int loops = args[myi].loops;

	int pg_sz = alloc_pg_bufs(ar_sz, &buf, pg_chunks);
	fp = fopen(args[myi].filename.c_str(), "wb");
	if (!fp)
	{
		printf("Unable to open file %s at %s %d\n", args[myi].filename.c_str(), __FILE__, __LINE__);
		exit(1);
	}
	int num_pages = ar_sz / pg_sz;
	for (int i=0; i < loops; i++) {
		for (int j= 0; j < ar_sz; j+= 512) {
			buf[j] += j + val;
		}
		val++;
		for (int j=0; j < num_pages; j += pg_chunks) {
			byts += fwrite(buf+(j*pg_sz), pg_chunks*pg_sz, 1, fp);
		}
	}
	byts *= pg_chunks * pg_sz;
	args[myi].rezult = val;
	fclose(fp);
	return byts;
}

static size_t disk_read(int myi, int ar_sz)
{
	FILE *fp;
	int val=0, pg_chunks=16;
	char *buf;
	size_t ret;
	size_t byts=0;
	int loops = args[myi].loops;

	int pg_sz = alloc_pg_bufs(ar_sz, &buf, pg_chunks);
	fp = fopen(args[myi].filename.c_str(), "rb");
	if (!fp)
	{
		printf("Unable to open file %s at %s %d\n", args[myi].filename.c_str(), __FILE__, __LINE__);
		exit(1);
	}
	int num_pages = ar_sz / pg_sz;
	for (int i=0; i < loops; i++) {
		for (int j=0; j < num_pages; j += pg_chunks) {
			byts += fread(buf+(j*pg_sz), pg_chunks*pg_sz, 1, fp);
		}
		for (int j= 0; j < ar_sz; j+= 512) {
			val += buf[j];
		}
	}
	byts *= pg_chunks * pg_sz;
	fclose(fp);
	args[myi].rezult = val;
	return byts;
}

float disk_all(unsigned int i)
{
	double tm_to_run = args[i].spin_tm;
	int cpu =  args[i].id;
	uint64_t j;
	uint64_t rezult = 0;
	double tm_end, tm_beg, bytes=0.0;
	int arr_sz = 1024*1024;
	args[i].filename = "disk_tst_" + std::to_string(i);
	tm_end = tm_beg = dclock();
	while((tm_end - tm_beg) < tm_to_run) {
		switch(args[i].wrk_typ) {
			case WRK_DISK_WR:
				bytes += disk_write(i, arr_sz);
				break;
			case WRK_DISK_RDWR:
				bytes += disk_write(i, arr_sz);
			case WRK_DISK_RD:
				bytes += disk_read(i, arr_sz);
				break;
			case WRK_DISK_WR_DIR:
				bytes += disk_write_dir(i, arr_sz);
				break;
			case WRK_DISK_RDWR_DIR:
				bytes += disk_write_dir(i, arr_sz);
			case WRK_DISK_RD_DIR:
				bytes += disk_read_dir(i, arr_sz);
				break;
		}
		tm_end = dclock();
	}
	double dura2, dura;
	dura2 = dura = tm_end - tm_beg;
	if (dura2 == 0.0) {
		dura2 = 1.0;
	}
	args[i].dura = dura;
	args[i].tm_end = tm_end;
	args[i].perf = 1.0e-6 * (double)(bytes)/dura2;
	args[i].units = "MiB/sec";
	printf("cpu[%d]: tid= %d, beg/end= %f,%f, dura= %f, %s= %f\n",
		cpu, mygettid(), tm_beg, tm_end, dura, args[i].units.c_str(), args[i].perf);
	args[i].rezult = bytes;
	args[i].tm_beg = tm_beg;
	args[i].tm_end = tm_end;
	return 0;
}

float mem_bw(unsigned int i)
{
	char *dst, *src;
	double tm_to_run = args[i].spin_tm;
	int cpu =  args[i].id;
	//unsigned int i;
	uint64_t ops = 0;
	uint64_t j, loops;
	uint64_t rezult = 0;
	double tm_end, tm_beg, bytes=0.0;
	int strd = (int)args[i].bump;
	int arr_sz = (int)args[i].size;
	//fprintf(stderr, "got to %d\n", __LINE__);
	if (args[i].size_str.size() > 0 && (strstr(args[i].size_str.c_str(), "k") || strstr(args[i].size_str.c_str(), "K"))) {
		arr_sz *= 1024;
	}
	if (args[i].size_str.size() > 0 && (strstr(args[i].size_str.c_str(), "m") || strstr(args[i].size_str.c_str(), "M"))) {
		arr_sz *= 1024*1024;
	}
	pin(i);
	my_msleep(0); // necessary? in olden times it was.
	if (i==0) {
		printf("strd= %d, arr_sz= %d, %d KB, %.4f MB\n",
			strd, arr_sz, arr_sz/1024, (double)(arr_sz)/(1024.0*1024.0));
	}
	dst = (char *)malloc(arr_sz);
	array_write(dst, arr_sz, strd);
	if (args[i].wrk_typ == WRK_MEM_BW_2RDWR ||
		args[i].wrk_typ == WRK_MEM_BW_2RD ||
		args[i].wrk_typ == WRK_MEM_BW_2RD2WR) {
		src = (char *)malloc(arr_sz);
		array_write(src, arr_sz, strd);
	}
	bool do_loop = true;
	if (args[i].wrk_typ == WRK_MEM_BW_REMOTE) {
		int nd = cpu_belongs_to_which_node[cpu];
		int cpu_order_for_nd = nodes_index_into_cpulist[cpu];
		int other_nd = (nd == 0 ? 1 : 0);
		int cpu_to_switch_to = -1;
		if (cpu_order_for_nd < nodes_cpulist[other_nd].size()) {
			cpu_to_switch_to = nodes_cpulist[other_nd][cpu_order_for_nd];
		}
		if (cpu_to_switch_to != -1 && nd == 1) {
			printf("thread[%.3d] malloc'd memory on cpu= %d node= %d and going to read the memory on cpu= %d node= %d\n", 
				i, i, nd, cpu_to_switch_to, other_nd);
			pin(cpu_to_switch_to);
			my_msleep(0);
		} else {
			do_loop = false;
		}
#if 1
		if (nd == 0) {
			do_loop = false;
		}
#endif
	        //printf("try mem_bw_remote cpu= %d, nd= %d, oth_nd= %d, cpu_2_sw= %d at %s %d\n", cpu, nd, other_nd, cpu_to_switch_to, __FILE__, __LINE__);
	}

	mem_bw_threads_up++;
	do_barrier_wait();

	tm_end = tm_beg = dclock();
	int iters=0;
	loops=1;
	double tm_prev;
	if (!do_loop) {
		args[i].dura = 0;
		args[i].tm_end = tm_end;
		args[i].units = "GB/sec";
		args[i].perf = 0.0;
		return 0.0;
	}
	while((tm_end - tm_beg) < tm_to_run) {
#if 1
		for (int ii=0; ii < loops; ii++) 
		{
#endif
		if (args[i].wrk_typ == WRK_MEM_BW) {
			rezult += array_read(dst, arr_sz, strd);
			bytes += arr_sz;
		}
		else if (args[i].wrk_typ == WRK_MEM_BW_RDWR) {
			rezult += array_read_write(dst, arr_sz, strd);
			bytes += 2*arr_sz;
		}
		else if (args[i].wrk_typ == WRK_MEM_BW_2RD) {
			rezult += array_2read(dst, src, arr_sz, strd);
			bytes += 2*arr_sz;
		}
		else if (args[i].wrk_typ == WRK_MEM_BW_2RDWR) {
			rezult += array_2read_write(dst, src, arr_sz, strd);
			bytes += 3*arr_sz;
		}
		else if (args[i].wrk_typ == WRK_MEM_BW_REMOTE) {
			rezult += array_read(dst, arr_sz, strd);
			bytes += arr_sz;
		}
		else if (args[i].wrk_typ == WRK_MEM_BW_2RD2WR) {
			rezult += array_2read_2write(dst, src, arr_sz, strd);
			bytes += 4*arr_sz;
		}
#if 1
		}
#endif
		tm_end = dclock();
#if 1
		// try to reduce the time spend in dclock()
		if (++iters > 2) {
			if ((tm_end - tm_prev) < 0.01) {
				loops += 10;
			}
			iters = 3;
		}
		tm_prev = tm_end;
#endif
	}
	double dura2, dura;
	dura2 = dura = tm_end - tm_beg;
	if (dura2 == 0.0) {
		dura2 = 1.0;
	}
	args[i].dura = dura;
	args[i].tm_end = tm_end;
	args[i].units = "GB/sec";
	args[i].perf = 1.0e-9 * (double)(bytes)/(dura2);
	printf("cpu[%3d]: tid= %6d, beg/end= %f,%f, dura= %f, Gops= %f, %s= %f\n",
		cpu, mygettid(), tm_beg, tm_end, dura, 1.0e-9 * (double)ops, args[i].units.c_str(), args[i].perf);
	args[i].rezult = rezult;
	return rezult;
}

#ifndef _WIN32
#define D1 " rorl $2, %%eax;"
//#define D1 " andl $1, %%eax;"
//#define D1 " rcll $1, %%eax;"
#define D10 D1 D1 D1 D1 D1  D1 D1 D1 D1 D1
#define D100 D10 D10 D10 D10 D10  D10 D10 D10 D10 D10
#define D1000 D100 D100 D100 D100 D100  D100 D100 D100 D100 D100
#define D10000 D1000 D1000 D1000 D1000 D1000  D1000 D1000 D1000 D1000 D1000
#define D100000 D10000 D10000 D10000 D10000 D10000  D10000 D10000 D10000 D10000 D10000
#define D40000 D10000 D10000 D10000 D10000
#define D1000000 D100000 D100000 D100000 D100000 D100000  D100000 D100000 D100000 D100000 D100000

#define C1 " rorl $2, %%ecx; rorl $2, %%eax;"
//#define C1 " andl $1, %%eax;"
//#define C1 " rcll $1, %%eax;"
#define C10 C1 C1 C1 C1 C1  C1 C1 C1 C1 C1
#define C100 C10 C10 C10 C10 C10  C10 C10 C10 C10 C10
#define C1000 C100 C100 C100 C100 C100  C100 C100 C100 C100 C100
#define C10000 C1000 C1000 C1000 C1000 C1000  C1000 C1000 C1000 C1000 C1000
#define C100000 C10000 C10000 C10000 C10000 C10000  C10000 C10000 C10000 C10000 C10000
#define C40000 C10000 C10000 C10000 C10000
#define C1000000 C100000 C100000 C100000 C100000 C100000  C100000 C100000 C100000 C100000 C100000
#endif

#ifdef _WIN32
#if defined __cplusplus
extern "C" { /* Begin "C" */
/* Intrinsics use C name-mangling.  */
#endif /* __cplusplus */
#endif
extern unsigned int win_rorl(unsigned int b);
#ifdef _WIN32
#if defined __cplusplus
}
/* Intrinsics use C name-mangling.  */
#endif /* __cplusplus */
#endif
float simd_dot0(unsigned int i)
{
	double tm_to_run = args[i].spin_tm;
	int a = 43;
	int cpu =  args[i].id, clock = args[i].clock;
	int iiloops = 0, iters = 0;
	//unsigned int i;
	uint64_t ops = 0;
	uint64_t j, loops;
	uint64_t rezult = 0;
	double tm_end, tm_beg, tm_prev;
	double tm_endt, tm_begt, tm_prevt;
	double xbeg, xend, xprev, xcumu = 0.0,  xinst=0.0, xfreq, xelap=0.0;
	loops = args[i].loops;
	mem_bw_threads_up++;
	double tm_tsc0, tm_tsc1;
	uint64_t tsc0, tsc1;
	uint32_t cpu0, cpu1, cpu_initial;
	double did_iters=0;

	do_barrier_wait();
	uint64_t tsc_initial = get_tsc_and_cpu(&cpu_initial);
	args[i].tsc_initial = tsc_initial;

	tm_end = tm_beg = dclock();

	if (args[i].wrk_typ == WRK_FREQ_SML) {
		int b = 2, imx = 100000, ii;
		if (loops != 0) {
			imx = loops;
		}
		ops = 0;
		xbeg = get_cputime();
		xprev = xbeg;
#if 0
		while((tm_end - tm_beg) < tm_to_run) {
		did_iters++;
		for (ii=0; ii < imx; ii++) {
			asm ( "movl %1, %%eax;"
				".align 4;"
				D1000
				" movl %%eax, %0;"
				:"=r"(b) /* output */
				:"r"(a)  /* input */
				:"%eax"  /* clobbered reg */
			);
			a |= b;
		}
		tm_end = dclock();
		}
#else
		if (clock == CLOCK_USE_SYS) {
			tm_endt = tm_begt = dclock();
		} else {
			tm_endt = tm_begt = mclk(tsc_initial);
		}
		tm_prevt = tm_begt;
		while((tm_endt - tm_begt) < tm_to_run) {
			did_iters++;
			for (ii=0; ii < imx; ii++) {
#ifdef _WIN32
				b = win_rorl(b); // 10000 inst
#else
				asm ( "movl %1, %%eax;"
					".align 4;"
					D10000
					" movl %%eax, %0;"
					:"=r"(b) /* output */
					:"r"(a)  /* input */
					:"%eax"  /* clobbered reg */
				);
#endif
				a |= b;
			}
			if (clock == CLOCK_USE_SYS) {
				tm_endt = dclock();
			} else {
				tm_endt = mclk(tsc_initial);
			}
			if (args[i].got_SLA) {
				struct rt_str rt;
				rt.elap_time = tm_endt - tm_prevt;
				xend = get_cputime();
				rt.cpu_time = xend - xprev;
				args[i].rt_vec.push_back(rt);
				tm_prevt = tm_endt;
				xprev = xend;
			}
#ifdef __linux__
		   if (options.yield == 1) {
		      pthread_yield();
		   }
#endif
		}
#endif
		xend = get_cputime();
		xcumu += xend-xbeg;
		xinst += (double)10000 * (double)imx;
		xinst *= did_iters;
		//printf("xcumu tm= %.3f, xinst= %g freq= %.3f GHz, i= %d, wrk_typ= %d\n", xcumu, xinst, 1.0e-9 * xinst/xcumu, i, args[i].wrk_typ);
		ops = (uint64_t)(xinst);
		tm_end = dclock();
	}
	if (args[i].wrk_typ == WRK_FREQ
#ifdef _WIN32
		|| args[i].wrk_typ == WRK_FREQ2
#endif
		) {
		int imx = 100, ii, b;
		if (loops != 0) {
			imx = loops;
		}
		ops = 0;
		xbeg = get_cputime();
		while((tm_end - tm_beg) < tm_to_run) {
		did_iters++;
		for (ii=0; ii < imx; ii++) {
#ifdef _WIN32
			for (int jj=0; jj < 100; jj++) {
				a = win_rorl(a); // 10,000 rorl * 1000 loops = 1M rorl 
			}
			b = a;
#else
			asm ("movl %1, %%eax;"
				".align 4;"
				D1000000
				" movl %%eax, %0;"
				:"=r"(b) /* output */
				:"r"(a)  /* input */
				:"%eax"  /* clobbered reg */
			);
#endif
			a |= b;
		}
		tm_end = dclock();
		}
		xend = get_cputime();
		xinst += (double)1000000 * (double)imx;
		xinst *= did_iters;
		xcumu += xend-xbeg;
		//printf("xcumu tm= %.3f, xinst= %g freq= %.3f GHz, i= %d, wrk_typ= %d\n", xcumu, xinst, 1.0e-9 * xinst/xcumu, i, args[i].wrk_typ);
		ops = (uint64_t)(xinst);
	}
#ifndef _WIN32
	// experimental... just working on this on linux so far
	if (args[i].wrk_typ == WRK_FREQ2) {
		int imx = 100, ii, b;
		ops = 0;
		xbeg = get_cputime();
		while((tm_end - tm_beg) < tm_to_run) {
		did_iters++;
		for (ii=0; ii < imx; ii++) {
			asm ("movl %1, %%eax;"
			     "movl %1, %%ecx;"
				".align 4;"
				C1000000
				" movl %%ecx, %0;"
				:"=r"(b) /* output */
				:"r"(a)  /* input */
				:"%ecx","%eax"  /* clobbered reg */
			);
			a |= b;
		}
		tm_end = dclock();
		}
		xend = get_cputime();
		xinst += (double)1000000 * (double)imx;
		xinst *= did_iters;
		xcumu += xend-xbeg;
		//printf("xcumu tm= %.3f, xinst= %g freq= %.3f GHz, i= %d, wrk_typ= %d\n", xcumu, xinst, 1.0e-9 * xinst/xcumu, i, args[i].wrk_typ);
		ops = (uint64_t)(xinst);
	}
#endif

	if (args[i].wrk_typ == WRK_SPIN) {
		xbeg = get_cputime();
		while((tm_end - tm_beg) < tm_to_run) {
#if 1
		for (int ii=0; ii < iiloops; ii++) 
		{
#endif
#if 1
		rezult = do_scale(loops, rezult, ops);
#endif
#if 1
		}
#endif
		did_iters++;
		tm_end = dclock();
#if 1
		// try to reduce the time spend in dclock()
		if (++iters > 2) {
			if ((tm_end - tm_prev) < 0.01) {
				iiloops += 10;
			}
			iters = 3;
		}
		tm_prev = tm_end;
#endif
		}
		xend = get_cputime();
		xcumu += xend-xbeg;
	}
	double dura2, dura;
	dura2 = dura = tm_end - tm_beg;
	if (dura2 == 0.0) {
		dura2 = 1.0;
	}
	args[i].dura = dura;
	args[i].tm_beg = tm_beg;
	args[i].tm_end = tm_end;
	args[i].units = "Gops/sec";
	if (args[i].wrk_typ == WRK_SPIN) {
		args[i].perf = 1.0e-9 * (double)(ops)/(dura2);
		args[i].rezult = rezult;
	}
	if (args[i].wrk_typ == WRK_FREQ || args[i].wrk_typ == WRK_FREQ2 || args[i].wrk_typ == WRK_FREQ_SML) {
		args[i].perf = 1.0e-9 * xinst/xcumu;
		args[i].rezult = (double)a;
		args[i].cpu_time = xcumu;
		args[i].dura = xcumu;
		//rezult = (float)a;
		//printf("in simd[%d].perf= %f\n", i, args[i].perf);
	}
	if (did_iters == 0.0) {
		did_iters=1.0;
	}
	printf("cpu[%3d]: tid= %6d, beg/end= %f,%f, dura= %f, CPUms/iter= %f ms, iter/CPUsec= %f, rt_vec.sz= %d, cpu_tm= %f, Gops= %f, %s= %f\n",
		cpu, mygettid(), tm_beg, tm_end, dura, 1000.0*xcumu/did_iters, did_iters/xcumu, (int)args[i].rt_vec.size(), xcumu, 1.0e-9 * (double)ops, args[i].units.c_str(), args[i].perf);
	return rezult;
}

float dispatch_work(int  i)
{
	float res=0.0;
	int wrk = args[i].wrk_typ;

	do_trace_marker_write("Begin "+wrk_typs[wrk]+" for thread= "+std::to_string(i));
	switch(wrk) {
		case WRK_SPIN:
		case WRK_FREQ:
		case WRK_FREQ2:
		case WRK_FREQ_SML:
			res = simd_dot0(i);
			break;
		case WRK_MEM_BW:
		case WRK_MEM_BW_RDWR:
		case WRK_MEM_BW_2RDWR:
		case WRK_MEM_BW_2RD2WR:
		case WRK_MEM_BW_REMOTE:
		case WRK_MEM_BW_2RD:
			res = mem_bw(i);
			break;
		case WRK_DISK_RD:
		case WRK_DISK_WR:
		case WRK_DISK_RDWR:
		case WRK_DISK_RD_DIR:
		case WRK_DISK_WR_DIR:
		case WRK_DISK_RDWR_DIR:
			res = disk_all(i);
			break;
		default:
			break;
	}
	do_trace_marker_write("End "+wrk_typs[wrk]+" for thread= "+std::to_string(i));
	return res;
}



std::vector <std::string> split_cmd_line(const char *argv0, const char *cmdline, int *argc)
{
	std::vector <std::string> std_argv;
	int slen = strlen(cmdline);
	int i=0, arg = -1;
	char *argv[256];
	char *cp = (char *)malloc(slen+1);
	argv[++arg] = (char *)argv0;
	strcpy(cp, cmdline);
	char qt[2];
	qt[0] = 0;
	while (i < slen) {
		if (cp[i] != ' ') {
			if (cp[i] == '"' || cp[i] == '\'') {
			   qt[0] = cp[i];
			   i++;
			}
			argv[++arg] = cp+i;
			while (((cp[++i] != ' ' && qt[0] == 0) || (qt[0] != 0 && cp[i] != qt[0])) && i < slen) {
				;
			}
			if (qt[0] != 0 && cp[i] == qt[0]) {
				qt[0] = 0;
				cp[i] = 0;
				i++;
			}
		}
		if (cp[i] == ' ') {
			cp[i] = 0;
			i++;
		}
	}
	for (i=0; i <= arg; i++) {
		//printf("arg[%d]= '%s'\n", i, argv[i]);
		std_argv.push_back(argv[i]);
	}
	*argc = (int)std_argv.size();
	return std_argv;
}

void opt_set_spin_tm(const char *optarg)
{
	const char *cpc = strchr(optarg, ',');
	options.spin_tm = atof(optarg);
	if (cpc) {
		options.spin_tm_multi = atof(cpc+1);
	} else {
		options.spin_tm_multi = options.spin_tm;
	}
	printf("spin_tm single_thread= %f, multi_thread= %f at %s %d\n",
		options.spin_tm, options.spin_tm_multi, __FILE__, __LINE__);
}

void opt_set_clock(const char *optarg)
{
	int use = -1;
        if (*optarg == 't' || *optarg == 'T') {
		use = CLOCK_USE_TSC;
	} else if (*optarg == 's' || *optarg == 'S') {
		use = CLOCK_USE_SYS;
	}
	if (use == -1) {
		printf("error: got '-c %s' but arg to -c must be 't' (for use TSC) or 's' (for use system clock)\n", optarg);
		exit(1);
	}
	options.clock = use;
	printf("got -c %s\n", optarg);
}

void opt_set_cpus(const char *optarg)
{
	options.cpus = atoi(optarg);
	printf("cpus= %s\n", optarg);
}

void opt_set_nopin(void)
{
	options.nopin = 1;
	printf("nopin= 1\n");
}

void opt_set_yield(void)
{
	options.yield = 1;
	printf("yield= 1\n");
}

void opt_set_size(const char *optarg)
{
	options.size = atoi(optarg);
	options.size_str = optarg;
	printf("array size is %s\n", optarg);
}

void opt_set_SLA(const char *optarg)
{
	char *cp, *cp0;
	int len = (int)strlen(optarg);
	int i=0, lvl=0;
	cp = (char *)optarg;
	if (*cp != 'p') {
		printf("expected to find 'p' at position %d, (like p90:), got '%c' in -S '%s'\n", i, *cp, optarg);
		printf("bye at %s %d\n", __FILE__, __LINE__);
		exit(1);
	}
	i++;
	cp++;
	lvl = atoi(cp);
	struct sla_str ss;
	ss.level = lvl;
	cp0 = strchr(cp, ':');
	if (!cp0) {
		printf("got SLA p%d\n", lvl);
		options.sla.push_back(ss);
		return;
	}
	i = (int)(cp0 - optarg) + 1;
	if (i >= len) {
		// just report this service level, no warn or critical levels
		printf("got SLA p%d\n", lvl);
		options.sla.push_back(ss);
		return;
	}
	cp = (char *)(optarg+i);
	ss.warn = atoi(cp);
	printf("got SLA p%d:%d\n", lvl, ss.warn);
	cp0 = (char *)strchr(optarg, ',');
	if (cp0) {
		cp0++;
		ss.critical = atoi(cp0);
		printf("got SLA p%d:%d,%d\n", lvl, ss.warn, ss.critical);
	}
	options.sla.push_back(ss);
	printf("did SLA[%d] -S %s\n", (int)(options.sla.size()-1), optarg);
}

void opt_set_loops(const char *optarg)
{
	options.loops = atoi(optarg);
	options.loops_str = optarg;
	printf("loops size is %s\n", optarg);
}

void opt_set_bump(const char *optarg)
{
	options.bump = atoi(optarg);
	options.bump_str = optarg;
	printf("bump (stride) size is %s\n", optarg);
}

void opt_set_phase(const char *optarg)
{
	options.phase = optarg;
	printf("phase string is %s\n", optarg);
}

void opt_set_wrk_typ(const char *optarg)
{
	options.work = optarg;
	options.wrk_typ = UINT32_M1;
	for (uint32_t j=0; j < wrk_typs.size(); j++) {
		if (options.work == wrk_typs[j]) {
			options.wrk_typ = j;
			printf("got work= '%s' at %s %d\n", options.work.c_str(), __FILE__, __LINE__);
			break;
		}
	}
	if (options.wrk_typ == UINT32_M1) {
		printf("Error in arg 2. Must be 1 of:\n");
		for (uint32_t j=0; j < wrk_typs.size(); j++) {
			printf("\t%s\n", wrk_typs[j].c_str());
		}
		printf("Bye at %s %d\n", __FILE__, __LINE__);
		exit(1);
	}
}

static int
get_opt_main (int argc, std::vector <std::string> argvs)
{
	int c;

	int verbose_flag=0, help_flag=0, show_json=0;
	const char **argv;

	argv = (const char **)malloc((1+argvs.size())*sizeof(const char *));

	for (size_t i=0; i < argvs.size(); i++) {
		argv[i] = (const char *)argvs[i].c_str();
	}
	argv[argvs.size()] = 0;
	

	struct option long_options[] =
	{
		/* These options set a flag. */
		{"verbose",     no_argument,       0, 'v', "set verbose mode"
			"   Specify '-v' multiple times to increase the verbosity level.\n"
		},
		{"nopin",     no_argument,       0, 'P', "don't pin the thread to the cpu"
			"   The default is to pin thread 0 to cpu 0, thread 1 to cpu 1, etc.\n"
			"   This option is in intended for you to use some other tool (like numactl --cpunodebind=0 --membind=1 ...\n"
		},
		{"yield",     no_argument,       0, 'Y', "yield the cpu after each loop"
			"   The default is to not yield the cpu after each loop.\n"
			"   This option is intended for the freq_sml work type.\n"
		},
		{"help",        no_argument,       &help_flag, 'h', "display help and exit"},
		/* These options don't set a flag.
			We distinguish them by their indices. */
		{"time",      required_argument,   0, 't', "time to run (in seconds)\n"
		   "   -t tm_secs[,tm_secs_multi] \n"
		   "   required arg\n"
		},
		{"work",      required_argument,   0, 'w', "type of work to do\n"
		   "   work_type: spin|mem_bw|mem_bw_rdwr|mem_bw_2rd|mem_bw_2rdwr|mem_bw_2rd2wr|mem_bw_remote|disk_rd|disk_wr|disk_rdwr|disk_rd_dir|disk_wr_dir|disk_rdwr_dir\n"
		   "   required arg"
		},
		{"bump",      required_argument,   0, 'b', "bytes to bump through array (in bytes)\n"
		   "   '-b 64' when looping over array, use 64 byte stride\n"
		   "   For mem_bw tests, the default stride size is 64 bytes."
		},
		{"size",      required_argument,   0, 's', "array size (bytes)\n"
		   "   you can add 'k' (KB), 'm' (MB) \n"
		   "   '-s 100m' (100 MB) \n"
		   "   For mem_bw tests, the default array size is 80m."
		},
		{"phase",      required_argument,   0, 'p', "phase string\n"
		   "   phase string which will be put into ETW/ftrace event string\n"
		   "   optional arg\n"
		},
		{"loops",      required_argument,   0, 'l', "bytes to bump through array (in bytes)\n"
		   "   '-l 100' for disk tests. Used in work=spin to set number of loops over array\n"
		   "   for disk tests, default is 100. spin loops = 10000."
		},
		{"num_cpus",      required_argument,   0, 'n', "number of cpus to use\n"
		   "   '-n 1' will use just 1 cpu.\n"
		   "   For mem_bw/freq tests.\n"
		   "   The default is all cpus (1 thread per cpu).\n"
		},
		{"SLA",      required_argument,   0, 'S', "Service Level Agreement\n"
		   "   Enter -S pLVL0:warn0,crit0;pLVL1:warn1,crit1... for example\n"
		   "   '-S p50:80,100 -S p95:200,250 -S p99:250,300'\n"
		   "   where p50 means 50 percent of response times below  80 ms is okay, give a warning for above 80 and critical warning if p50 is above 100.\n"
		   "     and p95 means 95 percent of response times below 200 ms is okay, give a warning for above 80 and critical warning if p50 is above 100.\n"
		   "   You can enter multiple -S options.\n"
		   "   The default is no SLA, no SLA analysis\n"
		},
		{"clock",      required_argument,   0, 'c', "select clock to use for freq_sml\n"
		   "   '-c s' use system clock: clock_gettime() on linux and gettimeofday() clone for windows\n"
		   "   '-c t' use tsc clock: has less overhead (no syscall) but if migrate between cpus then problems\n"
		   "   default is '-c s'"
		},
		{0, 0, 0, 0}
	};

	while (1)
	{
		/* getopt_long stores the option index here. */
		int option_index = 0;

		c = mygetopt_long(argc, argv, "hvPYb:c:l:n:p:s:S:t:w:", long_options, &option_index);

		/* Detect the end of the options. */
		if (c == -1)
			break;

		switch (c)
		{
		case 0:
//abcd
			fprintf(stderr, "opt[%d]= %s\n", option_index, long_options[option_index].name);
			if (strcmp(long_options[option_index].name, "nopin") == 0) {
				opt_set_nopin();
				break;
			}
			if (strcmp(long_options[option_index].name, "yield") == 0) {
				opt_set_yield();
				break;
			}
			if (strcmp(long_options[option_index].name, "time") == 0) {
				opt_set_spin_tm(optarg);
				break;
			}
			if (strcmp(long_options[option_index].name, "work") == 0) {
				opt_set_wrk_typ(optarg);
				break;
			}
			if (strcmp(long_options[option_index].name, "clock") == 0) {
				opt_set_clock(optarg);
				break;
			}
			if (strcmp(long_options[option_index].name, "bump") == 0) {
				opt_set_bump(optarg);
				break;
			}
			if (strcmp(long_options[option_index].name, "num_cpus") == 0) {
				opt_set_cpus(optarg);
				break;
			}
			if (strcmp(long_options[option_index].name, "size") == 0) {
				opt_set_size(optarg);
				break;
			}
			if (strcmp(long_options[option_index].name, "SLA") == 0) {
				opt_set_SLA(optarg);
				break;
			}
			if (strcmp(long_options[option_index].name, "loops") == 0) {
				opt_set_loops(optarg);
				break;
			}
			if (strcmp(long_options[option_index].name, "phase") == 0) {
				opt_set_phase(optarg);
				break;
			}

		case 'h':
			printf ("option -h set\n");
			help_flag++;
			break;
		case 'b':
			opt_set_bump(optarg);
			break;
		case 'c':
			opt_set_clock(optarg);
			break;
		case 'l':
			opt_set_loops(optarg);
			break;
		case 'n':
			opt_set_cpus(optarg);
			break;
		case 'P':
			opt_set_nopin();
			break;
		case 'Y':
			opt_set_yield();
			break;
		case 's':
			opt_set_size(optarg);
			break;
		case 'S':
			opt_set_SLA(optarg);
			break;
		case 'p':
			opt_set_phase(optarg);
			break;
		case 't':
			printf ("option -t with value `%s'\n", optarg);
			opt_set_spin_tm(optarg);
			break;
		case 'w':
			printf ("option -w with value `%s'\n", optarg);
			opt_set_wrk_typ(optarg);
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

	free(argv);

	if (options.wrk_typ == WRK_SPIN && options.loops == 0) {
		options.loops = 10000;
		printf("setting default loops= %d\n", (int)options.loops);
	}


	return 0;
}


int read_options_file(std::string argv0, std::string opts, std::vector <std::vector <std::string>> &argvs)
{
	std::ifstream file2;
	file2.open (opts.c_str(), std::ios::in);
	if (!file2.is_open()) {
		printf("messed up fopen of flnm= %s at %s %d\n", opts.c_str(), __FILE__, __LINE__);
		exit(1);
	}
	std::vector <std::string> opt_lines;
	std::string line2;
	int i=0;
	while(!file2.eof()){
		std::getline (file2, line2);
		if (line2.size() > 0) {
			int argc;
			std::vector <std::string> argv;
			if (line2.size() > 0 && line2.substr(0,1) != "#") {
				opt_lines.push_back(line2);
				printf("line[%d]= '%s'\n", i++, line2.c_str());
				argv = split_cmd_line(argv0.c_str(), line2.c_str(), &argc);
#if 0
#ifdef _WIN32
				argv = CommandLineToArgvA((PCHAR)line2.c_str(), &argc);
#else
				argv = split_commandline(line2.c_str(), &argc);
#endif
#endif
				argvs.push_back(argv);
				for (uint32_t i=0; i < argv.size(); i++) {
					printf("opts_file: argv[%d]='%s'\n", i, argv[i].c_str());
				}
				printf("argc= %d\n", argc);
			}
		}
	}
	file2.close();
	return 0;
}

int read_file_into_string(std::string filename, std::string &buf)
{
	std::ifstream t(filename);
	std::stringstream buffer;
	buffer << t.rdbuf();
	buf = buffer.str();
	if (buf.size() > 0) {
		if (buf[buf.size()-1] == '\n') {
			buf.resize(buf.size()-1);
		}
	}
	return 0;
}

std::vector <std::vector <std::string>> argvs;
std::vector <phase_str> phase_vec;

bool compare_by_elap_time(const rt_str &a, const rt_str &b)
{
    return a.elap_time < b.elap_time;
}

int main(int argc, char **argv)
{
	uint32_t cpu, cpu0;
	uint64_t t0, t1;
	double t_first = dclock(), tm_beg, tm_end;
	unsigned num_cpus = std::thread::hardware_concurrency();
	bool doing_disk = false;
	std::mutex iomutex;
	printf("t_first= %.9f, build_date= %s, time= %s\n", t_first, __DATE__, __TIME__);
#if defined(__linux__) || defined(__APPLE__)
	printf("t_raw= %.9f\n", dclock_vari(CLOCK_MONOTONIC_RAW));
#if !defined(__APPLE__)
	printf("t_coarse= %.9f\n", dclock_vari(CLOCK_MONOTONIC_COARSE));
#endif
#if defined(__APPLE__)
	printf("t_boot= %.9f\n", dclock_vari(_CLOCK_UPTIME_RAW));
#else
	printf("t_boot= %.9f\n", dclock_vari(CLOCK_BOOTTIME));
#endif
	double proc_cputime = dclock_vari(CLOCK_PROCESS_CPUTIME_ID);
#endif
	std::cout << "Start Test 1 CPU" << std::endl; // prints !!!Hello World!!!
	double t_start, t_end;
	time_t c_start, c_end;
	printf("usage: %s -t tm_secs[,tm_secs_multi] -w work_type [ arg3 [ arg4 ]]]\n", argv[0]);
	printf("\twork_type: spin|mem_bw|mem_bw_rdwr|mem_bw_2rd|mem_bw_2rdwr|mem_bw_2rd2wr|mem_bw_remote|disk_rd|disk_wr|disk_rdwr|disk_rd_dir|disk_wr_dir|disk_rdwr_dir\n");
	printf("if mem_bw: -b stride in bytes. -s arg4 array_size in bytes\n");
	printf("Or first 2 options can be '-f input_file' where each line of input_file is the above cmdline options\n");
	printf("see input_files/haswell_spin_input.txt for example.\n");


	cpu = my_getcpu();
	cpu0 = cpu;
        tm_beg = tm_end = dclock();
        t0 = __rdtsc();
        while(tm_end - tm_beg < 0.1) {
        	t1 = get_tsc_and_cpu(&cpu0);
		tm_end = dclock();
	}
	frq = (double)(t1-t0)/(tm_end - tm_beg);
	ifrq = 1.0/frq;
	printf("tsc_freq= %.3f GHz, cpu_beg= %d, cpu_end= %d\n", frq*1.0e-9, cpu, cpu0);


	uint32_t wrk_typ = WRK_SPIN;
	std::string work = "spin";
	std::string phase_file;

	if (argc >= 2 && std::string(argv[1]) == "-f") {
		read_options_file(argv[0], argv[2], argvs);
		for (int j=1; j < (argc-1); j++) {
			if (strcmp(argv[j], "-p") == 0) {
				phase_file = argv[j+1];
				printf("phase_file= %s at %s %d\n", phase_file.c_str(), __FILE__, __LINE__);
				break;
			}
		}
	} else {
		std::vector <std::string> av;
		for (int j=0; j < argc; j++) {
			av.push_back(argv[j]);
		}
		argvs.push_back(av);
	}
	for (uint32_t j=0; j < argvs.size(); j++) {
		uint32_t i=1;
		std::string phase;
		options.bump = 0;
		options.size = 0;
		get_opt_main((int)argvs[j].size(), argvs[j]);
		if (options.wrk_typ == WRK_MEM_BW || options.wrk_typ == WRK_MEM_BW_RDWR ||
			options.wrk_typ == WRK_MEM_BW_2RDWR || options.wrk_typ == WRK_MEM_BW_2RD ||
			options.wrk_typ == WRK_MEM_BW_REMOTE || 
			options.wrk_typ == WRK_MEM_BW_2RD2WR) {
			if (options.bump == 0) {
				options.bump = 64;
			}
			if (options.size == 0) {
				options.size = 80*1024*1024;
			}
		}
		if (argvs[j].size() > i) {
//abcd
			if (options.wrk_typ >= WRK_DISK_RD && options.wrk_typ <= WRK_DISK_RDWR_DIR) {
				doing_disk = true;
				options.loops = 100;
				printf("Setting disk loops to default -l %d\n", (int)options.loops);
			}
			if (options.wrk_typ == WRK_MEM_BW_REMOTE) {
				int nd_max = -1;
				std::vector <std::string> cpu_str_vec;
				for (int nd=0; nd < 2; nd++) {
					std::string nd_file= "/sys/devices/system/node/node"+std::to_string(nd)+"/cpulist";
					int rc = ck_filename_exists(nd_file.c_str(), __FILE__, __LINE__, 0);
					if (rc != 0) {
						printf("you requested mem_bw_remote but I don't see any node file named '%s'. Bye at %s %d\n",
							nd_file.c_str(), __FILE__, __LINE__);
						exit(0);
					}
					std::string cpu_str;
					rc = read_file_into_string(nd_file, cpu_str);
					cpu_str_vec.push_back(cpu_str);
				}
				// simple list assumed: 1, 2, 3 or 1-2, 4-6, 7 so comma separate groups of 1 or a range of cpus
				printf("Only using 2 nodes for mem_bw_remote at %s %d\n", __FILE__, __LINE__);
				nodes_cpulist.resize(2);
				cpu_belongs_to_which_node.resize(num_cpus, -1);
				nodes_index_into_cpulist.resize(num_cpus, -1);
				for (int nd=0; nd < 2; nd++) {
					//printf("cpu_str= %s\n", cpu_str_vec[nd].c_str());
					std::vector <std::string> grps;
					tkn_split(cpu_str_vec[nd], ",", grps);
					for (int grp=0; grp < grps.size(); grp++) {
						std::vector <std::string> beg_end;
						tkn_split(grps[grp], "-", beg_end);
						//printf("nd %d, grp[%d]= '%s', beg_end_sz= %d\n", nd, grp, grps[grp].c_str(), (int)beg_end.size());
						if (beg_end.size() == 0) {
							beg_end.push_back(grps[grp]);
							beg_end.push_back(grps[grp]);
						}
						int cpu_beg = atoi(beg_end[0].c_str());
						int cpu_end = atoi(beg_end[1].c_str());
						for (int cbe=cpu_beg; cbe <= cpu_end; cbe++) {
							//printf("nd %d, grp[%d]= '%s' cpu[%d]= %d\n", nd, grp, grps[grp].c_str(), (int)nodes_cpulist[nd].size(), cbe);
							cpu_belongs_to_which_node[cbe] = nd;
							nodes_index_into_cpulist[cbe] = (int)nodes_cpulist[nd].size();
							nodes_cpulist[nd].push_back(cbe);
						}
					}
				}
			}
		}

		if (options.cpus > 0 && options.cpus < num_cpus) {
			num_cpus = options.cpus;
		}
#if defined(__linux__) || defined(__APPLE__)
	pthread_barrier_init(&mybarrier, NULL, num_cpus+1);
#else
	InitializeSynchronizationBarrier(&mybarrier, num_cpus+1, -1);
#endif
		args.resize(num_cpus);
		std::vector<std::thread> threads(num_cpus);
		for (uint32_t i=0; i < num_cpus; i++) {
			args[i].phase     = options.phase;
			args[i].spin_tm   = options.spin_tm;
			args[i].id = i;
			args[i].loops     = options.loops;
			args[i].loops_str = options.loops_str;
			args[i].size      = options.size;
			args[i].size_str  = options.size_str;
			args[i].bump      = options.bump;
			args[i].clock     = options.clock;
			args[i].bump_str  = options.bump_str;
			args[i].work      = options.work;
			args[i].wrk_typ   = options.wrk_typ;
			args[i].got_SLA   = false;
			if (options.sla.size() > 0) {
				args[i].got_SLA = true;
			}
		}
		if (doing_disk && options.spin_tm > 0.0) {
			if (phase.size() > 0) {
				do_trace_marker_write("begin phase ST "+phase);
			}
			t_start = dclock();
			dispatch_work(0);
			t_end = dclock();
			if (phase.size() > 0) {
				std::string str = "end phase ST "+phase+", dura= "+std::to_string(args[0].dura)+", "+args[0].units+"= "+std::to_string(args[0].perf);
				do_trace_marker_write(str);
				printf("%s\n", str.c_str());
			}
		}
		for (unsigned i=0; i < num_cpus; i++) {
			args[i].spin_tm = options.spin_tm_multi;
		}
		t_start = dclock();
		printf("work= %s\n", work.c_str());

		if (!doing_disk && options.spin_tm_multi > 0.0) {
			std::vector <rt_str> rt_vec;
			uint64_t t0=0, t1;
			int sleep_ms = 1; // 1 ms
			for (unsigned i = 0; i < num_cpus; ++i) {
				threads[i] = std::thread([&iomutex, i] {
					dispatch_work(i);
				});
				while(mem_bw_threads_up <= i) {
					my_msleep(sleep_ms);
				}
			}
			if (phase.size() > 0) {
				do_trace_marker_write("begin phase MT "+phase);
			}
			do_barrier_wait();

			for (auto& t : threads) {
				t.join();
			}
			double tot = 0.0;
			for (unsigned i = 0; i < num_cpus; ++i) {
				tot += args[i].perf;
				if (options.verbose > 0) {
					t0 = args[0].tsc_initial;
					t1 = args[i].tsc_initial;
					if (t1 > t0) { t1 -= t0; } else { t1 = t0 - t1; }
					double tdf = ifrq * (double)t1;
					printf("tsc0= %" PRIu64", abs(dff from cpu0) %g secs, %%thread_time/elap= %.3f%%\n", args[i].tsc_initial, tdf, 100.0*args[i].cpu_time/(args[i].tm_end-args[i].tm_beg));
				}
				if (args[i].rt_vec.size() > 0) {
					rt_vec.insert(rt_vec.end(), args[i].rt_vec.begin(), args[i].rt_vec.end());	
				}
			}
			if (phase.size() > 0) {
				phase_str ps;
				ps.phase = phase+", "+args[0].units+"= "+std::to_string(tot);
				ps.dura = args[0].dura;
				ps.tm_end = args[0].tm_end;
				phase_vec.push_back(ps);
				std::string str = "end phase MT "+phase+", dura= "+std::to_string(args[0].dura)+", "+args[0].units+"= "+std::to_string(tot);
				do_trace_marker_write(str);
				printf("%s\n", str.c_str());
			}
			printf("work= %s, threads= %d, total perf= %.3f %s\n", wrk_typs[args[0].wrk_typ].c_str(), (int)args.size(), tot, args[0].units.c_str());
			if (rt_vec.size() > 0) {
				std::sort(rt_vec.begin(), rt_vec.end(), compare_by_elap_time);
				double cpu_tm=0.0, elap_tm=0.0;
				for (int ss=0; ss <= (int)rt_vec.size(); ss++) {
					cpu_tm += rt_vec[ss].cpu_time;
					elap_tm += rt_vec[ss].elap_time;
				}
				cpu_tm /= (double)(rt_vec.size());
				elap_tm /= (double)(rt_vec.size());
				printf("avg cpu_time for responses= %.3f ms, avg elap_time= %.3f ms, max_elap_tm= %.3f ms\n",
					1000.0*cpu_tm, 1000.0*elap_tm, 1000.0*rt_vec[rt_vec.size()-1].elap_time);
				for (int s=0; s < (int)options.sla.size(); s++) {
					int lvl = options.sla[s].level;
					double dlvl = 0.01*(double)lvl;
					double dsz = (double)rt_vec.size();
					int idx = (int)(dlvl * dsz);
					if (idx < 0 || idx >= (int)dsz) {
						printf("something wrong with SLA level p%d, got idx= %d but rt_vec.sz= %d... at %s %d\n", lvl, idx, (int)dsz, __FILE__, __LINE__);
					} else {
						printf("p%d: idx= %d, response tm= %.3f ms, warn= %d, crit= %d\n", lvl, idx, 1000 * rt_vec[idx].elap_time, options.sla[s].warn, options.sla[s].critical);
					}
				}
			}
		}

		t_end = dclock();
		if (options.loops > 0 && options.loops < 100 && !doing_disk && options.spin_tm_multi > 0.0) {
			std::cout << "\nExecution time on " << num_cpus << " CPUs: " << t_end - t_start << " secs" << std::endl;
		}
	}
#if defined(__linux__) || defined(__APPLE__)
	proc_cputime = dclock_vari(CLOCK_PROCESS_CPUTIME_ID) - proc_cputime;
	printf("process cpu_time= %.6f secs at %s %d\n", proc_cputime, __FILE__, __LINE__);
#endif
	if (phase_file.size() > 0) {
		std::ofstream file2;
		file2.open (phase_file.c_str(), std::ios::out);
		if (!file2.is_open()) {
			printf("messed up fopen of flnm= %s at %s %d\n", phase_file.c_str(), __FILE__, __LINE__);
			exit(1);
		}
		for (uint32_t i=0; i < phase_vec.size(); i++) {
			file2 << std::fixed << std::setprecision( 9 ) << phase_vec[i].tm_end << "\t" << phase_vec[i].dura << "\t" << phase_vec[i].phase << std::endl;
		}
		file2.close();
	}
	if (argc > 20) {
		for (unsigned i=0; i < num_cpus; i++) {
			printf("rezult[%d]= %lu\n", i, args[i].rezult);
		}
	}
	return 0;
}

