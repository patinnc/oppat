#!/bin/bash

PRF_CMD=~/perf.sh
PRF_CMD=~/bin/perf
PRF_CMD=perf
BASE=spin7
BASE=L2_bw7
BASE=arm_gb9
PFX=lnx
NUM_CPUS=`cat /proc/cpuinfo | grep processor |wc -l`
SCR_FIL=$0
SCR_DIR=`dirname "$(readlink -f "$0")"`
BIN_DIR=$SCR_DIR/../bin/
echo "SCR_DIR= $SCR_DIR and BIN_DIR=$BIN_DIR"
TRC_CMD=$BIN_DIR/trace-cmd

TEMPERATURE_FILE=/sys/devices/virtual/thermal/thermal_zone0/temp

ODIR=../oppat_data/$PFX/$BASE
mkdir -p $ODIR
chown -R root $ODIR

if [ -f $ODIR/file_list.json ]; then
  rm $ODIR/file_list.json
fi
if [ ! -f $BIN_DIR/wait.x ]; then
  echo "you need to build '$BIN_DIR/wait.x' using 'sh $SCR_DIR/../mk_spin.sh'"
  exit
fi
if [ ! -f $BIN_DIR/spin.x ]; then
  echo "you need to build '$BIN_DIR/spin.x' using 'sh $SCR_DIR/../mk_spin.sh'"
  exit
fi

if [ "$EUID" -ne 0 ]
  then echo "Please run as root or sudo"
  exit
fi

#disable nmi_watchdog consuming 1 hw counter slot
echo 0 > /proc/sys/kernel/nmi_watchdog

function ck_cmd_pid_threads_oper {
  local cmd=$1
  local pid=$2
  local init=$3
  local oper=$4
  local mx=$5
  echo "cmd= $cmd"
  echo "pid= $pid"
  local count=$init
  #while [ $count -le $mx ]
  while [ $count $oper $mx ]
  do
    sleep 1
    count=`ps -ef | grep $cmd | wc -l`
    echo "$cmd: got threads: $count"
  done
}



# trace-cmd clocks:
# [local] global counter uptime perf mono mono_raw boot x86-tsc
#$TRC_CMD record -C local -e sched:sched_switch -o trc$BASE.dat > trace_cmd_out.txt &
#$TRC_CMD record -C local -e thermal:thermal_power_cpu_get_power -e power:cpu_frequency -e power:cpu_idle -o trc$BASE.dat > trace_cmd_out.txt &
TC_DISK_EVT=" -e block:block_rq_issue -e block:block_rq_insert -e block:block_rq_complete -e ext4:ext4_direct_IO_enter -e ext4:ext4_direct_IO_exit -e ext4:ext4_da_write_begin -e ext4:ext4_da_write_end -e ext4:ext4_write_begin -e ext4:ext4_write_end "
TC_DISK_EVT=  # disk events put too much load on perf and cause prf_trace2.txt to have big gaps
#$TRC_CMD record -C local -e thermal:thermal_power_cpu_get_power -e power:cpu_idle -o trc$BASE.dat > trace_cmd_out.txt &
#$TRC_CMD record -C mono -e syscalls:sys_enter_write -e syscalls:sys_exit_write -e syscalls:sys_enter_read -e syscalls:sys_exit_read -e power:powernv_throttle -e i915:intel_gpu_freq_change -e i915:i915_flip_complete -e thermal:thermal_temperature -e power:cpu_idle -o trc$BASE.dat > trace_cmd_out.txt &
CPU_IDLE=" -e power:cpu_idle "
CPU_IDLE=  # lots of events on raspberry pi 3 b+ so just skip it. Doesn't provide so much info yet
IRQ_EVTS=" -e irq:irq_handler_entry -e irq:irq_handler_exit "
IRQ_EVTS=  #  tons of events on pi 3b+... and don't have a just for it yet so comment it out
$TRC_CMD record -C mono $IRQ_EVTS $TC_DISK_EVT -e power:cpu_frequency -e power:powernv_throttle -e thermal:thermal_temperature $CPU_IDLE -o $ODIR/tc_trace.dat > $ODIR/trace_cmd_out.txt &
PID_TRC_CMD=$!

