#!/bin/bash

TRC_CMD=~/bin/trace-cmd
PRF_CMD=~/perf.sh
PRF_CMD=perf
PRF_CMD=~/bin/perf
BASE=spin7
BASE=L2_bw7
BASE=mem_bw7
PFX=lnx
NUM_CPUS=`cat /proc/cpuinfo | grep processor |wc -l`
SCR_FIL=$0
SCR_DIR=`dirname "$(readlink -f "$0")"`
BIN_DIR=$SCR_DIR/../bin/
echo "SCR_DIR= $SCR_DIR and BIN_DIR=$BIN_DIR"

ODIR=../oppat_data/$PFX/$BASE
mkdir -p $ODIR
chown -R 777 $ODIR

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
#$TRC_CMD record -C local -e thermal:thermal_power_cpu_get_power -e power:cpu_idle -o trc$BASE.dat > trace_cmd_out.txt &
#$TRC_CMD record -C mono -e syscalls:sys_enter_write -e syscalls:sys_exit_write -e syscalls:sys_enter_read -e syscalls:sys_exit_read -e power:powernv_throttle -e i915:intel_gpu_freq_change -e i915:i915_flip_complete -e thermal:thermal_temperature -e power:cpu_idle -o trc$BASE.dat > trace_cmd_out.txt &
$TRC_CMD record -C mono -e power:powernv_throttle -e i915:intel_gpu_freq_change -e i915:i915_flip_complete -e thermal:thermal_temperature -e power:cpu_idle -o $ODIR/tc_trace.dat > $ODIR/trace_cmd_out.txt &
PID_TRC_CMD=$!

# it takes a few seconds to get the trace cmd threads up
ck_cmd_pid_threads_oper $TRC_CMD $PID_TRC_CMD 1 -le $NUM_CPUS
echo started $TRC_CMD

WAIT_FILE=wait.pid.txt
rm $WAIT_FILE

$PRF_CMD stat -a -e unc_arb_trk_requests.all,power/energy-pkg/,power/energy-cores/,power/energy-gpu/,power/energy-ram/,uncore_cbox_0/clockticks/,uncore_cbox_1/clockticks/,uncore_imc/data_reads/,uncore_imc/data_writes/ -I 20 -x "\t" -o $ODIR/prf_energy.txt $BIN_DIR/wait.x $ODIR/prf_energy2.txt > $ODIR/wait.txt &
#$BIN_DIR/wait.x $ODIR/prf_energy2.txt > $ODIR/wait.txt &
while [ ! -f $WAIT_FILE ]
do
  echo waiting for $WAIT_FILE to exist
  sleep 0.1
done

#$PRF_CMD record -a -k CLOCK_MONOTONIC -e ftrace:bprint -e ftrace:print  -F 997 -e "{cpu-clock,ref-cycles,cycles,instructions}:S" -o $ODIR/prf_trace2.data  &
#$PRF_CMD record -a -k CLOCK_MONOTONIC -e ftrace:bprint -e ftrace:print  -F 997 -e "{cpu-clock,ref-cycles,cycles,instructions}:S" -o $ODIR/prf_trace2.data  &
# r0100 = inst_retired.any using fixed but doesn't work (count is always 0)
# r0200 = cpu_clk_unhalted.thread using fixed but doesn't work (count is always 0)
# r0300 = cpu_clk_unhalted.ref , using fixed
# r3c   = cpu_clk_unhalted.thread maybe uses gen countr
# rc0   = inst_retired.any maybe uses gen counter

UOP_P0="cpu/name='uops_dispatched_port.port_0',umask=0x01,event=0xa1/"
UOP_P1="cpu/name='uops_dispatched_port.port_1',umask=0x02,event=0xa1/"
UOP_P2="cpu/name='uops_dispatched_port.port_2',umask=0x04,event=0xa1/"
UOP_P3="cpu/name='uops_dispatched_port.port_3',umask=0x08,event=0xa1/"
UOP_P4="cpu/name='uops_dispatched_port.port_4',umask=0x10,event=0xa1/"
UOP_P5="cpu/name='uops_dispatched_port.port_5',umask=0x20,event=0xa1/"
UOP_P6="cpu/name='uops_dispatched_port.port_6',umask=0x40,event=0xa1/"
UOP_P7="cpu/name='uops_dispatched_port.port_7',umask=0x80,event=0xa1/"

