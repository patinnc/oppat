// portions from /usr/include/linux/perf_event.h so using it probably attaches GPL
//
#include <stdint.h>

#pragma once

enum perf_header_version {
	PERF_HEADER_VERSION_1,
	PERF_HEADER_VERSION_2,
};

typedef uint64_t u64;
typedef uint32_t u32;

struct perf_file_section {
	u64 offset;
	u64 size;
};

// below enum from https://github.com/torvalds/linux/blob/master/tools/perf/util/header.h
enum {
	HEADER_RESERVED		= 0,	/* always cleared */
	HEADER_FIRST_FEATURE	= 1,
	HEADER_TRACING_DATA	= 1,
	HEADER_BUILD_ID,
	HEADER_HOSTNAME,
	HEADER_OSRELEASE,
	HEADER_VERSION,
	HEADER_ARCH,
	HEADER_NRCPUS,
	HEADER_CPUDESC,
	HEADER_CPUID,
	HEADER_TOTAL_MEM,
	HEADER_CMDLINE,
	HEADER_EVENT_DESC,
	HEADER_CPU_TOPOLOGY,
	HEADER_NUMA_TOPOLOGY,
	HEADER_BRANCH_STACK,
	HEADER_PMU_MAPPINGS,
	HEADER_GROUP_DESC,
	HEADER_AUXTRACE,
	HEADER_STAT,
	HEADER_CACHE,
	HEADER_SAMPLE_TIME,
	HEADER_MEM_TOPOLOGY,
	HEADER_LAST_FEATURE,
	HEADER_FEAT_BITS	= 256,
};

static char *feat_str[] = {
	"RESERVED",
	"TRACING_DATA",
	"BUILD_ID",
	"HOSTNAME",
	"OSRELEASE",
	"VERSION",
	"ARCH",
	"NRCPUS",
	"CPUDESC",
	"CPUID",
	"TOTAL_MEM",
	"CMDLINE",
	"EVENT_DESC",
	"CPU_TOPOLOGY",
	"NUMA_TOPOLOGY",
	"BRANCH_STACK",
	"PMU_MAPPINGS",
	"GROUP_DESC",
	"AUXTRACE",
	"STAT",
	"CACHE",
	"SAMPLE_TIME",
	"MEM_TOPOLOGY",
	"LAST_FEATURE",
};

enum perf_event_sample_format {
	PERF_SAMPLE_IP				= 1U << 0,
	PERF_SAMPLE_TID				= 1U << 1,
	PERF_SAMPLE_TIME			= 1U << 2,
	PERF_SAMPLE_ADDR			= 1U << 3,
	PERF_SAMPLE_READ			= 1U << 4,
	PERF_SAMPLE_CALLCHAIN		= 1U << 5,
	PERF_SAMPLE_ID				= 1U << 6,
	PERF_SAMPLE_CPU				= 1U << 7,
	PERF_SAMPLE_PERIOD			= 1U << 8,
	PERF_SAMPLE_STREAM_ID		= 1U << 9,
	PERF_SAMPLE_RAW				= 1U << 10,
	PERF_SAMPLE_BRANCH_STACK	= 1U << 11,
	PERF_SAMPLE_REGS_USER		= 1U << 12,
	PERF_SAMPLE_STACK_USER		= 1U << 13,
	PERF_SAMPLE_WEIGHT			= 1U << 14,
	PERF_SAMPLE_DATA_SRC		= 1U << 15,
	PERF_SAMPLE_IDENTIFIER		= 1U << 16,
	PERF_SAMPLE_TRANSACTION		= 1U << 17,
	PERF_SAMPLE_REGS_INTR		= 1U << 18,

	PERF_SAMPLE_MAX 			= 1U << 19,		/* non-ABI */
};

enum perf_type_id {
	PERF_TYPE_HARDWARE			= 0,
	PERF_TYPE_SOFTWARE			= 1,
	PERF_TYPE_TRACEPOINT		= 2,
	PERF_TYPE_HW_CACHE			= 3,
	PERF_TYPE_RAW				= 4,
	PERF_TYPE_BREAKPOINT		= 5,
	PERF_TYPE_ETW				= 6, // added by pfay

	PERF_TYPE_MAX,				/* non-ABI */
};

enum perf_event_read_format {
	PERF_FORMAT_TOTAL_TIME_ENABLED	= 1U << 0,
	PERF_FORMAT_TOTAL_TIME_RUNNING	= 1U << 1,
	PERF_FORMAT_ID					= 1U << 2,
	PERF_FORMAT_GROUP				= 1U << 3,