# it takes a few seconds to get the trace cmd threads up
ck_cmd_pid_threads_oper $TRC_CMD $PID_TRC_CMD 1 -le $NUM_CPUS
echo started $TRC_CMD

WAIT_FILE=wait.pid.txt
rm $WAIT_FILE

export S_TIME_FORMAT=ISO && iostat -z -d -o JSON -t 1  > $ODIR/iostat.json &
IOSTAT_CMD=$!

nicstat -p 1 > $ODIR/nicstat.txt &
NICSTAT_CMD=$!

vmstat -n -t 1 > $ODIR/vmstat.txt &
VMSTAT_CMD=$!

$PRF_CMD stat -a -e alignment-faults,emulation-faults,major-faults,minor-faults -I 20 -x "\t" -o $ODIR/prf_energy.txt $BIN_DIR/wait.x $ODIR/prf_energy2.txt > $ODIR/wait.txt &
#$BIN_DIR/wait.x $ODIR/prf_energy2.txt > $ODIR/wait.txt &
while [ ! -f $WAIT_FILE ]
do
  echo waiting for $WAIT_FILE to exist
  sleep 0.1
done

#x armv8_pmuv3/br_immed_retired/
#x armv8_pmuv3/br_mis_pred/
#x armv8_pmuv3/br_pred/
#  armv8_pmuv3/bus_access/
#  armv8_pmuv3/bus_cycles/
#  armv8_pmuv3/cid_write_retired/
#  armv8_pmuv3/cpu_cycles/
#  armv8_pmuv3/exc_return/
#  armv8_pmuv3/exc_taken/
#  armv8_pmuv3/inst_retired/
#2 armv8_pmuv3/l1d_cache/
#2 armv8_pmuv3/l1d_cache_refill/
#3 armv8_pmuv3/l1d_cache_wb/
#2 armv8_pmuv3/l1d_tlb_refill/
#x armv8_pmuv3/l1i_cache/
#x armv8_pmuv3/l1i_cache_refill/
#x armv8_pmuv3/l1i_tlb_refill/
#  armv8_pmuv3/l2d_cache/
#2 armv8_pmuv3/l2d_cache_refill/
#3 armv8_pmuv3/l2d_cache_wb/
#3 armv8_pmuv3/ld_retired/
#  armv8_pmuv3/mem_access/
#  armv8_pmuv3/memory_error/
#  armv8_pmuv3/pc_write_retired/
#3 armv8_pmuv3/st_retired/
#  armv8_pmuv3/sw_incr/
#  armv8_pmuv3/unaligned_ldst_retired/