IDQ_NOT_DEL="cpu/name='idq_uops_not_delivered.core',umask=0x01,event=0x9c/"
#  idq_uops_not_delivered.core [Uops not delivered to Resource Allocation Table (RAT) per thread when backend of the machine is not stalled Spec update: HSD135] cpu/umask=0x1,(null)=0x1e8483,event=0x9c/ 
IDQ_NOT_DEL_CYCLES="cpu/name='idq_uops_not_delivered.cycles_0_uops_deliv.core',umask=0x01,cmask=0x04,event=0x9c/"
#  idq_uops_not_delivered.cycles_0_uops_deliv.core [Cycles per thread when 4 or more uops are not delivered to Resource Allocation Table (RAT) when backend of the machine is not stalled Spec update: HSD135] cpu/umask=0x1,(null)=0x1e8483,cmask=0x4,event=0x9c/ 
IDQ_EMPTY="cpu/name='idq.empty',umask=0x02,event=0x79/"
#  idq.empty [Instruction Decode Queue (IDQ) empty cycles Spec update: HSD135] cpu/umask=0x2,(null)=0x1e8483,event=0x79/ 
ICACHE_STALL="cpu/name='icache.ifetch_stall',umask=0x04,event=0x80/"
#  icache.ifetch_stall [Cycles where a code fetch is stalled due to L1 instruction-cache miss] cpu/umask=0x4,(null)=0x1e8483,event=0x80/ 
IDQ_ALL_UOPS="cpu/name='idq.mite_all_uops',umask=0x3c,event=0x79/"
#  idq.mite_all_uops [Uops delivered to Instruction Decode Queue (IDQ) from MITE path] cpu/umask=0x3c,(null)=0x1e8483,event=0x79/ 

LSD_UOPS="cpu/name='lsd.uops',umask=0x01,event=0xa8/"
#  lsd.uops [Number of Uops delivered by the LSD] cpu/umask=0x1,(null)=0x1e8483,event=0xa8/ 

RAT_UOPS="cpu/name='uops_issued.any',umask=0x01,event=0x0e/"
#  uops_issued.any [Uops that Resource Allocation Table (RAT) issues to Reservation Station (RS)] cpu/umask=0x1,(null)=0x1e8483,event=0xe/ 

RAT_UOPS_STALL="cpu/name='uops_issued.core_stall_cycles',inv=0x1,umask=0x01,any=0x1,cmask=0x1,event=0x0e/"
#  uops_issued.core_stall_cycles [Cycles when Resource Allocation Table (RAT) does not issue Uops to Reservation Station (RS) for all threads] cpu/inv=0x1,umask=0x1,any=0x1,(null)=0x1e8483,cmask=0x1,event=0xe/ 

RET_UOPS_STALL="cpu/name='uops_retired.core_stall_cycles',inv=0x1,umask=0x01,any=0x1,cmask=0x1,event=0xc2/"
# uops_retired.core_stall_cycles [Cycles without actually retired uops] cpu/inv=0x1,umask=0x1,any=0x1,(null)=0x1e8483,cmask=0x1,event=0xc2/ 

UOPS_RETIRED="cpu/name='uops_retired.all',umask=0x01,event=0xc2/"
#  uops_retired.all [Actually retired uops Supports address when precise (Precise event)] cpu/umask=0x1,(null)=0x1e8483,event=0xc2/ 

DSB_UOPS="cpu/name='idq.dsb_uops',umask=0x08,event=0x79/"
#  idq.dsb_uops [Uops delivered to Instruction Decode Queue (IDQ) from the Decode Stream Buffer (DSB) path] cpu/umask=0x8,(null)=0x1e8483,event=0x79/ 

