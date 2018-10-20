/* Copyright (c) 2018 Patrick Fay
 *
 * License http://opensource.org/licenses/mit-license.php MIT License
 */

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#define pid_t int
#endif

#include <iostream>
#include <time.h>
#include <stdlib.h>
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

#ifdef __linux__
#include <unistd.h>
#include <sys/syscall.h>   /* For SYS_xxx definitions */
#endif

#include "utils.h"

enum { // below enum has to be in same order as wrk_typs
	WRK_SPIN,
	WRK_MEM_BW,
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
	"disk_rd",
	"disk_wr",
	"disk_rdwr",
	"disk_rd_dir",
	"disk_wr_dir",
	"disk_rdwr_dir"
};


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
	std::string work, filename;
	double spin_tm;
	unsigned long rezult, loops, adder;
	double tm_beg, tm_end;
	int id, wrk_typ;
	char *loops_str, *adder_str;
	args_str(): spin_tm(0.0), rezult(0), loops(0), adder(0), tm_beg(0.0), tm_end(0.0), id(-1),
		wrk_typ(-1), loops_str(0), adder_str(0) {}
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

static size_t sys_getpagesize(void)
{
	size_t pg_sz = 4096; // sane? default
#ifdef __linux__
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
#ifdef __linux__
	rc = posix_memalign((void **)buf_ptr, pg_sz, use_bytes);
#else
	*buf_ptr = (char *)_aligned_malloc(use_bytes, pg_sz);
#endif
	if (rc != 0 || *buf_ptr == NULL) {
		printf("failed to malloc %" PRId64 " bytes on alignment= %d at %s %d\n",
			use_bytes, (int)pg_sz, __FILE__, __LINE__);
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
	double dura = tm_end - tm_beg;
	printf("cpu[%d]: tid= %d, beg/end= %f,%f, dura= %f, MiB/sec= %f\n",
		cpu, mygettid(), tm_beg, tm_end, dura, 1.0e-6 * (double)(bytes)/(dura));
	args[i].rezult = bytes;
	args[i].tm_beg = tm_beg;
	args[i].tm_end = tm_end;
	return 0;
}

float mem_bw(unsigned int i)
{
	char *buf;
	double tm_to_run = args[i].spin_tm;
	int cpu =  args[i].id;
	//unsigned int i;
	uint64_t ops = 0;
	uint64_t j, loops;
	uint64_t rezult = 0;
	double tm_end, tm_beg, bytes=0.0;
	int strd = (int)args[i].loops;
	int arr_sz = (int)args[i].adder;
	if (args[i].adder_str && (strstr(args[i].adder_str, "k") || strstr(args[i].adder_str, "K"))) {
		arr_sz *= 1024;
	}
	if (args[i].adder_str && (strstr(args[i].adder_str, "m") || strstr(args[i].adder_str, "M"))) {
		arr_sz *= 1024*1024;
	}
	if (i==0) {
		printf("strd= %d, arr_sz= %d, %d KB, %.4f MB\n",
			strd, arr_sz, arr_sz/1024, (double)(arr_sz)/(1024.0*1024.0));
	}
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

	switch(args[i].wrk_typ) {
		case WRK_SPIN:
			res = simd_dot0(i);
			break;
		case WRK_MEM_BW:
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
	return res;
}

int main(int argc, char **argv)
{

	char *loops_str=NULL, *adder_str= NULL;
	double t_first = dclock();
	unsigned num_cpus = std::thread::hardware_concurrency();
	double spin_tm = 2.0;
	bool doing_disk = false;
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
	printf("usage: %s tm_secs [work_type [ arg3 [ arg4 ]]]\n", argv[0]);
	printf("\twork_type: spin|mem_bw|disk_rd|disk_wr|disk_rdwr|disk_rd_dir|disk_wr_dir|disk_rdwr_dir\n");
	printf("if mem_bw: arg3 is stride in bytes. arg4 is array size in bytes\n");

	int i=1;
	if (argc > i) {
		spin_tm = atof(argv[i]);
	}
	i++; //2
	uint32_t wrk_typ = WRK_SPIN;
	std::string work = "spin";
	if (argc > i) {
		work = std::string(argv[i]);
		wrk_typ = UINT32_M1;
		for (uint32_t j=0; j < wrk_typs.size(); j++) {
			if (work == wrk_typs[j]) {
				wrk_typ = j;
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
		if (wrk_typ == WRK_MEM_BW) {
			loops = 64;
			adder = 80*1024*1024;
		}
	}
	i++; //3
	if (argc > i) {
		loops = atoi(argv[i]);
		loops_str = argv[i];
		printf("arg3 is %s\n", loops_str);
	}
	i++; //4
	if (argc > i) {
		adder = atoi(argv[i]);
		adder_str = argv[i];
		printf("arg4 is %s\n", adder_str);
	}

	args.resize(num_cpus);
	for (unsigned i=0; i < num_cpus; i++) {
		args[i].spin_tm = spin_tm;
		args[i].id = i;
		args[i].loops = loops;
		args[i].adder = adder;
		args[i].loops_str = loops_str;
		args[i].adder_str = adder_str;
		args[i].work = work;
		args[i].wrk_typ = wrk_typ;
	}
	t_start = dclock();
	dispatch_work(0);
	t_end = dclock();
	t_start = dclock();
	printf("work= %s\n", work.c_str());

	if (!doing_disk) {
		for (unsigned i = 0; i < num_cpus; ++i) {
			threads[i] = std::thread([&iomutex, i] {
				dispatch_work(i);
			});
		}

		for (auto& t : threads) {
			t.join();
		}
	}

	t_end = dclock();
	if (loops < 100 && !doing_disk) {
		std::cout << "\nExecution time on 4 CPUs: " << t_end - t_start << " secs" << std::endl;
		for (unsigned i=0; i < num_cpus; i++) {
			printf("rezult[%d]= %lu\n", i, args[i].rezult);
		}
	}
	return 0;
}