# branch:
#   br_cond [Conditional branch executed] armv8_pmuv3/event=0xc9/ 
#   br_cond_mispred [Conditional branch mispredicted] armv8_pmuv3/event=0xcc/ 
#   br_indirect_mispred [Indirect branch mispredicted] armv8_pmuv3/event=0xca/ 
#   br_indirect_mispred_addr [Indirect branch mispredicted because of address miscompare] armv8_pmuv3/event=0xcb/ 
#   br_indirect_spec [Branch speculatively executed, indirect branch] armv8_pmuv3/event=0x7a/ 
# 
# bus:
#   bus_access_rd [Bus access read] armv8_pmuv3/event=0x60/ 
#   bus_access_wr [Bus access write] armv8_pmuv3/event=0x61/ 
# 
# cache:
#   ext_snoop [SCU Snooped data from another CPU for this CPU] armv8_pmuv3/event=0xc8/ 
#   prefetch_linefill [Linefill because of prefetch] armv8_pmuv3/event=0xc2/ 
#   prefetch_linefill_drop [Instruction Cache Throttle occurred] armv8_pmuv3/event=0xc3/ 
#   read_alloc [Read allocate mode] armv8_pmuv3/event=0xc5/ 
#   read_alloc_enter [Entering read allocate mode] armv8_pmuv3/event=0xc4/ 
# 
# memory:
#   ext_mem_req [External memory request] armv8_pmuv3/event=0xc0/ 
#   ext_mem_req_nc [Non-cacheable external memory request] armv8_pmuv3/event=0xc1/ 
# 
# other:
#   exc_fiq [Exception taken, FIQ] armv8_pmuv3/event=0x87/ 
#   exc_irq [Exception taken, IRQ] armv8_pmuv3/event=0x86/ 
#   l1d_cache_err [L1 Data Cache (data, tag or dirty) memory error, correctable or non-correctable] armv8_pmuv3/event=0xd1/ 
#   l1i_cache_err [L1 Instruction Cache (data or tag) memory error] armv8_pmuv3/event=0xd0/ 
#0  pre_decode_err [Pre-decode error] armv8_pmuv3/event=0xc6/ 
#   tlb_err [TLB memory error] armv8_pmuv3/event=0xd2/ 
# 
# pipeline:
#2  agu_dep_stall [Cycles there is an interlock for a load/store instruction waiting for data to calculate the address in the AGU] armv8_pmuv3/event=0xe5/ 
#x  decode_dep_stall [Cycles the DPU IQ is empty and there is a pre-decode error being processed] armv8_pmuv3/event=0xe3/ 
#x  ic_dep_stall [Cycles the DPU IQ is empty and there is an instruction cache miss being processed] armv8_pmuv3/event=0xe1/ 
#x  iutlb_dep_stall [Cycles the DPU IQ is empty and there is an instruction micro-TLB miss being processed] armv8_pmuv3/event=0xe2/ 
#2  ld_dep_stall [Cycles there is a stall in the Wr stage because of a load miss] armv8_pmuv3/event=0xe7/ 
#x  other_iq_dep_stall [Cycles that the DPU IQ is empty and that is not because of a recent micro-TLB miss, instruction cache miss or pre-decode error] armv8_pmuv3/event=0xe0/ 
#4  simd_dep_stall [Cycles there is an interlock for an Advanced SIMD/Floating-point operation] armv8_pmuv3/event=0xe6/ 
#4  other_interlock_stall [Cycles there is an interlock other than Advanced SIMD/Floating-point instructions or load/store instruction] armv8_pmuv3/event=0xe4/ 
#3  st_dep_stall [Cycles there is a stall in the Wr stage because of a store] armv8_pmuv3/event=0xe8/ 
#3  stall_sb_full [Data Write operation that stalls the pipeline because the store buffer is full] armv8_pmuv3/event=0xc7/ 

# armv8_pmuv3/br_immed_retired/
# armv8_pmuv3/br_mis_pred/
# armv8_pmuv3/br_pred/
#   decode_dep_stall [Cycles the DPU IQ is empty and there is a pre-decode error being processed] armv8_pmuv3/event=0xe3/ 
#   pre_decode_err [Pre-decode error] armv8_pmuv3/event=0xc6/ 
#   ic_dep_stall [Cycles the DPU IQ is empty and there is an instruction cache miss being processed] armv8_pmuv3/event=0xe1/ 
GRP0=br_immed_retired,br_mis_pred,br_pred,ic_dep_stall,decode_dep_stall,pre_decode_err

# armv8_pmuv3/l1i_cache/
# armv8_pmuv3/l1i_cache_refill/
# armv8_pmuv3/l1i_tlb_refill/
#   ic_dep_stall [Cycles the DPU IQ is empty and there is an instruction cache miss being processed] armv8_pmuv3/event=0xe1/ 
#   iutlb_dep_stall [Cycles the DPU IQ is empty and there is an instruction micro-TLB miss being processed] armv8_pmuv3/event=0xe2/ 
#   other_iq_dep_stall [Cycles that the DPU IQ is empty and that is not because of a recent micro-TLB miss, instruction cache miss or pre-decode error] armv8_pmuv3/event=0xe0/ 
GRP1=l1i_cache,l1i_cache_refill,l1i_tlb_refill,ic_dep_stall,iutlb_dep_stall,other_iq_dep_stall