	PERF_FORMAT_MAX 				= 1U << 4,		/* non-ABI */
};

enum perf_event_type {

	PERF_RECORD_MMAP			= 1,
	PERF_RECORD_LOST			= 2,
	PERF_RECORD_COMM			= 3,
	PERF_RECORD_EXIT			= 4,
	PERF_RECORD_THROTTLE		= 5,
	PERF_RECORD_UNTHROTTLE		= 6,
	PERF_RECORD_FORK			= 7,
	PERF_RECORD_READ			= 8,
	PERF_RECORD_SAMPLE			= 9,
	PERF_RECORD_MMAP2			= 10,
	PERF_RECORD_AUX				= 11,
	PERF_RECORD_ITRACE_START	= 12,
	PERF_RECORD_LOST_SAMPLES	= 13,
	PERF_RECORD_SWITCH			= 14,
	PERF_RECORD_SWITCH_CPU_WIDE	= 15,
	PERF_RECORD_MAX,			/* non-ABI */
};

enum perf_hw_id {
	/*
	 * Common hardware events, generalized by the kernel:
	 */
	PERF_COUNT_HW_CPU_CYCLES		= 0,
	PERF_COUNT_HW_INSTRUCTIONS		= 1,
	PERF_COUNT_HW_CACHE_REFERENCES	= 2,
	PERF_COUNT_HW_CACHE_MISSES		= 3,
	PERF_COUNT_HW_BRANCH_INSTRUCTIONS	= 4,
	PERF_COUNT_HW_BRANCH_MISSES		= 5,
	PERF_COUNT_HW_BUS_CYCLES		= 6,
	PERF_COUNT_HW_STALLED_CYCLES_FRONTEND	= 7,
	PERF_COUNT_HW_STALLED_CYCLES_BACKEND	= 8,
	PERF_COUNT_HW_REF_CPU_CYCLES		= 9,

	PERF_COUNT_HW_MAX,			/* non-ABI */
};


enum perf_sw_ids {
	PERF_COUNT_SW_CPU_CLOCK			= 0,
	PERF_COUNT_SW_TASK_CLOCK		= 1,
	PERF_COUNT_SW_PAGE_FAULTS		= 2,
	PERF_COUNT_SW_CONTEXT_SWITCHES	= 3,
	PERF_COUNT_SW_CPU_MIGRATIONS	= 4,
	PERF_COUNT_SW_PAGE_FAULTS_MIN	= 5,
	PERF_COUNT_SW_PAGE_FAULTS_MAJ	= 6,
	PERF_COUNT_SW_ALIGNMENT_FAULTS	= 7,
	PERF_COUNT_SW_EMULATION_FAULTS	= 8,
	PERF_COUNT_SW_DUMMY				= 9,
	PERF_COUNT_SW_BPF_OUTPUT		= 10,

	PERF_COUNT_SW_MAX,			/* non-ABI */
};



#define __u32 uint32_t
#define __s32  int32_t
#define __u64 uint64_t
#define __s64  int64_t

#pragma pack(push, 1)
#pragma pack(pop)
struct perf_event_attr {

	/*
	 * Major type: hardware/software/tracepoint/etc.
	 */
	__u32			type;

	/*
	 * Size of the attr structure, for fwd/bwd compat.
	 */
	__u32			size;

	/*
	 * Type specific configuration information.
	 */
	__u64			config;

	union {
		__u64		sample_period;
		__u64		sample_freq;
	};

	__u64			sample_type;
	__u64			read_format;

	__u64			disabled       :  1, /* off by default        */
				inherit	       :  1, /* children inherit it   */
				pinned	       :  1, /* must always be on PMU */
				exclusive      :  1, /* only group on PMU     */
				exclude_user   :  1, /* don't count user      */
				exclude_kernel :  1, /* ditto kernel          */
				exclude_hv     :  1, /* ditto hypervisor      */
				exclude_idle   :  1, /* don't count when idle */
				mmap           :  1, /* include mmap data     */
				comm	       :  1, /* include comm data     */
				freq           :  1, /* use freq, not period  */
				inherit_stat   :  1, /* per task counts       */
				enable_on_exec :  1, /* next exec enables     */
				task           :  1, /* trace fork/exit       */
				watermark      :  1, /* wakeup_watermark      */
				/*
				 * precise_ip:
				 *
				 *  0 - SAMPLE_IP can have arbitrary skid
				 *  1 - SAMPLE_IP must have constant skid
				 *  2 - SAMPLE_IP requested to have 0 skid
				 *  3 - SAMPLE_IP must have 0 skid
				 *
				 *  See also PERF_RECORD_MISC_EXACT_IP
				 */
				precise_ip     :  2, /* skid constraint       */
				mmap_data      :  1, /* non-exec mmap data    */
				sample_id_all  :  1, /* sample_type all events */