MS_UOPS="cpu/name='idq.ms_uops',umask=0x30,event=0x79/"
#  idq.ms_uops [Uops delivered to Instruction Decode Queue (IDQ) while Microcode Sequenser (MS) is busy] cpu/umask=0x30,(null)=0x1e8483,event=0x79/ 

UOP_LD_L3_MISS="cpu/name='mem_load_uops_l3_miss_retired.local_dram',umask=0x01,event=0xd3/"
#  mem_load_uops_l3_miss_retired.local_dram [(null)] cpu/umask=0x1,(null)=0x186a3,event=0xd3/  , top down says to use this... we'll see


MISP_CYCLES="cpu/name='int_misc.recovery_cycles',umask=0x03,cmask=0x1,event=0x0d/"
#  int_misc.recovery_cycles [Core cycles the allocator was stalled due to recovery from earlier clear event for this thread (e.g. misprediction or memory nuke)] cpu/umask=0x3,(null)=0x1e8483,cmask=0x1,event=0xd/ 

ICACHE_MISSES="cpu/name='icache.misses',umask=0x02,event=0x80/"
#  icache.misses [Number of Instruction Cache, Streaming Buffer and Victim Cache Misses.  Includes Uncacheable accesses] cpu/umask=0x2,(null)=0x30d43,event=0x80/ 

L1D_FULL_CYCLES="cpu/name='l1d_pend_miss.fb_full',umask=0x02,cmask=0x01,event=0x48/"
#  l1d_pend_miss.fb_full [Cycles a demand request was blocked due to Fill Buffers inavailability] cpu/umask=0x2,(null)=0x1e8483,cmask=0x1,event=0x48/ 

L1D_REPL="cpu/name='l1d.replacement',umask=0x01,event=0x51/"
#  l1d.replacement [L1D data line replacements] cpu/umask=0x1,(null)=0x1e8483,event=0x51/

MEM_LD_L1_HIT="cpu/name='mem_load_uops_retired.l1_hit',umask=0x01,event=0xd1/"
#  mem_load_uops_retired.l1_hit [Retired load uops with L1 cache hits as data sources Spec update: HSD29, HSM30. Supports address when precise (Precise event)] cpu/umask=0x1,(null)=0x1e8483,event=0xd1/ 

MEM_LD_L1_MISS="cpu/name='mem_load_uops_retired.l1_miss',umask=0x08,event=0xd1/"
#  mem_load_uops_retired.l1_miss [Retired load uops misses in L1 cache as data sources Spec update: HSM30. Supports address when precise (Precise event)] cpu/umask=0x8,(null)=0x186a3,event=0xd1/ 

STALLS_L1D_MISS_PEND="cpu/name='cycle_activity.stalls_l1d_pending',umask=0x0c,cmask=0x0c,event=0xa3/"
#  cycle_activity.stalls_l1d_pending [Execution stalls due to L1 data cache misses] cpu/umask=0xc,(null)=0x1e8483,cmask=0xc,event=0xa3/ 
STALLS_L2_MISS_PEND="cpu/name='cycle_activity.stalls_l2_pending',umask=0x05,cmask=0x05,event=0xa3/"
#  cycle_activity.stalls_l2_pending [Execution stalls due to L2 cache misses] cpu/umask=0x5,(null)=0x1e8483,cmask=0x5,event=0xa3/ 
STALLS_MEM_MISS_PEND="cpu/name='cycle_activity.stalls_ldm_pending',umask=0x06,cmask=0x06,event=0xa3/"
#  cycle_activity.stalls_ldm_pending [Execution stalls due to memory subsystem] cpu/umask=0x6,(null)=0x1e8483,cmask=0x6,event=0xa3/ 

OFFCORE_SQ_FULL="cpu/name='offcore_requests_buffer.sq_full',umask=0x01,event=0xb2/"
#  offcore_requests_buffer.sq_full [Offcore requests buffer cannot take more entries for this thread core] cpu/umask=0x1,(null)=0x1e8483,event=0xb2/ 