#  armv8_pmuv3/l1d_cache/
#  armv8_pmuv3/l1d_cache_refill/
#  armv8_pmuv3/l1d_tlb_refill/
#  armv8_pmuv3/l2d_cache_refill/
#   agu_dep_stall [Cycles there is an interlock for a load/store instruction waiting for data to calculate the address in the AGU] armv8_pmuv3/event=0xe5/ 
#   ld_dep_stall [Cycles there is a stall in the Wr stage because of a load miss] armv8_pmuv3/event=0xe7/ 
GRP2=l1d_cache,l1d_cache_refill,l1d_tlb_refill,l2d_cache_refill,agu_dep_stall,ld_dep_stall

#  armv8_pmuv3/ld_retired/
#  armv8_pmuv3/st_retired/
#  armv8_pmuv3/l2d_cache_wb/
#  armv8_pmuv3/l1d_cache_wb/
#3  st_dep_stall [Cycles there is a stall in the Wr stage because of a store] armv8_pmuv3/event=0xe8/ 
#3  stall_sb_full [Data Write operation that stalls the pipeline because the store buffer is full] armv8_pmuv3/event=0xc7/ 
GRP3=ld_retired,st_retired,l2d_cache_wb,l1d_cache_wb,st_dep_stall,stall_sb_full

#   prefetch_linefill [Linefill because of prefetch] armv8_pmuv3/event=0xc2/ 
#  armv8_pmuv3/l2d_cache/
#2 armv8_pmuv3/l2d_cache_refill/
#3 armv8_pmuv3/l2d_cache_wb/
#   bus_access
#  armv8_pmuv3/mem_access/
GRP4=prefetch_linefill,l2d_cache,l2d_cache_refill,l2d_cache_wb,bus_access,mem_access

#   bus_access_rd [Bus access read] armv8_pmuv3/event=0x60/ 
#   bus_access_wr [Bus access write] armv8_pmuv3/event=0x61/ 
#  armv8_pmuv3/bus_access/
#   ext_mem_req [External memory request] armv8_pmuv3/event=0xc0/ 
#   ext_mem_req_nc [Non-cacheable external memory request] armv8_pmuv3/event=0xc1/ 
#   ext_snoop [SCU Snooped data from another CPU for this CPU] armv8_pmuv3/event=0xc8/ 
GRP5=bus_access_rd,bus_access_wr,bus_access,ext_mem_req,ext_snoop
GRP6=inst_retired,simd_dep_stall,other_interlock_stall,bus_cycles,unaligned_ldst_retired,prefetch_linefill_drop

evt_lstp0="{cpu-clock,cpu_cycles,$GRP0}:S"
evt_lstp1="{cpu-clock,cpu_cycles,$GRP1}:S"
evt_lstp2="{cpu-clock,cpu_cycles,$GRP2}:S"
evt_lstp3="{cpu-clock,cpu_cycles,$GRP3}:S"
evt_lstp4="{cpu-clock,cpu_cycles,$GRP4}:S"
evt_lstp5="{cpu-clock,cpu_cycles,$GRP5}:S"
evt_lstp6="{cpu-clock,cpu_cycles,$GRP6}:S"
#evt_lstp6="{cpu-clock,cpu_cycles,$RES_STALL_ROB,$RES_STALL_RS,$RES_STALL_SB,$RAT_UOPS_STALL}:S"
#evt_lstp7="{cpu-clock,cpu_cycles,$STALLS_L1D_MISS_PEND,$STALLS_L2_MISS_PEND,$OFFCORE_SQ_FULL,$OFFCORE_RESP_ALL_L3_MISS}:S"