				exclude_host   :  1, /* don't count in host   */
				exclude_guest  :  1, /* don't count in guest  */

				exclude_callchain_kernel : 1, /* exclude kernel callchains */
				exclude_callchain_user   : 1, /* exclude user callchains */
				mmap2          :  1, /* include mmap with inode data     */
				comm_exec      :  1, /* flag comm events that are due to an exec */
				use_clockid    :  1, /* use @clockid for time fields */
				context_switch :  1, /* context switch data */
				__reserved_1   : 37;

	union {
		__u32		wakeup_events;	  /* wakeup every n events */
		__u32		wakeup_watermark; /* bytes before wakeup   */
	};

	__u32			bp_type;
	union {
		__u64		bp_addr;
		__u64		config1; /* extension of config */
	};
	union {
		__u64		bp_len;
		__u64		config2; /* extension of config1 */
	};
	__u64	branch_sample_type; /* enum perf_branch_sample_type */

	/*
	 * Defines set of user regs to dump on samples.
	 * See asm/perf_regs.h for details.
	 */
	__u64	sample_regs_user;

	/*
	 * Defines size of the user stack to dump on samples.
	 */
	__u32	sample_stack_user;

	__s32	clockid;
	/*
	 * Defines set of regs to dump for each sample
	 * state captured on:
	 *  - precise = 0: PMU interrupt
	 *  - precise > 0: sampled instruction
	 *
	 * See asm/perf_regs.h for details.
	 */
	__u64	sample_regs_intr;

	/*
	 * Wakeup watermark for AUX area
	 */
	__u32	aux_watermark;
	__u32	__reserved_2;	/* align to __u64 */
};

struct perf_header {
	char magic[8];		/* PERFILE2 */
	uint64_t size;		/* size of the header */
	uint64_t attr_size;	/* size of an attribute in attrs */
	struct perf_file_section attrs;
	struct perf_file_section data;
	struct perf_file_section event_types;
	uint64_t flags;
	uint64_t flags1[3];
};

struct perf_file_attr {
	struct perf_event_attr attr;
	struct perf_file_section id;
};

#pragma pack(push, 1)
struct perf_header_string {
	   uint32_t len;
	   char *string; /* zero terminated */
	   //char string[len]; /* zero terminated */
};
#pragma pack(pop)

#pragma pack(push, 1)
struct perf_header_string_list {
	uint32_t nr;
	//struct perf_header_string strings[nr]; /* variable length records */
	struct perf_header_string *strings; /* variable length records */
};
#pragma pack(pop)

	 /* struct {
	 *	struct perf_event_header	header;
	 *
	 *	#
	 *	# Note that PERF_SAMPLE_IDENTIFIER duplicates PERF_SAMPLE_ID.
	 *	# The advantage of PERF_SAMPLE_IDENTIFIER is that its position
	 *	# is fixed relative to header.
	 *	#
	 *
	 *	{ u64			id;	  } && PERF_SAMPLE_IDENTIFIER
	 *	{ u64			ip;	  } && PERF_SAMPLE_IP
	 *	{ u32			pid, tid; } && PERF_SAMPLE_TID
	 *	{ u64			time;     } && PERF_SAMPLE_TIME
	 *	{ u64			addr;     } && PERF_SAMPLE_ADDR
	 *	{ u64			id;	  } && PERF_SAMPLE_ID
	 *	{ u64			stream_id;} && PERF_SAMPLE_STREAM_ID
	 *	{ u32			cpu, res; } && PERF_SAMPLE_CPU
	 *	{ u64			period;   } && PERF_SAMPLE_PERIOD
	 *
	 *	{ struct read_format	values;	  } && PERF_SAMPLE_READ
	 *
	 *	{ u64			nr,
	 *	  u64			ips[nr];  } && PERF_SAMPLE_CALLCHAIN
	 *
	 *	#
	 *	# The RAW record below is opaque data wrt the ABI
	 *	#
	 *	# That is, the ABI doesn't make any promises wrt to
	 *	# the stability of its content, it may vary depending
	 *	# on event, hardware, kernel version and phase of
	 *	# the moon.
	 *	#
	 *	# In other words, PERF_SAMPLE_RAW contents are not an ABI.
	 *	#
	 *
	 *	{ u32			size;
	 *	  char                  data[size];}&& PERF_SAMPLE_RAW
	 *
	 *	{ u64                   nr;
	 *        { u64 from, to, flags } lbr[nr];} && PERF_SAMPLE_BRANCH_STACK
	 *
	 * 	{ u64			abi; # enum perf_sample_regs_abi
	 * 	  u64			regs[weight(mask)]; } && PERF_SAMPLE_REGS_USER
	 *
	 * 	{ u64			size;
	 * 	  char			data[size];
	 * 	  u64			dyn_size; } && PERF_SAMPLE_STACK_USER
	 *
	 *	{ u64			weight;   } && PERF_SAMPLE_WEIGHT
	 *	{ u64			data_src; } && PERF_SAMPLE_DATA_SRC
	 *	{ u64			transaction; } && PERF_SAMPLE_TRANSACTION
	 *	{ u64			abi; # enum perf_sample_regs_abi
	 *	  u64			regs[weight(mask)]; } && PERF_SAMPLE_REGS_INTR
	 *	{ u64			phys_addr;} && PERF_SAMPLE_PHYS_ADDR
	 * };
	PERF_RECORD_SAMPLE			= 9,
	 */
