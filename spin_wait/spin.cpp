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
#include <sys/time.h>
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
#include <signal.h>
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
#include <numa.h>
#include <unistd.h>
#include <sys/syscall.h>   /* For SYS_xxx definitions */
//#include <getcpu.h>
#endif
#ifdef _WIN32
#include <intrin.h>
#pragma intrinsic(__rdtsc)
#pragma intrinsic(__rdtscp)
#else
#ifdef __x86_64__
    // do x64 stuff
#include <x86intrin.h>
#elif __aarch64__
    // do arm stuff

#endif
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
std::vector <int> cpu_list;
std::vector <int> mem_list;
std::vector <int> membind_list;
std::vector <double> freq_fctr;

enum { // below enum has to be in same order as wrk_typs
        WRK_SPIN,
        WRK_SPIN_FAST,
        WRK_FREQ2,
        WRK_FREQ,
        WRK_FREQ_SML,
        WRK_FREQ_SML2,
        WRK_MEM_LAT,
        WRK_MEM_BW,
        WRK_MEM_BW_RDWR,
        WRK_MEM_BW_2RD,
        WRK_MEM_BW_2RDWR,
        WRK_MEM_BW_3RDWR,
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
        "spin_fast",
        "freq2",
        "freq",
        "freq_sml",
        "freq_sml2",
        "mem_lat",
        "mem_bw",
        "mem_bw_rdwr",
        "mem_bw_2rd",
        "mem_bw_2rdwr",
        "mem_bw_3rdwr",
        "mem_bw_remote",
        "mem_bw_2rd2wr",
        "disk_rd",
        "disk_wr",
        "disk_rdwr",
        "disk_rd_dir",
        "disk_wr_dir",
        "disk_rdwr_dir"
};

struct sla_str {
        int level, warn, critical;
        sla_str(): level(-1), warn(-1), critical(-1) {}
};

struct options_str {
        int verbose, help, wrk_typ, cpus, clock, nopin, yield, huge_pages, ping_pong_iter;
        std::string work, phase, bump_str, size_str, loops_str, filename;
        uint64_t bump, size, loops;
        double spin_tm, spin_tm_multi, ping_pong_run_secs, ping_pong_sleep_secs;
        std::vector <sla_str> sla;
        options_str(): verbose(0), help(0), wrk_typ(-1), cpus(-1), clock(CLOCK_USE_SYS), nopin(0),
                yield(0), huge_pages(0), ping_pong_iter(-1), bump(0), size(0), loops(0), spin_tm(2.0), spin_tm_multi(0),
                ping_pong_run_secs(0), ping_pong_sleep_secs(0)  {}

} options;

#ifdef __x86_64__
    // do x64 stuff
#include <x86intrin.h>
    // do arm stuff
#endif
static void my_getcpu(unsigned int *cpu)
{
#ifdef __x86_64__
   __rdtscp(cpu);
#elif __aarch64__
   int rc;
   unsigned int node;
   *cpu = mygetcpu();
#endif
        return;
}
#if __aarch64__
static uint64_t get_arm_cyc(void)
{
  uint64_t tsc;
  asm volatile("mrs %0, cntvct_el0" : "=r" (tsc));
  //asm volatile("mrs %0, pmcr_el0" : "=r" (tsc));
  return tsc;
}
    void readticks(unsigned int *result)
    {
      struct timeval t;
      unsigned int cc;
      unsigned int val;
      static int enabled=0;
      if (!enabled) {
               // program the performance-counter control-register:
             asm volatile("msr pmcr_el0, %0" : : "r" (17));
             //enable all counters
             asm volatile("msr PMCNTENSET_EL0, %0" : : "r" (0x8000000f));
            //clear the overflow 
            asm volatile("msr PMOVSCLR_EL0, %0" : : "r" (0x8000000f));
             enabled = 1;
      }
      //read the coutner value
      asm volatile("mrs %0, PMCCNTR_EL0" : "=r" (cc));
      //gettimeofday(&t,(struct timezone *) 0);
      //result[0] = cc;
      //result[1] = t.tv_usec;
      //result[2] = t.tv_sec;
    }
#define mfcp(rn)    ({u32 rval = 0U; __asm__ __volatile__( "mrc " rn "\n" : "=r" (rval)); rval; })
static inline uint32_t get_arm_cyc2(void)
{
  uint32_t cc = 0;
  //asm volatile ("mrc p15, 0, %0, c9, c13, 0":"=r" (cc));
    //asm volatile ("mrc CNTPCT_EL0" : "=r" (cc));
  readticks(&cc);
  //cc = mfcp(CNTPCT_EL0);
  return cc;
}
#endif

static uint64_t get_tsc_and_cpu(unsigned int *cpu)
{
        uint64_t tsc;
#ifdef __x86_64__
        tsc = __rdtscp(cpu);
#elif __aarch64__
        //my_getcpu(cpu);
        *cpu = mygetcpu();
        tsc = get_arm_cyc();
#endif
        return tsc;
}

#define MSR_APERF 0xE8

static inline uint64_t rdtscp( uint32_t *aux )
{
    uint64_t rax,rdx;
    *aux = 0;
#ifdef __x86_64__
    rax = __rdtscp(aux);
#elif __aarch64__
    //my_getcpu(aux);
    *aux = mygetcpu();
    rax = get_arm_cyc();
#endif

    return rax;
}

static uint64_t get_tsc_cpu_node(uint32_t *cpu, uint32_t *node) {
  uint32_t aux=0;
  uint64_t tsc = 0;
#ifdef __x86_64__
  tsc = rdtscp(&aux);
  *node = ((aux >> 12) & 0xf);
  *cpu  = (aux & 0xfff);
#elif __aarch64__
  //int rc;
  //rc = getcpu(cpu, &node);
  *cpu = mygetcpu();
  *node = 0;
  tsc = get_arm_cyc();
#endif
  return tsc;
}

// abc open_msr and read_msr based on http://web.eece.maine.edu/~vweaver/projects/rapl/rapl-read.c
static int open_msr(int core) {

    char msr_filename[128];
    int fd=-1;;

#ifdef __linux__
#ifdef __x86_64__
    snprintf(msr_filename, sizeof(msr_filename), "/dev/cpu/%d/msr", core);
    fd = open(msr_filename, O_RDONLY);
    if ( fd < 0 ) {
        if ( errno == ENXIO ) {
            if (options.verbose > 0) {
               fprintf(stderr, "rdmsr: No CPU %d\n", core);
            }
            return fd;
        } else if ( errno == EIO ) {
            if (options.verbose > 0) {
               fprintf(stderr, "rdmsr: CPU %d doesn't support MSRs\n", core);
            }
            return fd;
        } else {
            if (options.verbose > 0) {
              perror("rdmsr:open");
              fprintf(stderr,"Trying to open %s\n",msr_filename);
            }
            return fd;
        }
    }
#elif __aarch64__
    fd=-1;
#endif
#endif

    return fd;
}

static uint64_t read_msr(int fd, int which) {

    uint64_t data=0;

#ifdef __linux__
#ifdef __x86_64__
    if ( pread(fd, &data, sizeof data, which) != sizeof data ) {
        perror("rdmsr:pread");
        exit(127);
    }
#endif
#endif

    return data;
}

volatile int got_quit=0;

static void sighandler(int sig)
{
        got_quit=1;
}

static double mclk(uint64_t t0)
{
#ifdef __x86_64__
        uint64_t t1;
        double x;
        t1 = __rdtsc();
        x = (double)(t1 - t0);
        return x*ifrq;
#else
        return 0;
#endif

}