echo try prf2
echo $PRF_CMD record -a -k CLOCK_MONOTONIC --running-time -F 499 -e "$evt_lstp0" -e "$evt_lstp1" -e "$evt_lstp2" -e "$evt_lstp3" -e "$evt_lstp4" -e "$evt_lstp5" -o $ODIR/prf_trace2.data
     $PRF_CMD record -a -k CLOCK_MONOTONIC --running-time -F 499 -e "$evt_lstp0" -e "$evt_lstp1" -e "$evt_lstp2" -e "$evt_lstp3" -e "$evt_lstp4" -e "$evt_lstp5" -e "$evt_lstp6" -o $ODIR/prf_trace2.data &> $ODIR/prf_trace2.out &
PRF_CMD_PID2=$!
echo did prf2
#
#$PRF_CMD record -a  -e power:cpu_frequency/call-graph=no/ -g -e sched:sched_switch -e "{ref-cycles/freq=997/,cycles,instructions}"  -o prf_$BASE.data $BIN_DIR/spin.x
#$PRF_CMD record -a -k CLOCK_MONOTONIC -e cpu-clock,power:cpu_frequency/call-graph=no/ -g -e sched:sched_switch -e "{ref-cycles/freq=997/,cycles,instructions}"  -o $ODIR/prf_trace.data $BIN_DIR/spin.x 4 mem_bw > $ODIR/spin.txt
SPIN_ARGS="4 mem_bw 8 8k"
SPIN_ARGS="4 mem_bw 32 300k"
SPIN_ARGS="4 mem_bw_rdwr 64 80m"
SPIN_ARGS="4 mem_bw_2rd 64 40k"  # gets 44 & 38 bytes/cycle to L2, 120 GB/s 4 cpus
SPIN_ARGS="4 mem_bw_2rd 64 80m"
SPIN_ARGS="4 mem_bw_2rd2wr 64 100k" # get 36 & 37 bytes/cycle, 50 & 80 GB/s @ 4 cpus
SPIN_ARGS="4 mem_bw 64 80m" #
SPIN_ARGS="4 spin"
SPIN_ARGS="4 mem_bw 64 15k"  # gets 44 & 38 bytes/cycle to L2, 34*4 GB/s @ 4 cpus.
SPIN_ARGS="4 mem_bw_2rdwr 64 80m" # get xx & 37 bytes/cycle, 50 & 80 GB/s @ 4 cpus
SPIN_ARGS="4 mem_bw_2rd2wr 64 80m" # get xx & 37 bytes/cycle, 50 & 80 GB/s @ 4 cpus
SPIN_ARGS="4 mem_bw 64 100k"
SPIN_ARGS="4 mem_bw"
SPIN_ARGS="4 mem_bw_2rd 64 80m" # get xx & 37 bytes/cycle, 50 & 80 GB/s @ 4 cpus
SPIN_ARGS="-f input_files/arm_cortex_a53_spin_input.txt"
sleep 1 # it takes about a 1 second (it seems) for the previous perf cmd (with all the events) to get up and running
$BIN_DIR/clocks.x > $ODIR/clocks1.txt
SPIN_BIN=$BIN_DIR/spin.x
SPIN_BIN=$SCR_DIR/run_gb_and_gawk.sh
chmod a+rx $SPIN_BIN
GB_BIN=/home/pfay/geekbench/Geekbench-4.3.3-Linux/geekbench4
GB_BIN=/home/pfay/gb/dist/Geekbench-2.4.2-LinuxARM/geekbench
GB_AWK_SCR=$SCR_DIR/gb_rd_output.gawk
SPIN_ARGS="$GB_BIN $GB_AWK_SCR $ODIR"
#$PRF_CMD record -a -k CLOCK_MONOTONIC -e cpu-clock,power:cpu_frequency/call-graph=no/ -g -e sched:sched_switch -o $ODIR/prf_trace.data $BIN_DIR/spin.x 4 mem_bw > $ODIR/spin.txt
#$PRF_CMD record -a -k CLOCK_MONOTONIC -e cpu-clock,power:cpu_frequency/call-graph=no/ -g -e sched:sched_switch -o $ODIR/prf_trace.data $SPIN_BIN $SPIN_ARGS > $ODIR/spin.txt
$PRF_CMD record -a -k CLOCK_MONOTONIC  -g -e sched:sched_switch -o $ODIR/prf_trace.data $SPIN_BIN $SPIN_ARGS > $ODIR/spin.txt