#if 0
	   * If PERF_FORMAT_GROUP was specified to allow reading all events in a
		 group at once:

			 struct read_format {
				 u64 nr;            /* The number of events */
				 u64 time_enabled;  /* if PERF_FORMAT_TOTAL_TIME_ENABLED */
				 u64 time_running;  /* if PERF_FORMAT_TOTAL_TIME_RUNNING */
				 struct {
					 u64 value;     /* The value of the event */
					 u64 id;        /* if PERF_FORMAT_ID */
				 } values[nr];
			 };

	   * If PERF_FORMAT_GROUP was not specified:

			 struct read_format {
				 u64 value;         /* The value of the event */
				 u64 time_enabled;  /* if PERF_FORMAT_TOTAL_TIME_ENABLED */
				 u64 time_running;  /* if PERF_FORMAT_TOTAL_TIME_RUNNING */
				 u64 id;            /* if PERF_FORMAT_ID */
			 };
	 *	{ u64			id;	  } && PERF_SAMPLE_IDENTIFIER
	 *	{ u64			ip;	  } && PERF_SAMPLE_IP
	 *	{ u32			pid, tid; } && PERF_SAMPLE_TID
	 *	{ u64			time;     } && PERF_SAMPLE_TIME
	 *	{ u64			addr;     } && PERF_SAMPLE_ADDR
	 *	{ u64			id;	  } && PERF_SAMPLE_ID
	 *	{ u64			stream_id;} && PERF_SAMPLE_STREAM_ID
	 *	{ u32			cpu, res; } && PERF_SAMPLE_CPU
	 *	{ u64			period;   } && PERF_SAMPLE_PERIOD
	 *
	 *	{ struct read_format	values;	  } && PERF_SAMPLE_READ
	 *
	 *	{ u64			nr,
	 *	  u64			ips[nr];  } && PERF_SAMPLE_CALLCHAIN
#endif


