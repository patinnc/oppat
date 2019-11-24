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
#include <sys/types.h>
//#define _GNU_SOURCE         /* See feature_test_macros(7) */

#ifdef __linux__
#include <unistd.h>
#include <sys/syscall.h>   /* For SYS_xxx definitions */
#endif

#ifdef __APPLE__
#include <unistd.h>
#include <mach/thread_act.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <time.h>
#include "pthread_barrier_apple.h"
#endif

#include "trace_marker.h"
#include "utils.h"
#include "utils2.h"

#if defined(__linux__) || defined(__APPLE__)
pthread_barrier_t mybarrier;
#else
SYNCHRONIZATION_BARRIER mybarrier;
#endif
volatile int mem_bw_threads_up=0;

std::vector <std::vector <int>> nodes_cpulist;
std::vector <int> nodes_index_into_cpulist;
std::vector <int> cpu_belongs_to_which_node;

enum { // below enum has to be in same order as wrk_typs
	WRK_SPIN,
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

void do_trace_marker_write(std::string str)
{
#ifndef __APPLE__
	trace_marker_write("begin phase MT "+phase);
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

struct args_str {
	std::string work, filename, phase;
	double spin_tm;
	unsigned long rezult, loops, adder;
	double tm_beg, tm_end, perf, dura;
	int id, wrk_typ;
	char *dst;
	std::string loops_str, adder_str, units;
	args_str(): spin_tm(0.0), rezult(0), loops(0), adder(0), dst(0),
		tm_beg(0.0), tm_end(0.0), perf(0.0), dura(0.0), id(-1), wrk_typ(-1) {}
};

struct phase_str {
	std::string work, filename, phase;
	double tm_end, perf, dura;
	phase_str(): tm_end(0.0), perf(0.0), dura(0.0) {}
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
	uint64_t adder = args[i].adder;
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
	int strd = (int)args[i].loops;
	int arr_sz = (int)args[i].adder;
	//fprintf(stderr, "got to %d\n", __LINE__);
	if (args[i].adder_str.size() > 0 && (strstr(args[i].adder_str.c_str(), "k") || strstr(args[i].adder_str.c_str(), "K"))) {
		arr_sz *= 1024;
	}
	if (args[i].adder_str.size() > 0 && (strstr(args[i].adder_str.c_str(), "m") || strstr(args[i].adder_str.c_str(), "M"))) {
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
		if (cpu_to_switch_to != -1) {
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
	        printf("try mem_bw_remote cpu= %d, nd= %d, oth_nd= %d, cpu_2_sw= %d at %s %d\n", cpu, nd, other_nd, cpu_to_switch_to, __FILE__, __LINE__);
	}

	mem_bw_threads_up++;
#if defined(__linux__) || defined(__APPLE__)
	pthread_barrier_wait(&mybarrier);
#else
	EnterSynchronizationBarrier(&mybarrier, 0);
#endif
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
	mem_bw_threads_up++;
#if defined(__linux__) || defined(__APPLE__)
	pthread_barrier_wait(&mybarrier);
#else
	EnterSynchronizationBarrier(&mybarrier, 0);
#endif
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
	double dura2, dura;
	dura2 = dura = tm_end - tm_beg;
	if (dura2 == 0.0) {
		dura2 = 1.0;
	}
	args[i].dura = dura;
	args[i].tm_end = tm_end;
	args[i].units = "Gops/sec";
	args[i].perf = 1.0e-9 * (double)(ops)/(dura2);
	printf("cpu[%3d]: tid= %6d, beg/end= %f,%f, dura= %f, Gops= %f, %s= %f\n",
		cpu, mygettid(), tm_beg, tm_end, dura, 1.0e-9 * (double)ops, args[i].units.c_str(), args[i].perf);
	args[i].rezult = rezult;
	return rezult;
}

float dispatch_work(int  i)
{
	float res=0.0;
	int wrk = args[i].wrk_typ;

	do_trace_marker_write("Begin "+wrk_typs[wrk]+" for thread= "+std::to_string(i));
	switch(wrk) {
		case WRK_SPIN:
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

int main(int argc, char **argv)
{

	std::string loops_str, adder_str;
	double t_first = dclock();
	unsigned num_cpus = std::thread::hardware_concurrency();
	double spin_tm = 2.0, spin_tm_multi=0.0;
	bool doing_disk = false;
	std::mutex iomutex;
	//num_cpus = 1;
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
	unsigned long adder=1, loops = 0xffffff;
	printf("usage: %s tm_secs[,tm_secs_multi] [work_type [ arg3 [ arg4 ]]]\n", argv[0]);
	printf("\twork_type: spin|mem_bw|mem_bw_rdwr|mem_bw_2rd|mem_bw_2rdwr|mem_bw_2rd2wr|mem_bw_remote|disk_rd|disk_wr|disk_rdwr|disk_rd_dir|disk_wr_dir|disk_rdwr_dir\n");
	printf("if mem_bw: arg3 is stride in bytes. arg4 is array size in bytes\n");
	printf("Or first 2 options can be '-f input_file' where each line of input_file is the above cmdline options\n");
	printf("see input_files/haswell_spin_input.txt for example.\n");
#if defined(__linux__) || defined(__APPLE__)
	pthread_barrier_init(&mybarrier, NULL, num_cpus+1);
#else
	InitializeSynchronizationBarrier(&mybarrier, num_cpus+1, -1);
#endif

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
	args.resize(num_cpus);
	for (uint32_t j=0; j < argvs.size(); j++) {
		uint32_t i=1;
		std::string phase;
		if (argvs[j].size() > i) {
			char *cpc;
			cpc = strchr((char *)argvs[j][i].c_str(), ',');
			spin_tm = atof(argvs[j][i].c_str());
			if (cpc) {
				spin_tm_multi = atof(cpc+1);
			} else {
				spin_tm_multi = spin_tm;
			}
			printf("spin_tm single_thread= %f, multi_thread= %f at %s %d\n",
					spin_tm, spin_tm_multi, __FILE__, __LINE__);
		}
		i++; //2
		if (argvs[j].size() > i) {
			work = argvs[j][i];
			wrk_typ = UINT32_M1;
			for (uint32_t j=0; j < wrk_typs.size(); j++) {
				if (work == wrk_typs[j]) {
					wrk_typ = j;
					printf("got work= '%s' at %s %d\n", work.c_str(), __FILE__, __LINE__);
					break;
				}
			}
			if (wrk_typ == UINT32_M1) {
				printf("Error in arg 2. Must be 1 of:\n");
				for (uint32_t j=0; j < wrk_typs.size(); j++) {
					printf("\t%s\n", wrk_typs[j].c_str());
				}
				printf("Bye at %s %d\n", __FILE__, __LINE__);
				exit(1);
			}
			if (wrk_typ >= WRK_DISK_RD && wrk_typ <= WRK_DISK_RDWR_DIR) {
				doing_disk = true;
				loops = 100;
			}
			if (wrk_typ == WRK_MEM_BW_REMOTE) {
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
					printf("cpu_str= %s\n", cpu_str_vec[nd].c_str());
					std::vector <std::string> grps;
					tkn_split(cpu_str_vec[nd], ",", grps);
					for (int grp=0; grp < grps.size(); grp++) {
						std::vector <std::string> beg_end;
						tkn_split(grps[grp], "-", beg_end);
						printf("nd %d, grp[%d]= '%s', beg_end_sz= %d\n", nd, grp, grps[grp].c_str(), (int)beg_end.size());
						if (beg_end.size() == 0) {
							beg_end.push_back(grps[grp]);
							beg_end.push_back(grps[grp]);
						}
						int cpu_beg = atoi(beg_end[0].c_str());
						int cpu_end = atoi(beg_end[1].c_str());
						for (int cbe=cpu_beg; cbe <= cpu_end; cbe++) {
							printf("nd %d, grp[%d]= '%s' cpu[%d]= %d\n", nd, grp, grps[grp].c_str(), (int)nodes_cpulist[nd].size(), cbe);
							cpu_belongs_to_which_node[cbe] = nd;
							nodes_index_into_cpulist[cbe] = (int)nodes_cpulist[nd].size();
							nodes_cpulist[nd].push_back(cbe);
						}
					}
				}
			}
			if (wrk_typ == WRK_MEM_BW || wrk_typ == WRK_MEM_BW_RDWR ||
				wrk_typ == WRK_MEM_BW_2RDWR || wrk_typ == WRK_MEM_BW_2RD ||
				wrk_typ == WRK_MEM_BW_REMOTE || 
				wrk_typ == WRK_MEM_BW_2RD2WR) {
				loops = 64;
				adder = 80*1024*1024;
			}
		}
		i++; //3
		if (argvs[j].size() > i) {
			loops = atoi(argvs[j][i].c_str());
			loops_str = argvs[j][i];
			printf("arg3 is %s\n", loops_str.c_str());
		}
		i++; //4
		if (argvs[j].size() > i) {
			adder = atoi(argvs[j][i].c_str());
			adder_str = argvs[j][i];
			printf("arg4 is %s\n", adder_str.c_str());
		}
		i++; //5
		if (argvs[j].size() > i) {
			phase = argvs[j][i];
			printf("arg5 phase '%s'\n", phase.c_str());
		}

		std::vector<std::thread> threads(num_cpus);
		for (uint32_t i=0; i < num_cpus; i++) {
			args[i].phase   = phase;
			args[i].spin_tm = spin_tm;
			args[i].id = i;
			args[i].loops = loops;
			args[i].adder = adder;
			args[i].loops_str = loops_str;
			args[i].adder_str = adder_str;
			args[i].work = work;
			args[i].wrk_typ = wrk_typ;
		}
		if (doing_disk && spin_tm > 0.0) {
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
			args[i].spin_tm = spin_tm_multi;
		}
		t_start = dclock();
		printf("work= %s\n", work.c_str());

		if (!doing_disk && spin_tm_multi > 0.0) {
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
#if defined(__linux__) || defined(__APPLE__)
			pthread_barrier_wait(&mybarrier);
#else
			EnterSynchronizationBarrier(&mybarrier, 0);
#endif

			for (auto& t : threads) {
				t.join();
			}
			double tot = 0.0;
			for (unsigned i = 0; i < num_cpus; ++i) {
				tot += args[i].perf;
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
			//abcd
			printf("work= %s, threads= %d, total perf= %.3f %s\n", wrk_typs[args[i].wrk_typ].c_str(), (int)args.size(), tot, args[0].units.c_str());
		}

		t_end = dclock();
		if (loops < 100 && !doing_disk && spin_tm_multi > 0.0) {
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