void do_trace_marker_write(std::string str)
{
#if 1
    // get a seg fault sometimes so bypass trace_marker_write for now
    return;
#endif
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

#define LIST_FOR_MEM 0
#define LIST_FOR_CPU 1
#define LIST_FOR_MEMBIND 2


void pin(int cpu, int typ)
{
   if (options.nopin == 1) {
      return;
   }
#if defined(__linux__) || defined(__APPLE__)
        int i = cpu;

   if (typ == LIST_FOR_CPU && cpu_list.size() > 0 && i < cpu_list.size()) {
      if (options.verbose > 0) {
        printf("LIST_FOR_CPU pin thread %d to cpu %d at %s %d\n", i, cpu_list[i], __FILE__, __LINE__);
      }
      i = cpu_list[i];
   }
   if (typ == LIST_FOR_MEM && mem_list.size() > 0 && i < mem_list.size()) {
      if (options.verbose > 0) {
        printf("LIST_FOR_MEM pin thread %d to cpu %d at %s %d\n", i, mem_list[i], __FILE__, __LINE__);
      }
      i = mem_list[i];
   }
   if (typ == LIST_FOR_MEMBIND && membind_list.size() > 0) {
      static int did_membind = 0;
      if (did_membind == 0) {
        //did_membind = 1;
      if (options.verbose > 0) {
        printf("LIST_FOR_MEMBIND pin thread %d to cpu %d at %s %d\n", i, membind_list[i], __FILE__, __LINE__);
      }
#ifndef __APPLE__
      //struct bitmask *mask;
      //struct bitmask *mask = numa_allocate_nodemask();
      if (options.verbose > 0) {
      printf("numa_num_configured_cpus()= %d\n", numa_num_configured_cpus());
      }
      int num_nodes = numa_max_node()+1;
      struct bitmask *mask = numa_bitmask_alloc(num_nodes);

      int did_set = 0;
      for (int j=0; j < membind_list.size(); j++) {
          int k = membind_list[j];
          if (k < num_nodes) {
             numa_bitmask_setbit(mask, k);
             did_set = 1;
          } else {
            printf("membind option -m %d but node number must be less than number of numa nodes= %d\n", k, num_nodes);
          }
          //struct bitmask *numa_bitmask_setbit(struct bitmask *bmp, unsigned int n);
      }
      for (int j=0; j < mask->size; j++) {
          int k = numa_bitmask_isbitset(mask,j);
          if (k < num_nodes) {
          if (k != 0 && options.verbose > 0) {printf("membind mask bit[%d] = %d\n", j, k);}
          }
      }
      if (did_set == 1) {
         numa_set_membind(mask);
      }
#endif
      }
      return;
   }
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

#if 1
//get from utils.cpp now
double dclock2(void)
{
        struct timespec tp;
        clock_gettime(CLOCK_MONOTONIC, &tp);
        return (double)(tp.tv_sec) + 1.0e-9 * (double)(tp.tv_nsec);
}
#endif

#ifndef MYDCLOCK
#define MYDCLOCK dclock2
#endif

static double dclock3(int clock, uint64_t tsc_initial)
{
    double tm;
    if (clock == CLOCK_USE_SYS) {
        tm = MYDCLOCK();
    } else {
        tm = mclk(tsc_initial);
    }
    return tm;
}

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
        uint64_t size, bump, tsc_initial, cycles_beg, cycles_end;
        double tm_beg, tm_end, perf, dura, cpu_time, ping_tm_sleep, ping_tm_run, freq_load,
            freq_fctr_run_secs, freq_fctr_sleep_secs;
        int id, wrk_typ, clock, got_SLA, ping_iter, used_msr_cycles;
        std::vector <rt_str> rt_vec;
        char *dst;
        std::string bump_str, loops_str, size_str, units;
        args_str(): spin_tm(0.0), rezult(0), loops(0), dst(0), size(0), bump(0), tsc_initial(0),
                cycles_beg(0), cycles_end(0),
                tm_beg(0.0), tm_end(0.0), perf(0.0), dura(0.0), ping_tm_sleep(0.0), ping_tm_run(0.),
                id(-1), wrk_typ(-1), clock(CLOCK_USE_SYS),
                freq_load(0.0), freq_fctr_run_secs(0.0), freq_fctr_sleep_secs(0.0), got_SLA(false), ping_iter(-1),
                used_msr_cycles(0), cpu_time(0)  {}
};

struct phase_str {
        std::string work, filename, phase;
        double tm_end, perf, dura;
        phase_str(): tm_end(0.0), perf(0.0), dura(0.0) {}
};

uint64_t do_scale(uint64_t loops, uint64_t rez, uint64_t &ops)
{
        double rezult = rez;
        double x = rezult+1.0;
        for (uint64_t j = 0; j < loops; j++) {
                rezult += x * 1.00011;
                rezult -= x * 1.00012;
                x += 1.0;
        }
        ops += loops;
        return rezult;
}

uint64_t do_scale_fast(uint64_t loops, uint64_t rez, uint64_t &ops)
{
        uint64_t rezult = rez;
        for (uint64_t j = 0; j < loops; j++) {
                rezult += j;
        }
        ops += loops;
        return rezult;
}


std::vector <args_str> args;

static int array_write(char *buf, size_t ar_sz, int strd)
{
        int res=0;
        for (size_t i=0; i < ar_sz-strd; i += strd) {
                buf[i] = (char)(0xff & i);
        }
        return res;
}

static int array_read_write(char *buf, size_t ar_sz, int strd)
{
        int res=0;
        for (size_t i=0; i < ar_sz-strd; i += strd) {
                buf[i] += i;
        }
        return res;
}

static int array_2read_write(char *dst, char *src, size_t ar_sz, int strd)
{
        int res=0;
        for (size_t i=0; i < ar_sz-strd; i += strd) {
                dst[i] += src[i];
        }
        return res;
}

static int array_3read_write(char *dst, char *src, char *src2, size_t ar_sz, int strd)
{
        int res=0;
        for (size_t i=0; i < ar_sz-strd; i += strd) {
                dst[i] += src[i] + src2[i];
        }
        return res;
}

static int array_2read_2write(char *dst, char *src, size_t ar_sz, int strd)
{
        int res=0;
        for (size_t i=0; i < ar_sz-strd; i += strd) {
                dst[i] += res;
                src[i] += res;
                res++;
        }
        return res;
}

static int array_2read(char *dst, char *src, size_t ar_sz, int strd)
{
        int res=0;
        for (size_t i=0; i < ar_sz-strd; i += strd) {
                res += dst[i] + src[i];
        }
        return res;
}

static int array_read(char *buf, size_t ar_sz, int strd)
{
        int res=0;
        for (size_t i=0; i < ar_sz-strd; i += strd) {
                res += buf[i];
        }
        return res;
}

static int array_read_lat(char *buf, size_t ar_sz, int strd)
{
        int res=0;
        for (size_t i=0; i < ar_sz-strd; i += strd) {
                res += buf[i];
        }
        return res;
}

static size_t mysys_getpagesize(void)
{
        size_t pg_sz = 4096; // sane? default
#if defined(__linux__) || defined(__APPLE__)
        pg_sz = getpagesize();
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
        size_t pg_sz = mysys_getpagesize();
        size_t use_bytes = num_bytes;
        //printf("pg_sz= %d at %s %d\n", (int)pg_sz, __FILE__, __LINE__);
        if ((use_bytes % pg_sz) != 0) {
                int num_pgs = use_bytes/pg_sz;
                use_bytes = (num_pgs+1) * pg_sz;
        }
        if ((use_bytes % (pg_chunks * pg_sz)) != 0) {
                int num_pgs = use_bytes/(pg_chunks*pg_sz);
                use_bytes = (num_pgs+1) * (pg_chunks*pg_sz);
        }
        int rc=0;
        use_bytes = pg_chunks * pg_sz;
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

#ifndef O_SYNC
#define O_SYNC 0
#endif

static size_t disk_write_dir(int myi, size_t ar_sz)
{
        char *buf;
        size_t byts = 0;
        int val=0, loops = args[myi].loops, pg_chunks=16;
        if ((int)args[myi].bump > 0) {
                pg_chunks = args[myi].bump;
        }

        int pg_sz = alloc_pg_bufs(ar_sz, &buf, pg_chunks);
#ifdef _WIN32
        HANDLE fd = CreateFile(args[myi].filename.c_str(), GENERIC_WRITE,
                0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_NO_BUFFERING, NULL);
#else
        mode_t myumask = umask(0x777);
        //int fd = open(args[myi].filename.c_str(), O_WRONLY|O_CREAT|O_BINARY|O_DIRECT, 0x666);
        int fd = open(args[myi].filename.c_str(), O_WRONLY|O_CREAT|O_BINARY|O_DIRECT|O_SYNC, 0);
        chmod(args[myi].filename.c_str(), 0000660);
#endif
        if (!fd)
        {
                printf("Unable to open file %s at %s %d\n", args[myi].filename.c_str(), __FILE__, __LINE__);
                exit(1);
        }
#if defined(__APPLE__)
        fcntl(fd, F_NOCACHE, 1); // disable caching
#endif
        int num_pages = options.size / pg_sz;
        for (int i=0; i < loops; i++) {
#if 0
                for (int j= 0; j < ar_sz; j+= 256) {
                        buf[j] += j + val;
                }
#endif
                val++;
                if (i > 0) {
#ifndef _WIN32
                        lseek(fd, 0, SEEK_SET);
#endif
                }

                for (int j=0; j < num_pages; j += pg_chunks) {
#ifdef _WIN32
                        DWORD dwWritten=0;
                        WriteFile(fd, buf, pg_chunks*pg_sz, &dwWritten, NULL);
                        byts += dwWritten;
#else
                        ssize_t bdone = write(fd, buf, pg_chunks*pg_sz);
                        if (bdone <= 0) {
                                break;
                        }
                        byts += bdone;
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

static size_t disk_read_dir(int myi, size_t ar_sz)
{
        int val=0, pg_chunks=16;
        if ((int)args[myi].bump > 0) {
                pg_chunks = args[myi].bump;
        }
        char *buf;
        size_t ret;
        size_t byts = 0;
        int loops = args[myi].loops;

        int pg_sz = alloc_pg_bufs(ar_sz, &buf, pg_chunks);
#ifdef _WIN32
        HANDLE fd = CreateFile(args[myi].filename.c_str(), GENERIC_READ,
                0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_NO_BUFFERING, NULL);
#else
        int fd = open(args[myi].filename.c_str(), O_RDONLY | O_BINARY | O_DIRECT | O_SYNC, 0);
#endif
        if (!fd)
        {
                printf("Unable to open file %s at %s %d\n", args[myi].filename.c_str(), __FILE__, __LINE__);
                exit(1);
        }
#if defined(__APPLE__)
        fcntl(fd, F_NOCACHE, 1); // disable caching
#endif
        int num_pages = options.size / pg_sz;
        for (int i=0; i < loops; i++) {
                for (int j=0; j < num_pages; j += pg_chunks) {
#ifdef _WIN32
                        DWORD dwGot=0;
                        ReadFile(fd, buf, pg_chunks*pg_sz, &dwGot, NULL);
                        byts += dwGot;
#else
                        ssize_t bdone = read(fd, buf, pg_chunks*pg_sz);
                        if (bdone <= 0) {
                                break;
                        }
                        byts += bdone;
#endif
                }
                //for (int j= 0; j < ar_sz; j+= 256) {
                //      val += buf[j];
                //}
        }
#ifdef _WIN32
        CloseHandle(fd);
#else
        close(fd);
#endif
        args[myi].rezult = val;
        return byts;
}

static size_t disk_write(int myi, size_t ar_sz)
{
        FILE *fp;
        int val=0, pg_chunks=16;
        if ((int)args[myi].bump > 0) {
                pg_chunks = args[myi].bump;
        }
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
        int num_pages = options.size / pg_sz;
        for (int i=0; i < loops; i++) {
#if 0
                for (int j= 0; j < ar_sz; j+= 512) {
                        buf[j] += j + val;
                }
#endif
                val++;
                for (int j=0; j < num_pages; j += pg_chunks) {
                        byts += fwrite(buf, pg_chunks*pg_sz, 1, fp);
                }
                if (got_quit == 1) {
                        break;
                }
        }
        byts *= pg_chunks * pg_sz;
        args[myi].rezult = val;
        fclose(fp);
        return byts;
}

static size_t disk_read(int myi, size_t ar_sz)
{
        FILE *fp;
        int val=0, pg_chunks=16;
        if ((int)args[myi].bump > 0) {
                pg_chunks = args[myi].bump;
        }
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
        int num_pages = options.size / pg_sz;
        for (int i=0; i < loops; i++) {
                for (int j=0; j < num_pages; j += pg_chunks) {
                        byts += fread(buf, pg_chunks*pg_sz, 1, fp);
                }
#if 0
                for (int j= 0; j < ar_sz; j+= 512) {
                        val += buf[j];
                }
#endif
                if (got_quit == 1) {
                        break;
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
        if (options.filename.size() > 0) {
                args[i].filename = options.filename + "_" + std::to_string(i);
        } else {
                args[i].filename = "disk_tst_" + std::to_string(i);
        }
        tm_end = tm_beg = MYDCLOCK();
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
                tm_end = MYDCLOCK();
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
        char *dst, *src, *src2;
        double tm_to_run = args[i].spin_tm;
        int cpu =  args[i].id;
        //unsigned int i;
        uint64_t ops = 0;
        uint64_t j, loops = args[i].loops;
        uint64_t rezult = 0;
        double tm_end, tm_beg, bytes=0.0;
        int strd = (int)args[i].bump;
        size_t arr_sz = (size_t)args[i].size;
        //fprintf(stderr, "got to %d\n", __LINE__);
        int styp = LIST_FOR_MEM;
        if (membind_list.size() > 0) { styp = LIST_FOR_MEMBIND;}
        pin(i, styp);
        my_msleep(0); // necessary? in olden times it was.
        if (i==0) {
                printf("strd= %d, arr_sz= %.0f, %.0f KB, %.4f MB\n",
                        strd, (double)arr_sz, (double)(arr_sz/1024), (double)(arr_sz)/(1024.0*1024.0));
        }
        if (options.huge_pages) {
                size_t algn = 2*1024*1024;
                size_t hsz = arr_sz;
                size_t ck = hsz/algn;
                if ((ck*algn) != hsz) {
                        hsz = (ck+1)*algn;
                }
#if defined(__linux__) || defined(__APPLE__)
                int rch = posix_memalign((void **)&dst, algn, hsz);
#else
                dst = (char *)_aligned_malloc(hsz, algn);
#endif
                if (rch != 0) {
                        printf("posix_memalign malloc for dst huge_page array failed. Bye at %s %d\n", __FILE__, __LINE__);
                }
        } else {
                dst = (char *)malloc(arr_sz);
        }
        array_write(dst, arr_sz, strd);
        if (args[i].wrk_typ == WRK_MEM_BW_2RDWR ||
            args[i].wrk_typ == WRK_MEM_BW_3RDWR ||
                args[i].wrk_typ == WRK_MEM_BW_2RD ||
                args[i].wrk_typ == WRK_MEM_BW_2RD2WR) {
                char *trgt;
                int ido, iend = (args[i].wrk_typ == WRK_MEM_BW_3RDWR ? 2 : 1);
                for (ido=0; ido < iend; ido++) {
                if (options.huge_pages) {
                        size_t algn = 2*1024*1024;
                        size_t hsz = arr_sz;
                        size_t ck = hsz/algn;
                        if ((ck*algn) != hsz) {
                                hsz = (ck+1)*algn;
                        }
#if defined(__linux__) || defined(__APPLE__)
                        int rch = posix_memalign((void **)&trgt, algn, hsz);
#else
                        trgt = (char *)_aligned_malloc(hsz, algn);
#endif
                        if (rch != 0) {
                                printf("posix_memalign malloc for trgt huge_page array failed. Bye at %s %d\n", __FILE__, __LINE__);
                        }
                } else {
                        trgt = (char *)malloc(arr_sz);
                }
                array_write(trgt, arr_sz, strd);
                if (ido == 0) {
                        src = trgt;
                } else {
                        src2 = trgt;
                }
                }
        }
        bool do_loop = true;
        pin(i, LIST_FOR_CPU);
        my_msleep(0); // necessary? in olden times it was.
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
                        pin(cpu_to_switch_to, -1);
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

        tm_end = tm_beg = MYDCLOCK();
        int iters=0, adj_loops=0;
        if (loops == 0) {
          loops = 1;
          adj_loops = 1;
        }
        double tm_prev;
        if (!do_loop) {
                args[i].dura = 0;
                args[i].tm_end = tm_end;
                args[i].units = "GB/sec";
                args[i].perf = 0.0;
                return 0.0;
        }
        double loop_bytes = 0.0, loop_beg_time = 0.0, tot_time=0.0;
        while((tm_end - tm_beg) < tm_to_run) {
                if (got_quit == 1) { break; }
                loop_bytes = 0.0;
                loop_beg_time  = tm_end;
#if 1
                for (int ii=0; ii < loops; ii++)
                {
#endif
                if (got_quit == 1) { break; }
                if (args[i].wrk_typ == WRK_MEM_BW) {
                        rezult += array_read(dst, arr_sz, strd);
                        loop_bytes += arr_sz;
                }
                else if (args[i].wrk_typ == WRK_MEM_LAT) {
                        rezult += array_read_lat(dst, arr_sz, strd);
                        loop_bytes += arr_sz;
                }
                else if (args[i].wrk_typ == WRK_MEM_BW_RDWR) {
                        rezult += array_read_write(dst, arr_sz, strd);
                        loop_bytes += 2*arr_sz;
                }
                else if (args[i].wrk_typ == WRK_MEM_BW_2RD) {
                        rezult += array_2read(dst, src, arr_sz, strd);
                        loop_bytes += 2*arr_sz;
                }
                else if (args[i].wrk_typ == WRK_MEM_BW_2RDWR) {
                        rezult += array_2read_write(dst, src, arr_sz, strd);
                        loop_bytes += 3*arr_sz;
                }
                else if (args[i].wrk_typ == WRK_MEM_BW_3RDWR) {
                        rezult += array_3read_write(dst, src, src2, arr_sz, strd);
                        loop_bytes += 4*arr_sz;
                }
                else if (args[i].wrk_typ == WRK_MEM_BW_REMOTE) {
                        rezult += array_read(dst, arr_sz, strd);
                        loop_bytes += arr_sz;
                }
                else if (args[i].wrk_typ == WRK_MEM_BW_2RD2WR) {
                        rezult += array_2read_2write(dst, src, arr_sz, strd);
                        loop_bytes += 4*arr_sz;
                }
#if 1
                }
#endif
                tm_end = MYDCLOCK();
                if ((tm_end - tm_beg) < tm_to_run) {
                        bytes += loop_bytes;
                        tot_time += tm_end - loop_beg_time;;
                }
#if 0
                // try to reduce the time spend in MYDCLOCK()
                if (adj_loops == 1 && ++iters > 2) {
                        if ((tm_end - tm_prev) < 0.01) {
                                loops += 10;
                        }
                        iters = 3;
                }
                tm_prev = tm_end;
#endif
                if (got_quit != 0) {
                        break;
                }
        }

        double dura2, dura;
        //dura = tm_end - tm_beg;
        dura = tot_time;
        dura2 = dura;
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
//#define D1 " ror $2, %%eax;"
//#define D1 " movl $2, %%eax;"
#ifdef __x86_64__
#define D1 " rorl $2, %%eax;"
#elif __aarch64__
#define D1 "add     x0, x0, 1;"
#endif
//#define D1 " lea (%%eax),%%eax;"
//#define D1 " nop;"
//#define D1 " andl $1, %%eax;"
//#define D1 " rcll $1, %%eax;"
#define D10 D1 D1 D1 D1 D1  D1 D1 D1 D1 D1
#define D100 D10 D10 D10 D10 D10  D10 D10 D10 D10 D10
#define D1000 D100 D100 D100 D100 D100  D100 D100 D100 D100 D100
#define D10000 D1000 D1000 D1000 D1000 D1000  D1000 D1000 D1000 D1000 D1000
#define D100000 D10000 D10000 D10000 D10000 D10000  D10000 D10000 D10000 D10000 D10000
#define D40000 D10000 D10000 D10000 D10000
#define D1000000 D100000 D100000 D100000 D100000 D100000  D100000 D100000 D100000 D100000 D100000

#ifdef __x86_64__
#define C1 " rorl $2, %%ecx; rorl $2, %%eax;"
#elif __aarch64__
#define C1 "add     w0, w0, 1;"
#endif
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
        int iiloops = 100, iters = 0;
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
        double my_freq_fctr = 1.0, freq_fctr_imx=0.0, freq_fctr_imx_sub=0.0;

        int msr_fd = -1;
        uint32_t cpu_beg, cpu_end, nd_beg, nd_end;
        uint64_t tsc_beg=0, tsc_end=0;
        uint64_t cyc_beg=0, cyc_end=0;

        pin(i, LIST_FOR_CPU);
        do_barrier_wait();
        uint64_t tsc_initial = get_tsc_and_cpu(&cpu_initial);
        args[i].tsc_initial = tsc_initial;

        //abc 
        tsc_beg = get_tsc_cpu_node(&cpu_beg, &nd_beg);
#ifdef __x86_64__
        msr_fd = open_msr(cpu_beg);
        if (msr_fd >= 0) {
          cyc_beg = read_msr(msr_fd, MSR_APERF);
        }
#elif __aarch64__
        //cyc_beg = get_arm_cyc();
#endif
        tm_end = tm_beg = MYDCLOCK();

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
                tm_end = MYDCLOCK();
                }
#else
                //tm_endt = tm_begt = dclock3(clock, tsc_initial);
                tm_endt = tm_begt = dclock2();
                tm_prevt = tm_begt;
                double ping_tm_beg, ping_tm_end;
#define DO_RUN 0
#define DO_SLP 1
                int run_or_sleep = DO_RUN;
                double tm_prv;
		//int reps = 10000;
		int reps = 1000;
                while((tm_endt - tm_begt) < tm_to_run) {
                    did_iters++;
                    if (run_or_sleep == DO_RUN) {
                        for (ii=0; ii < imx; ii++) {
#ifdef _WIN32
                            b = win_rorl(b); // 10000 inst
#else
#ifdef __x86_64__
                            asm ( "movl %1, %%eax;"
                                    ".align 4;"
                                    D1000
                                    /*D1000*/
                                    " movl %%eax, %0;"
                                    :"=r"(b) /* output */
                                    :"r"(a)  /* input */
                                    :"%eax"  /* clobbered reg */
                                );
#elif __aarch64__
                            asm ( D1000 : : : "x0");
                             //a += b;
#endif
#endif
                            a |= b;
                        }
                    }
                    tm_prv = tm_endt;
                    tm_endt = dclock2();
                    //tm_endt = dclock3(clock, tsc_initial);
#ifdef __linux__
                    if (got_quit == 1) {
                        break;
                    }
#endif
                }
#endif
                xend = get_cputime();
                xcumu += xend-xbeg;
                xinst += (double)(reps) * (double)imx;
                xinst *= did_iters;
                //printf("xcumu tm= %.3f, xinst= %g freq= %.3f GHz, i= %d, wrk_typ= %d\n", xcumu, xinst, 1.0e-9 * xinst/xcumu, i, args[i].wrk_typ);
                ops = (uint64_t)(xinst);
                tm_end = dclock2();
        }

        if (args[i].wrk_typ == WRK_FREQ_SML2) {
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
                tm_end = MYDCLOCK();
                }
#else
                tm_endt = tm_begt = dclock3(clock, tsc_initial);
                tm_prevt = tm_begt;
                double ping_tm_beg, ping_tm_end;
#define DO_RUN 0
#define DO_SLP 1
                int run_or_sleep = DO_RUN;
                int freq_fctr_sz = (int)freq_fctr.size();
                double freq_fctr_run_secs=0.0, freq_fctr_sleep_secs=0.0;
                if (freq_fctr_sz > 0 && freq_fctr_sz > i) {
                    my_freq_fctr = freq_fctr[i];
                }
                int ping_drvr = 0, ping_chkr = 0, ping_iter = -1;
                double ping_sleep_secs = options.ping_pong_sleep_secs;
                double ping_run_secs   = options.ping_pong_run_secs;
                int ping_sleep_ms = (int)(1000.0*options.ping_pong_sleep_secs);
                if (options.ping_pong_iter != -1 && (args.size() % 2) == 0) {
                    if ((i % 2) == 0) { run_or_sleep = DO_RUN; } else { run_or_sleep = DO_SLP; }
                    ping_iter = 0;
                    if (ping_sleep_ms <= 0) { ping_sleep_ms = 1; }
                    args[i].ping_iter = ping_iter;
                    ping_drvr = i - (i % 2);
                    ping_chkr = ping_drvr + 1;
                    printf("i= %d, ping_drvr= %d, ping_chkr= %d at %s %d\n", i, ping_drvr, ping_chkr, __FILE__, __LINE__);
                }
                if (ping_iter != -1) {
                    ping_tm_beg = tm_endt;
                }
                double tm_prv;
                while((tm_endt - tm_begt) < tm_to_run) {
                    did_iters++;
                    if (run_or_sleep == DO_RUN) {
                        for (ii=0; ii < imx; ii++) {
#ifdef _WIN32
                            b = win_rorl(b); // 10000 inst
#else
#ifdef __x86_64__
                            asm ( "movl %1, %%eax;"
                                    ".align 4;"
                                    D10000
                                    " movl %%eax, %0;"
                                    :"=r"(b) /* output */
                                    :"r"(a)  /* input */
                                    :"%eax"  /* clobbered reg */
                                );
#elif __aarch64__
                            asm ( D10000 : : : "x0");
                            // a += b;
#endif
#endif
                            a |= b;
                        }
                    } else {
                        my_msleep(ping_sleep_ms);
                    }
                    tm_prv = tm_endt;
                    tm_endt = dclock3(clock, tsc_initial);
                    if (ping_iter != -1) {
                        int new_iter = 0;
                        if (run_or_sleep == DO_RUN) {
                            freq_fctr_imx += imx;
                            freq_fctr_imx_sub += imx;
                            if ((tm_endt - ping_tm_beg) >= ping_run_secs) {
                                ping_tm_end = tm_endt;
                                ping_iter++;
                                double ping_tm_diff = (ping_tm_end - ping_tm_beg);
                                args[i].ping_tm_run += ping_tm_diff;
                                ping_tm_beg = ping_tm_end;
                                if (options.verbose > 0) {
                                    printf("i= %d, ping_iter= %d run_or_sleep= %d run_tm= %.3f frq= %.3f at %s %d\n",
                                    i, ping_iter, run_or_sleep, args[i].ping_tm_run, 1.0e-9 * (double)(freq_fctr_imx_sub)*10000.0/ping_tm_diff, __FILE__, __LINE__);
                                }
                                double t_tm = dclock3(clock, tsc_initial);
                                args[ping_drvr].ping_iter = ping_iter;
                                while((t_tm - tm_begt) < tm_to_run && got_quit == 0) {
                                    if ( args[ping_chkr].ping_iter == ping_iter) {
                                        ping_tm_beg = t_tm;
                                        run_or_sleep = DO_SLP;
                                        break;
                                    }
                                    my_msleep(5);
                                    t_tm = dclock3(clock, tsc_initial);
                                }
                            } else {
                                if ((tm_endt - tm_begt) >= tm_to_run || got_quit != 0) {
                                    ping_tm_end = tm_endt;
                                    ping_iter++;
                                    double ping_tm_diff = (ping_tm_end - ping_tm_beg);
                                    args[i].ping_tm_run += ping_tm_diff;
                                }
                            }
                        } else {
                            args[i].ping_tm_sleep += 0.001 * (double)ping_sleep_ms;
                            if ( args[ping_drvr].ping_iter != ping_iter) {
                                args[ping_chkr].ping_iter = args[ping_drvr].ping_iter;
                                ping_iter = args[ping_chkr].ping_iter;
                                //args[i].ping_tm_sleep += (ping_tm_end - ping_tm_beg);
                                ping_tm_beg = dclock3(clock, tsc_initial);
                                run_or_sleep = DO_RUN;
                                freq_fctr_imx_sub = 0.0;
                            }
                        }
                    }
                    if (freq_fctr_sz > 0) {
                        double tm_per_iter = tm_endt - tm_prv;
                        if (run_or_sleep == DO_RUN) {
                            freq_fctr_run_secs += tm_per_iter;
                            freq_fctr_imx += imx;
                            args[i].freq_fctr_run_secs += tm_per_iter;
                            if (options.verbose > 0) {
                                printf("thrd[%d].freq_fctr_run_secs= %.3f\n", i, args[i].freq_fctr_run_secs);
                            }
                        } else {
                            freq_fctr_sleep_secs += tm_per_iter;
                            args[i].freq_fctr_sleep_secs += tm_per_iter;
                        }
                        double cur_run_ratio = freq_fctr_run_secs / (freq_fctr_run_secs + freq_fctr_sleep_secs);
                        if (cur_run_ratio > my_freq_fctr) {
                            run_or_sleep = DO_SLP;
                            ping_sleep_ms = (int)(1000.0 * tm_per_iter);
                            if (ping_sleep_ms <= 0) { ping_sleep_ms = 1; };
                        } else {
                            run_or_sleep = DO_RUN;
                        }
#if 0
                        double have_units  = (tm_to_run - tm_per_iter)/tm_per_iter;
                        double run_units =  my_freq_fctr * have_units;
                        double sleep_units =  (1.0 - my_freq_fctr) * have_units;
                        double run_per_slp =  (sleep_units > 0.0 ? run_units/sleep_units : 0.0);
                        double freq_fctr_run    = run_per_slp;
                        double freq_fctr_sleeps = 1;
                        //printf("tm_per_iter= %f secs, have_units= %f, run_units= %f sleep_units= %f, run_per_sleep= %f\n", tm_per_iter,
                        //    have_units, run_units, sleep_units, run_per_slp);
#endif
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
                    if (got_quit == 1) {
                        break;
                    }
#endif
                }
#endif
                xend = get_cputime();
                xcumu += xend-xbeg;
                if (freq_fctr_imx > 0) {
                    xinst += (double)10000 * freq_fctr_imx;
                } else {
                    xinst += (double)10000 * (double)imx;
                    xinst *= did_iters;
                }
                //printf("xcumu tm= %.3f, xinst= %g freq= %.3f GHz, i= %d, wrk_typ= %d\n", xcumu, xinst, 1.0e-9 * xinst/xcumu, i, args[i].wrk_typ);
                ops = (uint64_t)(xinst);
                tm_end = dclock3(clock, tsc_initial);
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
#ifdef __x86_64__
                        asm ("movl %1, %%eax;"
                                ".align 4;"
                                D100000
                                " movl %%eax, %0;"
                                :"=r"(b) /* output */
                                :"r"(a)  /* input */
                                :"%eax"  /* clobbered reg */
                        );
#elif __aarch64__
                            asm ( D100000 : : : "x0");
                            // a += b;
#endif
#endif
                        a |= b;
                }
                tm_end = MYDCLOCK();
                if (got_quit == 1) {
                        break;
                }
                }
                xend = get_cputime();
                xinst += (double)100000 * (double)imx;
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
#ifdef __x86_64__
                        asm ("movl %1, %%eax;"
                             "movl %1, %%ecx;"
                                ".align 4;"
                                C1000000
                                " movl %%ecx, %0;"
                                :"=r"(b) /* output */
                                :"r"(a)  /* input */
                                :"%ecx","%eax"  /* clobbered reg */
                        );
#elif __aarch64__
                            asm ( C1000000 : : : "x0");
                            // a += b;
#endif
                        a |= b;
                }
                tm_end = MYDCLOCK();
                if (got_quit == 1) {
                        break;
                }
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
                iiloops = args[i].loops;
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
                tm_end = MYDCLOCK();
#if 1
                // try to reduce the time spend in MYDCLOCK()
                if (++iters > 2) {
                        if ((tm_end - tm_prev) < 0.01) {
                                iiloops += 10;
                        }
                        iters = 3;
                }
                tm_prev = tm_end;
#endif
                if (got_quit == 1) {
                        break;
                }
                }
                xend = get_cputime();
                xcumu += xend-xbeg;
        }
        if (args[i].wrk_typ == WRK_SPIN_FAST) {
                xbeg = get_cputime();
                //iiloops = args[i].loops;
                while((tm_end - tm_beg) < tm_to_run) {
#if 1
                for (int ii=0; ii < iiloops; ii++)
                {
#endif
#if 1
                rezult = do_scale_fast(loops, rezult, ops);
#endif
#if 1
                }
#endif
                did_iters++;
                tm_end = MYDCLOCK();
#if 1
                // try to reduce the time spend in MYDCLOCK()
                if (++iters > 2) {
                        if ((tm_end - tm_prev) < 0.01) {
                                iiloops += 10;
                        }
                        iters = 3;
                }
                tm_prev = tm_end;
#endif
                if (got_quit == 1) {
                        break;
                }
                }
                xend = get_cputime();
                xcumu += xend-xbeg;
        }
        double dura2, dura;
        dura2 = dura = tm_end - tm_beg;
        if (dura2 == 0.0) {
                dura2 = 1.0;
        }
        if (freq_fctr_imx > 0) {
            if (args[i].freq_fctr_run_secs > 0.0) {
                dura2 = dura = args[i].freq_fctr_run_secs;
            } else if ( args[i].ping_tm_run > 0.0) {
                dura2 = dura = args[i].ping_tm_run;
            }
        }
#ifdef __x86_64__
        if (msr_fd >= 0) {
          cyc_end = read_msr(msr_fd, MSR_APERF);
          close(msr_fd);
        }
#elif __aarch64__
        //cyc_end = get_arm_cyc();
#endif
        args[i].cycles_beg = cyc_beg;
        args[i].cycles_end = cyc_end;
        args[i].dura = dura;
        args[i].tm_beg = tm_beg;
        args[i].tm_end = tm_end;
        args[i].units = "Gops/sec";
        if (options.verbose > 0) {
            if (freq_fctr.size() > 0) {
            printf("args[%d].freq_fctr_run_secs= %f, sleep_secs= %f, cur_run_ratio= %f target run_ratio= %f at %s %d\n",
                i, args[i].freq_fctr_run_secs, args[i].freq_fctr_sleep_secs,
                (args[i].freq_fctr_run_secs/(args[i].freq_fctr_run_secs + args[i].freq_fctr_sleep_secs)),
                (i < freq_fctr.size() ? freq_fctr[i] : 1.0), __FILE__, __LINE__);
            }
        }
        if (args[i].wrk_typ == WRK_SPIN || args[i].wrk_typ == WRK_SPIN_FAST) {
                args[i].perf = 1.0e-9 * (double)(ops)/(dura2);
                args[i].rezult = rezult;
        }
        if (args[i].wrk_typ == WRK_FREQ || args[i].wrk_typ == WRK_FREQ2 || args[i].wrk_typ == WRK_FREQ_SML ||
            args[i].wrk_typ == WRK_FREQ_SML2) {
                args[i].rezult = (double)a;
                if (args[i].cycles_beg != 0) {
                  args[i].perf = 1.0e-9 * (double)(args[i].cycles_end - args[i].cycles_beg)/(tm_end-tm_beg);
                  args[i].dura = tm_end - tm_beg;
                  args[i].used_msr_cycles = 1;
                } else {
                  args[i].perf = 1.0e-9 * xinst/xcumu;
                  args[i].dura = xcumu;
                }
                args[i].cpu_time = xcumu;
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
                case WRK_SPIN_FAST:
                case WRK_FREQ:
                case WRK_FREQ2:
                case WRK_FREQ_SML:
                case WRK_FREQ_SML2:
                        res = simd_dot0(i);
                        break;
                case WRK_MEM_BW:
                case WRK_MEM_LAT:
                case WRK_MEM_BW_RDWR:
                case WRK_MEM_BW_2RDWR:
                case WRK_MEM_BW_3RDWR:
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

static void opt_set_spin_tm(const char *optarg)
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

static void opt_set_clock(const char *optarg)
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

static void opt_set_cpus(const char *optarg)
{
        options.cpus = atoi(optarg);
        printf("cpus= %s\n", optarg);
}

static void opt_set_filename(const char *optarg)
{
        options.filename = optarg;
        printf("filename= %s\n", optarg);
}

static void opt_set_nopin(void)
{
        options.nopin = 1;
        printf("nopin= 1\n");
}

static void opt_set_yield(void)
{
        options.yield = 1;
        printf("yield= 1\n");
}

static void opt_set_huge_pages(void)
{
        options.huge_pages = 1;
        printf("try huge_page pos_memalign alloc = 1\n");
}

static void opt_set_size(const char *optarg)
{
        options.size = strtoull(optarg, NULL, 0);
        options.size_str = optarg;
        if (options.size_str.size() > 0 && (strstr(options.size_str.c_str(), "k") || strstr(options.size_str.c_str(), "K"))) {
                options.size *= 1024;
        } else if (options.size_str.size() > 0 && (strstr(options.size_str.c_str(), "m") || strstr(options.size_str.c_str(), "M"))) {
                options.size *= 1024*1024;
        } else if (options.size_str.size() > 0 && (strstr(options.size_str.c_str(), "g") || strstr(options.size_str.c_str(), "G"))) {
                options.size *= 1024*1024*1024;
        }
        double kb, mb, gb;
        kb = options.size;
        kb /= 1024.0;
        mb = kb / 1024.0;
        gb = mb / 1024.0;

        printf("array size is %s, %.0f bytes, %.3f KB, %.3f MB, %.3f GB\n", optarg, (double)options.size, kb, mb, gb);
}

static void opt_set_SLA(const char *optarg)
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

static void opt_set_ping_pong(const char *optarg)
{
        std::string my_str = std::string(optarg);
        std::string::size_type sz;     // alias of size_t

        options.ping_pong_run_secs  = std::stod (my_str,&sz);
        if (sz < (1+my_str.size())) {
          options.ping_pong_sleep_secs = std::stod (my_str.substr(sz+1));
        } else {
          options.ping_pong_sleep_secs = options.ping_pong_run_secs;
        }
        options.ping_pong_iter = 0;
        std::cout << "ping_pong_run_secs= " << options.ping_pong_run_secs << ", ping_pong_sleep_secs= " << options.ping_pong_sleep_secs << "\n";
}

static void opt_set_freq_fctr(unsigned int num_cpus_in, const char *optarg)
{
    std::string my_str = std::string(optarg);
    std::vector<std::string> result;
    std::stringstream s_stream(my_str); //create string stream from the string
    while(s_stream.good()) {
        std::string substr;
        std::getline(s_stream, substr, ','); //get first string delimited by comma
        result.push_back(substr);
    }
    std::string::size_type sz;
    for(int i = 0; i<result.size(); i++) {
        double dfctr = std::stod ( result.at(i), &sz);
        if (dfctr < 0.0) { dfctr = 0.0; }
        if (dfctr > 1.0) { dfctr = 1.0; }
        freq_fctr.push_back( dfctr );
        if ((i+1) == num_cpus_in) {
            break;
        }
    }
}

static void opt_set_List(unsigned int num_cpus_in, const char *optarg, int typ)
{
        std::string my_str = std::string(optarg);
        std::vector<std::string> result;
        std::stringstream s_stream(my_str); //create string stream from the string
        while(s_stream.good()) {
                std::string substr;
                std::getline(s_stream, substr, ','); //get first string delimited by comma
                result.push_back(substr);
        }
        std::size_t pos;
        std::string styp;
#ifdef __APPLE__
        printf("sorry but pinning is not supported on apple yet. Bye at %s %d\n", __FILE__, __LINE__);
        exit(1);
#endif
        if (typ == LIST_FOR_CPU) { styp = "cpu_list"; }
        if (typ == LIST_FOR_MEM) { styp = "mem_list"; }
        if (typ == LIST_FOR_MEMBIND) { styp = "membind_list"; }
        std::vector<int> ivec;
        for(int i = 0; i<result.size(); i++) {
                if (options.verbose > 0) {
                        std::cout << result.at(i) << std::endl;
                }
                std::string::size_type sz;
                pos = result.at(i).find("-");
                if (pos == std::string::npos) {
// number like 2
                        pos = result.at(i).find(":");
                        if (pos == std::string::npos) {
                                ivec.push_back(std::stoi(result.at(i), nullptr));
                                if (options.verbose > 0) {
                                  std::cout << "v= " << ivec.back() << "\n";
                                }
                        } else {
                                // 0:2 (start at 0, add 2 till end_cpu) or 0+2+16 (start at 0, add 2 till 16)
                                int ibeg = std::stoi(result.at(i), &sz);
                                std::string sb = result.at(i).substr(sz+1);
                                pos = sb.find(":");
                                int iadd = std::stoi(sb, &sz);
                                int iend;
                                if (pos == std::string::npos) {
                                        iend = num_cpus_in;
                                } else {
                                        iend = std::stoi(sb.substr(sz+1));
                                }
                                for (int j=ibeg; j< iend; j+= iadd) {
                                        ivec.push_back(j);
                                        if (options.verbose > 1) {
                                        std::cout << styp << " ivec[" << ivec.size()-1 << "] val= " << ivec.back() << "\n";
                                        }
                                }
                        }
                } else {
                        // range like 0-15 or 0-15:4
                        int ibeg = std::stoi(result.at(i), &sz);
                        int iend = std::stoi(result.at(i).substr(sz+1));
                        int iadd = 1;
                        pos = result.at(i).find(":");
                        if (pos != std::string::npos) {
                                std::string sb = result.at(i).substr(pos+1);
                                iadd = std::stoi(sb, &sz);
                        }
                        if (iadd < 1) { iadd = 1; }
                        if (options.verbose > 1) {
                           std::cout << styp << " ibeg= " << ibeg << ", iend= " << iend << ", iadd= " << iadd << "\n";
                        }
                           std::cout << styp << " ibeg= " << ibeg << ", iend= " << iend << ", iadd= " << iadd << "\n";
                        for (int j=ibeg; j<= iend; j+=iadd) {
                                ivec.push_back(j);
                                if (options.verbose > 1) {
                                        std::cout << styp << " ivec[" << ivec.size()-1 << "] val= " << ivec.back() << "\n";
                                }
                        }
                }
        }
        for(int i = 0; i<ivec.size(); i++) {
                if (typ == LIST_FOR_CPU) {
                        cpu_list.push_back(ivec[i]);
                }
                if (typ == LIST_FOR_MEM) {
                        mem_list.push_back(ivec[i]);
                }
                if (typ == LIST_FOR_MEMBIND) {
                        membind_list.push_back(ivec[i]);
                }
        }
}

static void opt_set_loops(const char *optarg)
{
        options.loops = atoi(optarg);
        options.loops_str = optarg;
        printf("loops size is %s\n", optarg);
}

static void opt_set_bump(const char *optarg)
{
        options.bump = atoi(optarg);
        options.bump_str = optarg;
        printf("bump (stride) size is %s\n", optarg);
}

static void opt_set_phase(const char *optarg)
{
        options.phase = optarg;
        printf("phase string is %s\n", optarg);
}

static void opt_set_wrk_typ(const char *optarg)
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
get_opt_main (unsigned int num_cpus_in, int argc, std::vector <std::string> argvs)
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
                        "   This option is intended for the freq_sml2 work type.\n"
                },
                {"huge_pages",     no_argument,       0, 'H', "alloc memory using posix_memalign on a 2MB boundary to try and use transparent huge pages\n"
                        "   The default is to not use THP\n"
                        "   This option is intended for the mem_bw work types.\n"
                },
                {"help",        no_argument,       &help_flag, 'h', "display help and exit"},
                /* These options don't set a flag.
                        We distinguish them by their indices. */
                {"time",      required_argument,   0, 't', "time to run (in seconds)\n"
                   "   -t tm_secs[,tm_secs_multi] \n"
                   "   required arg\n"
                },
                {"work",      required_argument,   0, 'w', "type of work to do\n"
                   "   work_type: spin|spin_fast|mem_bw|mem_bw_rdwr|mem_bw_2rd|mem_bw_2rdwr|mem_bw_2rd2wr|mem_bw_remote|disk_rd|disk_wr|disk_rdwr|disk_rd_dir|disk_wr_dir|disk_rdwr_dir\n"
                   "   sample disk io write cmd: ./bin/spin.x -w disk_wr_dir -f /mnt/nvme_pat1/tmp4 -s 200g -t 1 -l 2000 -b 4096\n"
                   "     takes 30 seconds on nvme drive capable of 1100 MB/s\n"
                   "   sample disk io read cmd: ./bin/spin.x -w disk_rd_dir -f /mnt/nvme_pat1/tmp4 -s 200g -t 1 -l 2000 -b 4096\n"
                   "     takes 10 seconds on nvme drive capable of 2800 MB/s\n"
                   "   spin do loop over floating point operations (maybe CPI=10 cycles/instruction?)\n"
                   "   spin_fast do loop over int add (IPC=4 on cascade lake 1 thr/core)\n"
                   "   mem_bw does pure read mem bandwidth test\n"
                   "   mem_bw_rdwr 2rd, 2rdwr 2rd2wr does various combos of read and write transaction\n"
                   "   disk_* tests does disk IO tests\n"
                   "   required arg\n"
                },
                {"bump",      required_argument,   0, 'b', "bytes to bump through array (in bytes) or, for disk tests, the number of pages per IO\n"
                   "   '-b 64' when looping over array, use 64 byte stride\n"
                   "   For mem_bw tests, the default stride size is 64 bytes."
                },
                {"file",      required_argument,   0, 'f', "filename for disk tests\n"
                   "   The file number will be appended to the filename.\n"
                   "   The file number is the thread number.\n"
                   "   The default filename is disk_tst_'file number' in the current dir\n"
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
                {"List",      required_argument,   0, 'L', "list of cpus to pin to for memory bw tests (after mallocing memory).\n"
                   "   Normally thread 0 is pinned to cpu 0, thr 1->cpu1 etc.\n"
                   "   If you specify -P (no pinning) then this option is ignored.\n"
                   "   If you don't specify the number of threads (-n X) option then the number of entries generated by the -L list option is used.\n"
                   "   Specify a comma separated list (-L 0,2,4) or 0:2 (start at 0 and add 2 till cpu_number >= number_of_cpus is exhausted).\n"
                   "   Or -L 0-8:2,10-16 (uses cpu 0,2,4,8,10,11,12,13,14,15,16)\n"
                },
                {"mem_node",      required_argument,   0, 'm', "list of nodes from which to malloc memory.\n"
                   "   you can use the -m cpu_list to list the nodes to pin to before the memory allocation.\n"
                   "   I'm not sure if -m works so I'm adding this option to use libnuma to specify the\n"
                   "   numa node for membind.  See -L option above for list syntax \n"
                },
                {"Mem",      required_argument,   0, 'M', "list of cpus to pin to before you allocate memory.\n"
                   "   Normally thread 0 is pinned to cpu 0, thr 1->cpu1 etc.\n"
                   "   If you specify -P (no pinning) then this option is ignored.\n"
                   "   See -L option above for list syntax\n"
                },
                {"loops",      required_argument,   0, 'l', "bytes to bump through array (in bytes)\n"
                   "   '-l 100' for disk tests. Used in work=spin or spin_fast to set number of loops over array\n"
                   "   for disk tests, default is 100. spin loops = 10000."
                },
                {"num_cpus",      required_argument,   0, 'n', "number of cpus to use\n"
                   "   '-n 1' will use just 1 cpu.\n"
                   "   For mem_bw/freq tests.\n"
                   "   The default is all cpus (1 thread per cpu).\n"
                },
                {"ping_pong",     required_argument,   0, 0, "comma separated list of alternating run_secs,sleep_secs.\n"
                   "   Right now assume 2 threads. start running thread 0 for runs_secs and thread 1 sleeps\n"
                   "   in increments of 'sleeps_secs'. After the run_secs is done a flag is set.\n"
                   "   then thread 0 begins sleeping sleeps_secs and once thrd 1 sees the flag it starts running for run_secs\n"
                   "   and so on until -t time_in_secs has elapsed.\n"
                   "   for example: ./bin/spin.x -w freq_sml2 -t 5 -L 4,28 --ping_pong 0.5,0.01 -l 10000\n" 
                   "   Currently only applies to freq_sml2. This feature is intended for testing perf on hyperthreaded cores\n"
                },
                {"freq_fctr",     required_argument,   0, 0, "comma separated list of load_factors (0.0 to 1.0).\n"
                   "   This is only fro freq_sml2 currently. The routine will try to keep the cpu busy by factor.s\n"
                   "   A factor of 0.0 means not busy at all, 1.0 means the cpu is kept completely busy.\n"
                   "   A best effort is made to keep the cpu busy according to the factor.\n"
                   "   example --freq_fctr 1.0,0.50   load thread 0 at 100% busy, thread 1 at 50.0% busy.\n"
                   "   If there are more threads than factors in the freq_fctr list then a factor of 1.0 is used.\n"
                },
                {"SLA",      required_argument,   0, 'S', "Service Level Agreement\n"
                   "   Enter -S pLVL0:warn0,crit0;pLVL1:warn1,crit1... for example\n"
                   "   '-S p50:80,100 -S p95:200,250 -S p99:250,300'\n"
                   "   where p50 means 50 percent of response times below  80 ms is okay, give a warning for above 80 and critical warning if p50 is above 100.\n"
                   "     and p95 means 95 percent of response times below 200 ms is okay, give a warning for above 80 and critical warning if p50 is above 100.\n"
                   "   You can enter multiple -S options.\n"
                   "   The default is no SLA, no SLA analysis\n"
                },
                {"clock",      required_argument,   0, 'c', "select clock to use for freq_sml and freq_sml2\n"
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

                c = mygetopt_long(argc, argv, "hvHPYb:c:f:L:l:m:M:n:p:s:S:t:w:", long_options, &option_index);

                /* Detect the end of the options. */
                if (c == -1)
                        break;

                switch (c)
                {
                case 0:
                        fprintf(stderr, "opt[%d]= %s\n", option_index, long_options[option_index].name);
                        if (strcmp(long_options[option_index].name, "nopin") == 0) {
                                opt_set_nopin();
                                break;
                        }
                        if (strcmp(long_options[option_index].name, "huge_pages") == 0) {
                                opt_set_huge_pages();
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
                        if (strcmp(long_options[option_index].name, "file") == 0) {
                                opt_set_filename(optarg);
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
                        if (strcmp(long_options[option_index].name, "ping_pong") == 0) {
                                opt_set_ping_pong(optarg);
                                break;
                        }
                        if (strcmp(long_options[option_index].name, "freq_fctr") == 0) {
                                opt_set_freq_fctr(num_cpus_in, optarg);
                                break;
                        }
                        if (strcmp(long_options[option_index].name, "List") == 0) {
                                opt_set_List(num_cpus_in, optarg, LIST_FOR_CPU);
                                break;
                        }
                        if (strcmp(long_options[option_index].name, "Mem") == 0) {
                                opt_set_List(num_cpus_in, optarg,  LIST_FOR_MEM);
                                break;
                        }
                        if (strcmp(long_options[option_index].name, "mem_node") == 0) {
                                opt_set_List(num_cpus_in, optarg,  LIST_FOR_MEMBIND);
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
                case 'H':
                        opt_set_huge_pages();
                        break;
                case 'b':
                        opt_set_bump(optarg);
                        break;
                case 'c':
                        opt_set_clock(optarg);
                        break;
                case 'f':
                        opt_set_filename(optarg);
                        break;
                case 'L':
                        opt_set_List(num_cpus_in, optarg, LIST_FOR_CPU);
                        break;
                case 'M':
                        opt_set_List(num_cpus_in, optarg, LIST_FOR_MEM);
                        break;
                case 'm':
                        opt_set_List(num_cpus_in, optarg, LIST_FOR_MEMBIND);
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

        if ((options.wrk_typ == WRK_SPIN || options.wrk_typ == WRK_SPIN_FAST) && options.loops == 0) {
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

int do_gettimeofday(void)
{
     struct timeval tp;
     int rc;
     rc = gettimeofday(&tp, NULL);
     double x = (double)tp.tv_sec + 1.0e-6*(double)tp.tv_usec;
     printf("t_gettimeofday= %f\n", x);
     return rc;
}

int main(int argc, char **argv)
{
        uint32_t cpu, cpu0;
        uint64_t t0, t1;
        double t_first = MYDCLOCK(), tm_beg, tm_beg_loop, tm_end_loop, tm_end, tm_beg_top, tm_end_top;
        int ping_iter = -1;
        double tot_ping_tm_run = 0.0, tot_ping_tm_sleep = 0.0;
        double tot_freq_fctr_run_secs = 0.0, tot_freq_fctr_sleep_secs = 0.0, tot_freq_fctr_sum=0.0, tot_freq_fctr_n=0.0 ;
        unsigned int num_cpus = std::thread::hardware_concurrency();
        bool doing_disk = false;
        std::mutex iomutex;
        tm_beg_top = MYDCLOCK();
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
        do_gettimeofday();
        std::cout << "Start Test 1 CPU" << std::endl; // prints !!!Hello World!!!
        double t_start, t_end;
        time_t c_start, c_end;
        printf("usage: %s -t tm_secs[,tm_secs_multi] -w work_type [ arg3 [ arg4 ]]]\n", argv[0]);
        printf("\twork_type: spin|spin_fast|mem_bw|mem_bw_rdwr|mem_bw_2rd|mem_bw_2rdwr|mem_bw_2rd2wr|mem_bw_remote|disk_rd|disk_wr|disk_rdwr|disk_rd_dir|disk_wr_dir|disk_rdwr_dir\n");
        printf("if mem_bw: -b stride in bytes. -s arg4 array_size in bytes\n");
        printf("Or first 2 options can be '-f input_file' where each line of input_file is the above cmdline options\n");
        printf("see input_files/haswell_spin_input.txt for example.\n");

        signal(SIGABRT, &sighandler);
        signal(SIGTERM, &sighandler);
        signal(SIGINT, &sighandler);

        tm_beg = tm_end = MYDCLOCK();
        my_getcpu(&cpu);
        cpu0 = cpu;
#ifdef __aarch64__
        t0 = get_tsc_and_cpu(&cpu0);
#else
        t0 = __rdtsc();
#endif
        while(tm_end - tm_beg < 0.1) {
                t1 = get_tsc_and_cpu(&cpu0);
                tm_end = MYDCLOCK();
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
                get_opt_main(num_cpus, (int)argvs[j].size(), argvs[j]);
                if (options.wrk_typ == WRK_MEM_BW || options.wrk_typ == WRK_MEM_BW_RDWR ||
                        options.wrk_typ == WRK_MEM_BW_2RDWR || options.wrk_typ == WRK_MEM_BW_2RD ||
                        options.wrk_typ == WRK_MEM_LAT ||
                        options.wrk_typ == WRK_MEM_BW_3RDWR ||
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
                        if (options.wrk_typ >= WRK_DISK_RD && options.wrk_typ <= WRK_DISK_RDWR_DIR) {
                                doing_disk = true;
                                if (options.loops == 0) {
                                        options.loops = 100;
                                        printf("Setting disk loops to default -l %d\n", (int)options.loops);
                                }
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

                if (options.cpus == -1 && cpu_list.size() > 0 && cpu_list.size() <= num_cpus) {
                    options.cpus = cpu_list.size();
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
                        t_start = MYDCLOCK();
                        dispatch_work(0);
                        t_end = MYDCLOCK();
                        if (phase.size() > 0) {
                                std::string str = "end phase ST "+phase+", dura= "+std::to_string(args[0].dura)+", "+args[0].units+"= "+std::to_string(args[0].perf);
                                do_trace_marker_write(str);
                                printf("%s\n", str.c_str());
                        }
                }
                for (unsigned i=0; i < num_cpus; i++) {
                        args[i].spin_tm = options.spin_tm_multi;
                }
                t_start = MYDCLOCK();
                printf("work= %s\n", work.c_str());
                fflush(NULL);

                if (!doing_disk && options.spin_tm_multi > 0.0) {
                        std::vector <rt_str> rt_vec;
                        uint64_t t0=0, t1;
                        int sleep_ms = 1; // 1 ms
                        for (unsigned i = 0; i < num_cpus; ++i) {
                                threads[i] = std::thread([&iomutex, i] {
                                        dispatch_work(i);
                                });
                                //while(mem_bw_threads_up <= i) {
                                //        my_msleep(sleep_ms);
                                //}
                        }
                        if (phase.size() > 0) {
                                do_trace_marker_write("begin phase MT "+phase);
                        }
                        do_barrier_wait();
                        tm_end_top  = MYDCLOCK();
                        tm_beg_loop = MYDCLOCK();

                        for (auto& t : threads) {
                                t.join();
                        }
                        tm_end_loop = MYDCLOCK();
                        double tot = 0.0;
                        for (unsigned i = 0; i < num_cpus; ++i) {
                                tot += args[i].perf;
                                tot_ping_tm_run  += args[i].ping_tm_run;
                                tot_ping_tm_sleep+= args[i].ping_tm_sleep;
                                ping_iter    = args[i].ping_iter;
                                tot_freq_fctr_run_secs   += args[i].freq_fctr_run_secs;
                                tot_freq_fctr_sleep_secs += args[i].freq_fctr_sleep_secs;
                                if (freq_fctr.size() > 0) {
                                    if (i < (int)freq_fctr.size()) {
                                        tot_freq_fctr_sum += freq_fctr[i];
                                    } else {
                                        tot_freq_fctr_sum += 1.0;
                                    }
                                    tot_freq_fctr_n += 1.0;
                                }
                                if (options.verbose > 1) {
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
                        if (wrk_typs[args[0].wrk_typ] == "freq_sml" || wrk_typs[args[0].wrk_typ] == "freq" ||
                            wrk_typs[args[0].wrk_typ] == "freq_sml2" || wrk_typs[args[0].wrk_typ] == "freq2") {
                            double avg_frq = tot/(double)(args.size());
                            printf("work= %s, threads= %d, total perf= %.3f %s, avg_freq= %.3f GHz\n",
                                wrk_typs[args[0].wrk_typ].c_str(), (int)args.size(), tot, args[0].units.c_str(), avg_frq);
                        } else {
                            printf("work= %s, threads= %d, total perf= %.3f %s\n", wrk_typs[args[0].wrk_typ].c_str(), (int)args.size(), tot, args[0].units.c_str());
                        }
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

                t_end = MYDCLOCK();
                if (options.loops > 0 && options.loops < 100 && !doing_disk && options.spin_tm_multi > 0.0) {
                        std::cout << "\nExecution time on " << num_cpus << " CPUs: " << t_end - t_start << " secs" << std::endl;
                }
        }
#if defined(__linux__) || defined(__APPLE__)
        proc_cputime = dclock_vari(CLOCK_PROCESS_CPUTIME_ID) - proc_cputime;
        tm_end = MYDCLOCK();
        printf("process cpu_time= %.6f secs, elapsed_secs= %.3f secs, tm_loop= %.3f tm_bef_loop= %.3f at %s %d\n",
        proc_cputime, tm_end-tm_beg, tm_end_loop-tm_beg_loop, tm_end_top-tm_beg_top, __FILE__, __LINE__);
        if (options.ping_pong_iter != -1) {
            printf("ping run_tm= %.3f secs, ping_sleep_tm= %.3f secs, ping_iters= %d at %s %d\n",
                tot_ping_tm_run, tot_ping_tm_sleep, ping_iter, __FILE__, __LINE__);
        }
        if (tot_freq_fctr_run_secs > 0.0 || tot_freq_fctr_sleep_secs > 0.0) {
            printf("freq_fctr run_tm= %.3f secs, sleep_tm= %.3f secs, avg_target run_fctr= %.3f got run_fctr= %.3f at %s %d\n",
                tot_freq_fctr_run_secs, tot_freq_fctr_sleep_secs, tot_freq_fctr_sum/tot_freq_fctr_n,
                (tot_freq_fctr_run_secs/(tot_freq_fctr_run_secs+tot_freq_fctr_sleep_secs)), __FILE__, __LINE__);
        }
        if (args[0].used_msr_cycles == 1) {
          printf("got cycles from /dev/cpu/*/msr device\n");
        }
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