#if 0
				   PERF_COUNT_HW_CPU_CYCLES
						  Total cycles.  Be wary of what happens during CPU frequency scaling.

				   PERF_COUNT_HW_INSTRUCTIONS
						  Retired instructions.  Be careful, these can be affected by various issues,  most  notably  hardware
						  interrupt counts.

				   PERF_COUNT_HW_CACHE_REFERENCES
						  Cache  accesses.   Usually  this  indicates Last Level Cache accesses but this may vary depending on
						  your CPU.  This may include prefetches and coherency messages; again this depends on the  design  of
						  your CPU.

				   PERF_COUNT_HW_CACHE_MISSES
						  Cache  misses.   Usually this indicates Last Level Cache misses; this is intended to be used in con‐
						  junction with the PERF_COUNT_HW_CACHE_REFERENCES event to calculate cache miss rates.

				   PERF_COUNT_HW_BRANCH_INSTRUCTIONS
						  Retired branch instructions.  Prior to Linux 2.6.35, this used the wrong event on AMD processors.

				   PERF_COUNT_HW_BRANCH_MISSES
						  Mispredicted branch instructions.

				   PERF_COUNT_HW_BUS_CYCLES
						  Bus cycles, which can be different from total cycles.

				   PERF_COUNT_HW_STALLED_CYCLES_FRONTEND (since Linux 3.0)
						  Stalled cycles during issue.

				   PERF_COUNT_HW_STALLED_CYCLES_BACKEND (since Linux 3.0)
						  Stalled cycles during retirement.

				   PERF_COUNT_HW_REF_CPU_CYCLES (since Linux 3.3)
						  Total cycles; not affected by CPU frequency scaling.

			  If type is PERF_TYPE_SOFTWARE, we are measuring software events provided by the kernel.  Set config  to  one  of
			  the following:

				   PERF_COUNT_SW_CPU_CLOCK
						  This reports the CPU clock, a high-resolution per-CPU timer.

				   PERF_COUNT_SW_TASK_CLOCK
						  This reports a clock count specific to the task that is running.

				   PERF_COUNT_SW_PAGE_FAULTS
						  This reports the number of page faults.

				   PERF_COUNT_SW_CONTEXT_SWITCHES
						  This  counts  context  switches.   Until Linux 2.6.34, these were all reported as user-space events,
						  after that they are reported as happening in the kernel.

				   PERF_COUNT_SW_CPU_MIGRATIONS
						  This reports the number of times the process has migrated to a new CPU.

				   PERF_COUNT_SW_PAGE_FAULTS_MIN
						  This counts the number of minor page faults.  These did not require disk I/O to handle.

				   PERF_COUNT_SW_PAGE_FAULTS_MAJ
						  This counts the number of major page faults.  These required disk I/O to handle.

				   PERF_COUNT_SW_ALIGNMENT_FAULTS (since Linux 2.6.33)
						  This counts the number of alignment faults.  These happen when unaligned memory accesses happen; the
						  kernel  can handle these but it reduces performance.  This happens only on some architectures (never
						  on x86).

				   PERF_COUNT_SW_EMULATION_FAULTS (since Linux 2.6.33)
						  This counts the number of emulation faults.  The kernel sometimes traps  on  unimplemented  instruc‐
						  tions and emulates them for user space.  This can negatively impact performance.

				   PERF_COUNT_SW_DUMMY (since Linux 3.12)
						  This  is a placeholder event that counts nothing.  Informational sample record types such as mmap or
						  comm must be associated with an active event.  This dummy event allows gathering such records  with‐
						  out requiring a counting event.

			  If  type  is  PERF_TYPE_TRACEPOINT, then we are measuring kernel tracepoints.  The value to use in config can be
			  //obtained from under debugfs tracing/events/*/*/id if ftrace is enabled in the kernel.

			  If type is PERF_TYPE_HW_CACHE, then we are measuring a hardware CPU cache event.  To calculate  the  appropriate
			  config value use the following equation:

					  (perf_hw_cache_id) | (perf_hw_cache_op_id << 8) |
					  (perf_hw_cache_op_result_id << 16)

				  where perf_hw_cache_id is one of:

					  PERF_COUNT_HW_CACHE_L1D
							 for measuring Level 1 Data Cache

					  PERF_COUNT_HW_CACHE_L1I
							 for measuring Level 1 Instruction Cache

					  PERF_COUNT_HW_CACHE_LL
							 for measuring Last-Level Cache

					  PERF_COUNT_HW_CACHE_DTLB
							 for measuring the Data TLB

					  PERF_COUNT_HW_CACHE_ITLB
							 for measuring the Instruction TLB

					  PERF_COUNT_HW_CACHE_BPU
							 for measuring the branch prediction unit

					  PERF_COUNT_HW_CACHE_NODE (since Linux 3.1)
							 for measuring local memory accesses

				  and perf_hw_cache_op_id is one of

					  PERF_COUNT_HW_CACHE_OP_READ
							 for read accesses

					  PERF_COUNT_HW_CACHE_OP_WRITE
							 for write accesses

					  PERF_COUNT_HW_CACHE_OP_PREFETCH
							 for prefetch accesses

				  and perf_hw_cache_op_result_id is one of

					  PERF_COUNT_HW_CACHE_RESULT_ACCESS
							 to measure accesses

					  PERF_COUNT_HW_CACHE_RESULT_MISS
							 to measure misses

  branch-instructions OR branches                    [Hardware event]
  branch-misses                                      [Hardware event]
  bus-cycles                                         [Hardware event]
  cache-misses                                       [Hardware event]
  cache-references                                   [Hardware event]
  cpu-cycles OR cycles                               [Hardware event]
  instructions                                       [Hardware event]
  ref-cycles                                         [Hardware event]
  alignment-faults                                   [Software event]
  bpf-output                                         [Software event]
  context-switches OR cs                             [Software event]
  cpu-clock                                          [Software event]
  cpu-migrations OR migrations                       [Software event]
  dummy                                              [Software event]
  emulation-faults                                   [Software event]
  major-faults                                       [Software event]
  minor-faults                                       [Software event]
  page-faults OR faults                              [Software event]
  task-clock                                         [Software event]
#endif
