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

static int alloc_pg_bufs(int num_bytes, char **buf_ptr)
{
	size_t pg_sz = sys_getpagesize();
	size_t use_bytes = num_bytes;
	if ((use_bytes % pg_sz) != 0) {
		use_bytes += use_bytes % pg_sz;
	}
	int rc;
#ifdef __linux__
	rc = posix_memalign((void **)buf_ptr, pg_sz, use_bytes);
#else
	*buf_ptr = (char *)_aligned_malloc(use_bytes, pg_sz);
#endif
	if (rc != 0) {
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
// windows needs CreateFile FILE_FLAG_NO_BUFFERING. See https://github.com/ronomon/direct-iok
// I haven't added this yet for windows... so the dir rtns are the same as not-dir on windows at the moment
#define O_DIRECT 0
#endif

static size_t disk_write_dir(int myi, const char *filename, int ar_sz, int loops)
{
	int fd, val=0;
	char *buf;

	int pg_sz = alloc_pg_bufs(ar_sz, &buf);
	fd = open(filename, O_WRONLY|O_CREAT|O_BINARY|O_DIRECT, 0x666);
	if (!fd)
	{
		printf("Unable to open file %s at %s %d\n", filename, __FILE__, __LINE__);
		exit(1);
	}
	size_t byts = 0;
	int num_pages = ar_sz / pg_sz;
	for (int i=0; i < loops; i++) {
		for (int j= 0; j < ar_sz; j+= 256) {
			buf[j] += j + val;
		}
		val++;
		for (int j=0; j < num_pages; j += 16) {
			byts += write(fd, buf+(j*pg_sz), 16*pg_sz);
		}
	}
	args[myi].rezult = val;
	close(fd);
	return byts;
}

static size_t disk_read_dir(int myi, const char *filename, int ar_sz, int loops)
{
	int fd, val=0;
	char *buf;
	size_t ret;

	int pg_sz = alloc_pg_bufs(ar_sz, &buf);
	fd = open(filename, O_RDONLY | O_BINARY | O_DIRECT, 0);
	if (!fd)
	{
		printf("Unable to open file %s at %s %d\n", filename, __FILE__, __LINE__);
		exit(1);
	}
	int num_pages = ar_sz / pg_sz;
	size_t byts = 0;
	for (int i=0; i < loops; i++) {
		for (int j=0; j < num_pages; j += 16) {
			byts += read(fd, buf+(j*pg_sz), 16*pg_sz);
		}
		for (int j= 0; j < ar_sz; j+= 256) {
			val += buf[j];
		}
	}
	close(fd);
	args[myi].rezult = val;
	return byts;
}

static int disk_write(int myi, const char *filename, int ar_sz, int loops, bool do_flush)
{
	int counter;
	FILE *fp;
	int val=0;
	char *buf;

	int pg_sz = alloc_pg_bufs(ar_sz, &buf);
	fp = fopen(filename,"wb");
	if (!fp)
	{
		printf("Unable to open file %s at %s %d\n", filename, __FILE__, __LINE__);
		exit(1);
	}
	int num_pages = ar_sz / pg_sz;
	for (int i=0; i < loops; i++) {
		for (int j= 0; j < ar_sz; j+= 512) {
			buf[j] += j + val;
		}
		val++;
		for (int j=0; j < num_pages; j++) {
      			fwrite(buf+(j*pg_sz), pg_sz, 1, fp);
		}
	}
	//fflush(fp);
	//fsync(fileno(fp));
	args[myi].rezult = val;
	fclose(fp);
	return 0;
}

static int disk_read(int myi, const char *filename, int ar_sz, int loops)
{
	int counter;
	FILE *fp;
	int val=0;
	char *buf;
	size_t ret;

	int pg_sz = alloc_pg_bufs(ar_sz, &buf);
	fp = fopen(filename,"rb");
	if (!fp)
	{
		printf("Unable to open file %s at %s %d\n", filename, __FILE__, __LINE__);
		exit(1);
	}
	//setvbuf(fp, NULL, _IONBF, 0);
	int num_pages = ar_sz / pg_sz;
	for (int i=0; i < loops; i++) {
		for (int j=0; j < num_pages; j++) {
      			ret = fread(buf+(j*pg_sz), pg_sz, 1, fp);
		}
		for (int j= 0; j < ar_sz; j+= 512) {
			val += buf[j];
		}
	}
	fclose(fp);
	args[myi].rezult = val;
	return val;
}

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

float disk_wr(unsigned int i)
{
	double tm_to_run = args[i].spin_tm;
	int cpu =  args[i].id;
	//unsigned int i;
	uint64_t ops = 0;
	uint64_t j, loops;
	uint64_t rezult = 0;
	double tm_end, tm_beg, bytes=0.0;
	std::string filename = "disk_tst_" + std::to_string(i);
	loops = args[i].loops;
	int arr_sz = 1024*1024;
	uint64_t adder = args[i].adder;
	loops = 100;
	bool do_flush = false;
	tm_end = tm_beg = dclock();
	while((tm_end - tm_beg) < tm_to_run) {
		disk_write(i, filename.c_str(), arr_sz, loops, do_flush);
		bytes += arr_sz * loops;
		tm_end = dclock();
	}
	double dura = tm_end - tm_beg;
	printf("cpu[%d]: tid= %d, beg/end= %f,%f, dura= %f, MiB/sec= %f\n",
		cpu, mygettid(), tm_beg, tm_end, dura, 1.0e-6 * (double)(bytes)/(dura));
	args[i].rezult = rezult;
	return 0;
}

float disk_rd(unsigned int i)
{
	double tm_to_run = args[i].spin_tm;
	int bops=0, cpu =  args[i].id;
	//unsigned int i;
	uint64_t ops = 0;
	uint64_t j, loops;
	uint64_t rezult = 0;
	double tm_end, tm_beg, bytes=0.0;
	std::string filename = "disk_tst_" + std::to_string(i);
	loops = args[i].loops;
	int arr_sz = 1024*1024;
	uint64_t adder = args[i].adder;
	loops = 100;
	tm_end = tm_beg = dclock();
	while((tm_end - tm_beg) < tm_to_run) {
		bops += disk_read(i, filename.c_str(), arr_sz, loops);
		bytes += arr_sz * loops;
		tm_end = dclock();
	}
	double dura = tm_end - tm_beg;
	printf("cpu[%d]: tid= %d, beg/end= %f,%f, dura= %f, MiB/sec= %f\n",
		cpu, mygettid(), tm_beg, tm_end, dura, 1.0e-6 * (double)(bytes)/(dura));
	if (bops < 0) {
		printf("bops= %d at %s %d\n", bops, __FILE__, __LINE__);
	}
	args[i].rezult = rezult;
	return 0;
}

float disk_rdwr(unsigned int i)
{
	double tm_to_run = args[i].spin_tm;
	int bops=0, cpu =  args[i].id;
	//unsigned int i;
	uint64_t ops = 0;
	uint64_t j, loops;
	uint64_t rezult = 0;
	double tm_end, tm_beg, bytes=0.0;
	std::string filename = "disk_tst_" + std::to_string(i);
	loops = args[i].loops;
	int arr_sz = 1024*1024;
	uint64_t adder = args[i].adder;
	loops = 100;
	bool do_flush= true;
	tm_end = tm_beg = dclock();
	while((tm_end - tm_beg) < tm_to_run) {
		disk_write(i, filename.c_str(), arr_sz, loops, do_flush);
		bops += disk_read(i, filename.c_str(), arr_sz, loops);
		bytes += 2.0 * arr_sz * loops;
		tm_end = dclock();
	}
	double dura = tm_end - tm_beg;
	printf("cpu[%d]: tid= %d, beg/end= %f,%f, dura= %f, MiB/sec= %f\n",
		cpu, mygettid(), tm_beg, tm_end, dura, 1.0e-6 * (double)(bytes)/(dura));
	if (bops < 0) {
		printf("bops= %d at %s %d\n", bops, __FILE__, __LINE__);
	}
	args[i].rezult = rezult;
	return 0;
}

float disk_wr_dir(unsigned int i)
{
	double tm_to_run = args[i].spin_tm;
	int bops=0, cpu =  args[i].id;
	//unsigned int i;
	uint64_t ops = 0;
	uint64_t j, loops;
	uint64_t rezult = 0;
	double tm_end, tm_beg, bytes=0.0;
	std::string filename = "disk_tst_" + std::to_string(i);
	loops = args[i].loops;
	int arr_sz = 1024*1024;
	uint64_t adder = args[i].adder;
	loops = 100;
	bool do_flush= true;
	tm_end = tm_beg = dclock();
	while((tm_end - tm_beg) < tm_to_run) {
		bytes += disk_write_dir(i, filename.c_str(), arr_sz, loops);
		tm_end = dclock();
	}
	double dura = tm_end - tm_beg;
	printf("cpu[%d]: tid= %d, beg/end= %f,%f, dura= %f, MiB/sec= %f\n",
		cpu, mygettid(), tm_beg, tm_end, dura, 1.0e-6 * (double)(bytes)/(dura));
	if (bops < 0) {
		printf("bops= %d at %s %d\n", bops, __FILE__, __LINE__);
	}
	args[i].rezult = rezult;
	return 0;
}

float disk_rd_dir(unsigned int i)
{
	double tm_to_run = args[i].spin_tm;
	int bops=0, cpu =  args[i].id;
	//unsigned int i;
	uint64_t ops = 0;
	uint64_t j, loops;
	uint64_t rezult = 0;
	double tm_end, tm_beg, bytes=0.0;
	std::string filename = "disk_tst_" + std::to_string(i);
	loops = args[i].loops;
	int arr_sz = 1024*1024;
	uint64_t adder = args[i].adder;
	loops = 100;
	bool do_flush= true;
	tm_end = tm_beg = dclock();
	while((tm_end - tm_beg) < tm_to_run) {
		bytes += disk_read_dir(i, filename.c_str(), arr_sz, loops);
		tm_end = dclock();
	}
	double dura = tm_end - tm_beg;
	printf("cpu[%d]: tid= %d, beg/end= %f,%f, dura= %f, MiB/sec= %f\n",
		cpu, mygettid(), tm_beg, tm_end, dura, 1.0e-6 * bytes/dura);
	args[i].rezult = rezult;
	return 0;
}

float disk_rdwr_dir(unsigned int i)
{
	double tm_to_run = args[i].spin_tm;
	int bops=0, cpu =  args[i].id;
	//unsigned int i;
	uint64_t ops = 0;
	uint64_t j, loops;
	uint64_t rezult = 0;
	double tm_end, tm_beg, bytes=0.0;
	std::string filename = "disk_tst_" + std::to_string(i);
	loops = args[i].loops;
	int arr_sz = 1024*1024;
	uint64_t adder = args[i].adder;
	loops = 100;
	tm_end = tm_beg = dclock();
	while((tm_end - tm_beg) < tm_to_run) {
		bytes += disk_write_dir(i, filename.c_str(), arr_sz, loops);
		bytes += disk_read_dir(i, filename.c_str(), arr_sz, loops);
		tm_end = dclock();
	}
	double dura = tm_end - tm_beg;
	printf("cpu[%d]: tid= %d, beg/end= %f,%f, dura= %f, MiB/sec= %f\n",
		cpu, mygettid(), tm_beg, tm_end, dura, 1.0e-6 * (double)(bytes)/(dura));
	if (bops < 0) {
		printf("bops= %d at %s %d\n", bops, __FILE__, __LINE__);
	}
	args[i].rezult = rezult;
	return 0;
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
	} else if (args[i].work == "disk_wr") {
		res = disk_wr(i);
	} else if (args[i].work == "disk_rd") {
		res = disk_rd(i);
	} else if (args[i].work == "disk_rdwr") {
		res = disk_rdwr(i);
	} else if (args[i].work == "disk_rd_dir") {
		res = disk_rd_dir(i);
	} else if (args[i].work == "disk_wr_dir") {
		res = disk_wr_dir(i);
	} else if (args[i].work == "disk_rdwr_dir") {
		res = disk_rdwr_dir(i);
	}
	return res;
}

int main(int argc, char **argv)
{

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

	int i=1;
	if (argc > i) {
		spin_tm = atof(argv[i]);
	}
	i++; //2
	std::string work = "spin";
	if (argc > i) {
		work = std::string(argv[i]);
		if (work != "spin" && work != "mem_bw" && work != "disk_wr" && work != "disk_rd" &&
			work != "disk_rdwr" && work != "disk_rdwr_dir" &&
			work != "disk_rd_dir" && work != "disk_wr_dir") {
			printf("supported work types = spin, mem_bw, disk_w', disk_rd, disk_rdwr, disk_rd_dir, disk_wr_dir, disk_rdwr_dir at %s %d\n", __FILE__, __LINE__);
			exit(1);
		}
		if (work == "disk_wr" || work == "disk_rd" || work == "disk_rdwr" || work == "disk_rdwr_dir" ||
			work == "disk_wr_dir" || work == "disk_rd_dir") {
			doing_disk = true;
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