#$PRF_CMD record -a -k CLOCK_MONOTONIC -e cpu-clock,power:cpu_frequency/call-graph=no/ -g -e sched:sched_switch -o $ODIR/prf_trace.data $BIN_DIR/spin.x 4 mem_bw > $ODIR/spin.txt
#$PRF_CMD record -a -k CLOCK_MONOTONIC -e cpu-clock,power:cpu_frequency/call-graph=no/ -g -e sched:sched_switch -o $ODIR/prf_trace.data $BIN_DIR/spin.x $SPIN_ARGS > $ODIR/spin.txt
#$PRF_CMD record -a -k CLOCK_MONOTONIC  -g -e sched:sched_switch -o $ODIR/prf_trace.data $BIN_DIR/spin.x $SPIN_ARGS > $ODIR/spin.txt

$BIN_DIR/clocks.x > $ODIR/clocks2.txt
kill -2 `cat $WAIT_FILE`
kill -2 $PID_TRC_CMD 
kill -2 $PRF_CMD_PID2
kill -2 $IOSTAT_CMD
kill -2 $NICSTAT_CMD
kill -2 $VMSTAT_CMD
wait $PRF_CMD_PID2

ck_cmd_pid_threads_oper $TRC_CMD $PID_TRC_CMD 2 -gt 1

#exit
#sudo ../perf.sh record -a -g -e sched:sched_switch   -o $BASE.data sleep 0.5
trcFopts=" -F trace:comm,tid,pid,time,cpu,event,trace,period,callindent "
hwFopts="     -F hw:comm,tid,pid,time,cpu,period,event,ip,sym,dso,symoff,brstack,brstacksym,flags "
trcFopts=" -F trace:comm,tid,pid,time,cpu,period,event,sym,dso,symoff,trace,brstack,brstacksym,flags,bpf-output,callindent"
trcFopts=" -F trace:comm,tid,pid,time,cpu,period,event,ip,sym,dso,symoff,trace,brstack,brstacksym,flags,bpf-output,callindent"
#trcFopts=
Fopts=" -F comm,tid,pid,time,cpu,period,event,ip,sym,dso,symoff,trace,flags,callindent"
Fopts2=" -F comm,tid,pid,time,cpu,period,event,ip,sym,dso,symoff,flags,callindent"
Fopts3=" -F comm,tid,pid,time,cpu,event,ip,sym,dso,symoff,flags,callindent"

#../perf.sh script -I --ns --header -f $hwFopts $trcFopts -D -i prf_trace.data > prf_d.txt
#$PRF_CMD script -I --ns --header -f $hwFopts $trcFopts -i $ODIR/prf_trace.data > $ODIR/prf_trace.txt
$PRF_CMD script -I --ns --demangle --header -f $Fopts -i $ODIR/prf_trace.data  > $ODIR/prf_trace.txt
echo did perf script
echo $PRF_CMD script -I --ns --header -f $Fopts -i $ODIR/prf_trace2.data _ $ODIR/prf_trace2.txt
$PRF_CMD script -I --ns --demangle --header -f $Fopts2 -i $ODIR/prf_trace2.data > $ODIR/prf_trace2.txt
#$PRF_CMD script -I --ns --header -f $Fopts3 -i $ODIR/prf_trace2.data > $ODIR/prf_trace2.txt
echo did perf script2
$TRC_CMD report -t -i $ODIR/tc_trace.dat > $ODIR/tc_trace.txt
echo did trace-cmd report
#chmod a+rw $ODIR/tc*
#chmod a+rw $ODIR/*.json
#chmod a+rw $ODIR/*.txt
rm $WAIT_FILE
rm $ODIR/prf_*.data.old