OFFCORE_RESP_ALL_L3_MISS="cpu/name='offcore_response.all_requests.l3_miss.any_response',umask=0x01,event=0xb7,offcore_rsp=0x3FFFC08FFF/"
#  offcore_response.all_requests.l3_miss.any_response [Counts all requests that miss in the L3] cpu/umask=0x1,(null)=0x186a3,event=0xb7,offcore_rsp=0xffc08fff/ 

OFFCORE_REQ_ALL_L2_RD_MISS="cpu/name='offcore_requests.all_data_rd',umask=0x08,event=0xb0/"
#  offcore_requests.all_data_rd [Demand and prefetch data reads] cpu/umask=0x8,(null)=0x186a3,event=0xb0/ 


L2_REQ_MISS="cpu/name='l2_rqsts.miss',umask=0x3f,event=0x24/"
#  l2_rqsts.miss [All requests that miss L2 cache Spec update: HSD78] cpu/umask=0x3f,(null)=0x30d43,event=0x24/ 

L2_REQ_REF="cpu/name='l2_rqsts.references',umask=0xff,event=0x24/"
#  l2_rqsts.references [All L2 requests Spec update: HSD78] cpu/umask=0xff,(null)=0x30d43,event=0x24/ 

#L2_TRANS_ALL="cpu/name='l2_trans.all_requests',umask=0x80,event=0xf0/"
L2_TRANS_ALL="cpu/name='l2_trans.all_requests',umask=0xff,event=0xf0/"
#  l2_trans.all_requests [Transactions accessing L2 pipe] cpu/umask=0x80,(null)=0x30d43,event=0xf0/ 


RES_STALL_ROB="cpu/name='resource_stalls.rob',umask=0x10,event=0xa2/"
#  resource_stalls.rob [Cycles stalled due to re-order buffer full] cpu/umask=0x10,(null)=0x1e8483,event=0xa2/ 
RES_STALL_RS="cpu/name='resource_stalls.rs',umask=0x04,event=0xa2/"
#  resource_stalls.rs [Cycles stalled due to no eligible RS entry available] cpu/umask=0x4,(null)=0x1e8483,event=0xa2/ 
RES_STALL_SB="cpu/name='resource_stalls.sb',umask=0x08,event=0xa2/"
#  resource_stalls.sb [Cycles stalled due to no store buffers available. (not including draining form sync)] cpu/umask=0x8,(null)=0x1e8483,event=0xa2/ 

UNC_ARB_ALL="uncore_arb/name='unc_arb_trk_requests.all',umask=0x01,event=0x81/"
#  unc_arb_trk_requests.all [Unit: uncore_arb Total number of Core outgoing entries allocated.  Accounts for Coherent and non-coherent traffic] uncore_arb/umask=0x1,event=0x81/ 


evt_lstp0="{cpu-clock,cycles,$UOP_P0,$UOP_P1,$UOP_P2,$UOP_P3}:S"
evt_lstp1="{cpu-clock,cycles,$UOP_P4,$UOP_P5,$UOP_P6,$UOP_P7}:S"
evt_lstp2="{cpu-clock,cycles,$LSD_UOPS,$IDQ_ALL_UOPS,$L2_TRANS_ALL,$DSB_UOPS}:S"
evt_lstp3="{cpu-clock,cycles,$MS_UOPS,$ICACHE_STALL,$MISP_CYCLES,$ICACHE_MISSES}:S"
evt_lstp4="{cpu-clock,cycles,$L1D_FULL_CYCLES,$L1D_REPL,$L2_REQ_MISS,$L2_REQ_REF}:S"
evt_lstp5="{cpu-clock,cycles,$RAT_UOPS,$IDQ_NOT_DEL,$UOPS_RETIRED,$RET_UOPS_STALL}:S"
evt_lstp6="{cpu-clock,cycles,$RES_STALL_ROB,$RES_STALL_RS,$RES_STALL_SB,$RAT_UOPS_STALL}:S"
evt_lstp7="{cpu-clock,cycles,$STALLS_L1D_MISS_PEND,$STALLS_L2_MISS_PEND,$OFFCORE_SQ_FULL,$OFFCORE_RESP_ALL_L3_MISS}:S"
#evt_lstp8="{cpu-clock,cycles,$OFFCORE_REQ_ALL_L2_RD_MISS,$L2_REQ_MISS}:S"