chmod a+rwx $SCR_DIR/dump_all_perf_events.sh
$SCR_DIR/dump_all_perf_events.sh $ODIR
cp $SCR_FIL $ODIR

# get the cpu topology info. the power and cpufreq subdirs contain files that cause bsdtar errors
bsdtar cjvf $ODIR/sys_devices_system_cpu.tgz --exclude "power" --exclude "cpufreq" /sys/devices/system/cpu


echo "{}] } ] }}" >> $ODIR/iostat.json # iostat doesn't print out the closing json


echo "{\"file_list\":[" > $ODIR/file_list.json
echo "   {\"cur_dir\":\"%root_dir%/oppat_data/$PFX/$BASE\"}," >> $ODIR/file_list.json
echo "   {\"cur_tag\":\"${PFX}_$BASE\"}," >> $ODIR/file_list.json
echo "   {\"bin_file\":\"prf_trace.data\", \"txt_file\":\"prf_trace.txt\", \"tag\":\"%cur_tag%\", \"type\":\"PERF\"}," >> $ODIR/file_list.json
echo "   {\"bin_file\":\"prf_trace2.data\", \"txt_file\":\"prf_trace2.txt\", \"tag\":\"%cur_tag%\", \"type\":\"PERF\"}," >> $ODIR/file_list.json
echo "   {\"bin_file\":\"tc_trace.dat\",   \"txt_file\":\"tc_trace.txt\",  \"tag\":\"%cur_tag%\", \"type\":\"TRACE_CMD\"}," >> $ODIR/file_list.json
echo "   {\"bin_file\":\"prf_energy.txt\", \"txt_file\":\"prf_energy2.txt\", \"wait_file\":\"wait.txt\", \"tag\":\"%cur_tag%\", \"type\":\"LUA\"}," >> $ODIR/file_list.json
echo "   {\"bin_file\":\"spin.txt\", \"txt_file\":\"\", \"wait_file\":\"\", \"tag\":\"%cur_tag%\", \"type\":\"LUA\", \"lua_file\":\"spin.lua\", \"lua_rtn\":\"spin\"}, " >> $ODIR/file_list.json
echo "   {\"bin_file\":\"\", \"txt_file\":\"gb_phase.tsv\", \"wait_file\":\"\", \"tag\":\"%cur_tag%\", \"type\":\"LUA\", \"lua_file\":\"gb_phase.lua\", \"lua_rtn\":\"gb_phase\", \"options\":\"USE_AS_PHASE,FIND_MULTI_PHASE,USE_EXTRA_STR\"}, " >> $ODIR/file_list.json
echo "   {\"bin_file\":\"\", \"txt_file\":\"iostat.json\", \"wait_file\":\"\", \"tag\":\"%cur_tag%\", \"type\":\"LUA\", \"lua_file\":\"iostat.lua\", \"lua_rtn\":\"iostat\"}, " >> $ODIR/file_list.json
echo "   {\"bin_file\":\"\", \"txt_file\":\"vmstat.txt\", \"wait_file\":\"\", \"tag\":\"%cur_tag%\", \"type\":\"LUA\", \"lua_file\":\"vmstat.lua\", \"lua_rtn\":\"vmstat\"}, " >> $ODIR/file_list.json
echo "   {\"bin_file\":\"\", \"txt_file\":\"nicstat.txt\", \"wait_file\":\"\", \"tag\":\"%cur_tag%\", \"type\":\"LUA\", \"lua_file\":\"nicstat.lua\", \"lua_rtn\":\"nicstat\"} " >> $ODIR/file_list.json
echo "  ]} " >> $ODIR/file_list.json

chmod a+rw $ODIR/*
chown -R root $ODIR

#sudo ../perf.sh script -I --header -i perf.data -F hw:comm,tid,pid,time,cpu,event,ip,sym,dso,symoff,period -F trace:comm,tid,pid,time,cpu,event,trace,ip,sym,period -i perf.data > perf.txt