echo try prf2
echo $PRF_CMD record -a -k CLOCK_MONOTONIC --running-time -F 997 -e "$evt_lstp0" -e "$evt_lstp1" -e "$evt_lstp2" -e "$evt_lstp3" -e "$evt_lstp4" -e "$evt_lstp5" -e "$evt_lstp6" -e "$evt_lstp7" -o $ODIR/prf_trace2.data 
     $PRF_CMD record -a -k CLOCK_MONOTONIC --running-time -F 997 -e "$evt_lstp0" -e "$evt_lstp1" -e "$evt_lstp2" -e "$evt_lstp3" -e "$evt_lstp4" -e "$evt_lstp5" -e "$evt_lstp6" -e "$evt_lstp7" -o $ODIR/prf_trace2.data &> $ODIR/prf_trace2.out &
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
#$PRF_CMD record -a -k CLOCK_MONOTONIC -e cpu-clock,power:cpu_frequency/call-graph=no/ -g -e sched:sched_switch -o $ODIR/prf_trace.data $BIN_DIR/spin.x 4 mem_bw > $ODIR/spin.txt
$PRF_CMD record -a -k CLOCK_MONOTONIC -e cpu-clock,power:cpu_frequency/call-graph=no/ -g -e sched:sched_switch -o $ODIR/prf_trace.data $BIN_DIR/spin.x $SPIN_ARGS > $ODIR/spin.txt

kill -2 `cat $WAIT_FILE`
kill -2 $PID_TRC_CMD 
kill -2 $PRF_CMD_PID2
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
$PRF_CMD script -I --ns --header -f $Fopts -i $ODIR/prf_trace.data  > $ODIR/prf_trace.txt
echo did perf script
echo $PRF_CMD script -I --ns --header -f $Fopts -i $ODIR/prf_trace2.data _ $ODIR/prf_trace2.txt
$PRF_CMD script -I --ns --header -f $Fopts2 -i $ODIR/prf_trace2.data > $ODIR/prf_trace2.txt
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

echo "{\"file_list\":[" > $ODIR/file_list.json
echo "   {\"cur_dir\":\"%root_dir%/oppat_data/$PFX/$BASE\"}," >> $ODIR/file_list.json
echo "   {\"cur_tag\":\"${PFX}_$BASE\"}," >> $ODIR/file_list.json
echo "   {\"bin_file\":\"prf_trace.data\", \"txt_file\":\"prf_trace.txt\", \"tag\":\"%cur_tag%\", \"type\":\"PERF\"}," >> $ODIR/file_list.json
echo "   {\"bin_file\":\"prf_trace2.data\", \"txt_file\":\"prf_trace2.txt\", \"tag\":\"%cur_tag%\", \"type\":\"PERF\"}," >> $ODIR/file_list.json
echo "   {\"bin_file\":\"tc_trace.dat\",   \"txt_file\":\"tc_trace.txt\",  \"tag\":\"%cur_tag%\", \"type\":\"TRACE_CMD\"}," >> $ODIR/file_list.json
echo "   {\"bin_file\":\"prf_energy.txt\", \"txt_file\":\"prf_energy2.txt\", \"wait_file\":\"wait.txt\", \"tag\":\"%cur_tag%\", \"type\":\"LUA\"}," >> $ODIR/file_list.json
echo "   {\"bin_file\":\"spin.txt\", \"txt_file\":\"\", \"wait_file\":\"\", \"tag\":\"%cur_tag%\", \"type\":\"LUA\", \"lua_file\":\"spin.lua\", \"lua_rtn\":\"spin\"} " >> $ODIR/file_list.json
echo "  ]} " >> $ODIR/file_list.json

chmod a+rw $ODIR/*
chown -R 777 $ODIR

#sudo ../perf.sh script -I --header -i perf.data -F hw:comm,tid,pid,time,cpu,event,ip,sym,dso,symoff,period -F trace:comm,tid,pid,time,cpu,event,trace,ip,sym,period -i perf.data > perf.txt
