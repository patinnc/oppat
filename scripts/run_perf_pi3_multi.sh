#!/bin/bash

TRC_CMD=/home/pi/ppat/oppat/bin/trace-cmd
PRF_CMD=/usr/bin/perf_4.9
PRF_CMD=/home/pi/bin/perf
BASE=mem_bw5_pi_multi3
PFX=arm
NUM_CPUS=`cat /proc/cpuinfo | grep processor |wc -l`
SCR_DIR=`dirname "$(readlink -f "$0")"`
BIN_DIR=$SCR_DIR/../bin/
echo "SCR_DIR= $SCR_DIR and BIN_DIR=$BIN_DIR"

ODIR=../oppat_data/$PFX/$BASE
mkdir -p $ODIR


if [ -f $ODIR/file_list.json ]; then
  rm $ODIR/file_list.json
fi
rm $ODIR/*.txt
rm $ODIR/*.data
rm $ODIR/*.dat
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


#sudo ../perf.sh record -a -e "{cycles,instructions}" -e sched:sched_switch -e offcore_response.all_code_rd.l3_miss.local_dram/freq:100/ -o $BASE.data sleep 0.5
#sudo ../perf.sh record -a -e sched:sched_switch -F 100 -e cpu/offcore_response.all_code_rd.l3_miss.local_dram/ -o $BASE.data sleep 0.5
#sudo ../perf.sh record -a  -e sched:sched_switch --group -e "{cycles,instructions}"  -o $BASE.data sleep 0.5

# trace-cmd clocks:
# [local] global counter uptime perf mono mono_raw boot x86-tsc
#$TRC_CMD record -C mono -e power:powernv_throttle -e i915:intel_gpu_freq_change -e i915:i915_flip_complete -e thermal:thermal_temperature -e power:cpu_idle -o $ODIR/tc_trace.dat > $ODIR/trace_cmd_out.txt &
#$TRC_CMD record -C mono -e power:powernv_throttle -e thermal:thermal_temperature -e power:cpu_idle -o $ODIR/tc_trace.dat > $ODIR/trace_cmd_out.txt &
$TRC_CMD record -C mono -e power:powernv_throttle -e power:cpu_frequency -e clk:clk_set_rate_complete  -e thermal:thermal_temperature -e power:cpu_idle -o $ODIR/tc_trace.dat > $ODIR/trace_cmd_out.txt &
PID_TRC_CMD=$!


# it takes a few seconds to get the trace cmd trheds up
ck_cmd_pid_threads_oper $TRC_CMD $PID_TRC_CMD 1 -le $NUM_CPUS
echo started $TRC_CMD

#$PRF_CMD record -a -g -e sched:sched_switch -e "{cycles,instructions}"  -o prf.data sleep 0.5
#$PRF_CMD record -a -g -F 992 -e "{cpu-clock,cycles,instructions}"  -o prf_$BASE.data sleep 0.5
#$PRF_CMD record -a -g  -e "{cpu-clock/freq=992/,cycles,instructions}"  -o prf_$BASE.data ./spin.x  -t 4 -w spin

#$PRF_CMD record -a -g  -e "{ref-cycles,cycles,instructions}"  -o prf_$BASE.data ./spin.x  -t 4 -w spin

#$PRF_CMD record -a -g  -e ref-cycles,cycles,instructions  -o prf_$BASE.data ./spin.x  -t 4 -w spin

#$PRF_CMD record -a -g -e sched:sched_switch -e "{cpu-clock/freq=992/,ref-cycles,cycles,instructions}:G"  -o prf_$BASE.data ./spin.x  -t 4 -w spin

#$PRF_CMD record -a -g -F 997 -e "{ref-cycles,cycles,instructions}"  -o prf_$BASE.data ./spin.x  -t 4 -w spin

#$PRF_CMD record -a -g -e sched:sched_switch -e "{ref-cycles/freq=103/,cycles,instructions}"  -o prf_$BASE.data ./spin.x  -t 4 -w spin

# ../perf.sh stat -a -e power/energy-pkg/,power/energy-cores/,cycles -v -I 1000 sleep 1000
#  sudo ../perf.sh stat -a -e power/energy-pkg/,power/energy-cores/,power/energy-gpu/,power/energy-ram/  -I 1000 -x '\t'  sleep 1000
WAIT_FILE=wait.pid.txt
rm $WAIT_FILE
#$PRF_CMD stat -a -e power/energy-pkg/,power/energy-cores/,power/energy-gpu/,power/energy-ram/,uncore_cbox_0/clockticks/,uncore_cbox_1/clockticks/,uncore_imc/data_reads/,uncore_imc/data_writes/ -I 20 -x "\t" -o $ODIR/prf_energy.txt $BIN_DIR/wait.x $ODIR/prf_energy2.txt > $ODIR/wait.txt &
$BIN_DIR/wait.x $ODIR/prf_energy2.txt > $ODIR/wait.txt &
while [ ! -f $WAIT_FILE ]
do
  echo waiting for $WAIT_FILE to exist
  sleep 0.1
done

#
#$PRF_CMD record -a -T -k CLOCK_MONOTONIC -e power:powernv_throttle/call-graph=no/ -e thermal:thermal_temperature/call-graph=no/ -e power:cpu_idle/call-graph=no/ -e cpu-clock,power:cpu_frequency/call-graph=no/ -g -e sched:sched_switch -e "{cycles,instructions}"  -o $ODIR/prf_trace.data $BIN_DIR/spin.x -t 4 -w mem_bw > $ODIR/spin.txt
#$PRF_CMD record -a -k monotonic -e power:powernv_throttle -e power:cpu_frequency -e clk:clk_set_rate_complete  -e "{r19/freq=1000/,re7,rc0}:S"  -e thermal:thermal_temperature -e power:cpu_idle -e cpu-clock,power:cpu_frequency  -o $ODIR/prf_trace2.data  &
#PRF_CMD_PID=$!
#$PRF_CMD record -a -k monotonic -e power:powernv_throttle -e power:cpu_frequency -e clk:clk_set_rate_complete  -e thermal:thermal_temperature -e power:cpu_idle -o $ODIR/prf_trace1.data  &
#PRF_CMD_PID1=$!

# check if /proc/sys/kernel/nmi_watchdog is 1 which indicates kernel might be using an event 
# events below from DDI0500J_cortex_a53_trm.pdf
#
# r16=L2 dcache access, 
# r17=L2 dcache refill,
# r19=bus access,
# r1d=bus cycles,
# r60=BUS_ACCESS_LD,
# r61=BUS_ACCESS_ST,
# rC0=External memory request.
# rc1=Non-cacheable external memory request.
# rc2=Linefill because of prefetch.
# rc4=entering read allocate mode
# re7=stall cycle due to load miss
#$PRF_CMD record -a -k monotonic -F 997 -e "{cpu-clock,cycles,instructions,r16,r17,r19,r1d,r60}:S" -o $ODIR/prf_trace2.data  &
#$PRF_CMD record -a -k monotonic -F 997 -e "{cpu-clock/freq=997/,cycles,instructions,r16,r17,rc0,rc2,re7}:S" -o $ODIR/prf_trace2.data  &
#0x0D	BR_IMMED_RETIRED	Instruction architecturally executed, immediate branch.
#0x0E	BR_RETURN_RETIRED	Instruction architecturally executed, condition code check pass, procedure return.
#0x10	BR_MIS_PRED	Mispredicted or not predicted branch speculatively executed.
#0x12	BR_PRED	Predictable branch speculatively executed.
#0xC9	BR_COND	Conditional branch executed.
#0xCC	BR_COND_MISPRED	Conditional branch mispredicted.


$PRF_CMD record --running-time -a -k monotonic -F 997 -e "{cpu-clock,cycles,instructions,r16,r17,rc0,rc2,re7}:S" -e "{cpu-clock,cycles,instructions,r0e,r10,r12,rc9,rcc}:S"  -o $ODIR/prf_trace2.data  &
PRF_CMD_PID2=$!

#$PRF_CMD record -a -k monotonic -e power:powernv_throttle/call-graph=no/ -e power:cpu_frequency/call-graph=no/ -e clk:clk_set_rate_complete/call-graph=no/  -e "{r19/call-graph=no,freq=10000/,re7,rc0}:S"  -e thermal:thermal_temperature/call-graph=no/ -e power:cpu_idle/call-graph=no/ -e cpu-clock,power:cpu_frequency/call-graph=no/ -g -e sched:sched_switch -e "{cycles,instructions}"  -o $ODIR/prf_trace.data $BIN_DIR/spin.x -t 4 -w mem_bw > $ODIR/spin.txt
#$PRF_CMD record -a -k monotonic  -g -e sched:sched_switch  -e "{cpu-clock,cycles,instructions}"  -o $ODIR/prf_trace.data $BIN_DIR/spin.x -t 4 -w mem_bw > $ODIR/spin.txt
$PRF_CMD record -a -k monotonic  -g -e sched:sched_switch,cpu-clock  -o $ODIR/prf_trace.data $BIN_DIR/spin.x -t 4 -w mem_bw -b 32 -s 20m > $ODIR/spin.txt

kill -2 `cat $WAIT_FILE`
kill -2 $PID_TRC_CMD 
#kill -2 $PRF_CMD_PID1
#wait $PRF_CMD_PID1
kill -2 $PRF_CMD_PID2
wait $PRF_CMD_PID2
#wait $PID_TRC_CMD

ck_cmd_pid_threads_oper $TRC_CMD $PID_TRC_CMD 2 -gt 1

#exit
#sudo ../perf.sh record -a -g -e sched:sched_switch   -o $BASE.data sleep 0.5
trcFopts=" -F trace:comm,tid,pid,time,cpu,event,trace,period,callindent "
hwFopts="  -F hw:comm,tid,pid,time,cpu,period,event,ip,sym,dso,symoff,brstack,brstacksym,flags,callindent "
trcFopts=" -F trace:comm,tid,pid,time,cpu,period,event,sym,dso,symoff,trace,brstack,brstacksym,flags,bpf-output,callindent"
trcFopts=" -F trace:comm,tid,pid,time,cpu,period,event,ip,sym,dso,symoff,trace,brstack,brstacksym,flags,bpf-output,callindent"
rawFopts="     -F raw:comm,tid,pid,time,cpu,period,event,ip,sym,dso,symoff,flags "
swFopts="     -F sw:comm,tid,pid,time,cpu,period,event,ip,sym,dso,symoff,flags,callindent "
Fopts="     -F comm,tid,pid,time,cpu,period,event,ip,sym,dso,symoff,flags,callindent "
Fopts=" -F comm,tid,pid,time,cpu,period,event,ip,sym,dso,symoff,trace,flags,callindent"
#trcFopts=

echo $PRF_CMD script -I --header -f $hwFopts $trcFopts -i $ODIR/prf_trace.data  
#$PRF_CMD script -I --header  -i $ODIR/prf_trace.data  > $ODIR/prf_trace.txt
echo do prf_trace
echo $PRF_CMD script -I --header -f $Fopts -i $ODIR/prf_trace.data  _ $ODIR/prf_trace.txt
#$PRF_CMD script -I --header -f $swFopts $trcFopts -i $ODIR/prf_trace.data  > $ODIR/prf_trace.txt
$PRF_CMD script -I --header -f $Fopts -i $ODIR/prf_trace.data  > $ODIR/prf_trace.txt
#$PRF_CMD script -I --header  -i $ODIR/prf_trace.data  > $ODIR/prf_trace.txt
#echo do prf_trace1
#$PRF_CMD script -I --header -f $trcFopts -i $ODIR/prf_trace1.data  > $ODIR/prf_trace1.txt
echo do prf_trace2
#$PRF_CMD script -I --header  -i $ODIR/prf_trace2.data  > $ODIR/prf_trace2.txt
echo $PRF_CMD script -I --header -f $Fopts -i $ODIR/prf_trace2.data  _ $ODIR/prf_trace2.txt
#$PRF_CMD script -I --header -f $swFopts $rawFopts  -i $ODIR/prf_trace2.data  > $ODIR/prf_trace2.txt
$PRF_CMD script -I --header -f $Fopts  -i $ODIR/prf_trace2.data  > $ODIR/prf_trace2.txt
#$PRF_CMD script -I --header -f $hwFopts $trcFopts -D -i $ODIR/prf_trace.data > $ODIR/prf_d.txt
#$PRF_CMD script -I --ns --header -f $hwFopts $trcFopts -i $ODIR/prf_trace.data > $ODIR/prf_trace.txt
#$PRF_CMD script -I --ns --header -f $hwFopts $trcFopts -D -i $ODIR/prf_trace.data > $ODIR/prf_d.txt
$TRC_CMD report -t -i $ODIR/tc_trace.dat > $ODIR/tc_trace.txt
#chmod a+rw $ODIR/tc*
#chmod a+rw $ODIR/*.json
#chmod a+rw $ODIR/*.txt
rm $WAIT_FILE
rm $ODIR/prf_*.data.old

chmod a+rwx $SCR_DIR/dump_all_perf_events.sh
bash $SCR_DIR/dump_all_perf_events.sh $ODIR

echo "{\"file_list\":[" > $ODIR/file_list.json
echo "   {\"cur_dir\":\"%root_dir%/oppat_data/$PFX/$BASE\"}," >> $ODIR/file_list.json
echo "   {\"cur_tag\":\"${PFX}_$BASE\"}," >> $ODIR/file_list.json
echo "   {\"bin_file\":\"prf_trace.data\", \"txt_file\":\"prf_trace.txt\", \"tag\":\"%cur_tag%\", \"type\":\"PERF\"}," >> $ODIR/file_list.json
echo "   {\"bin_file\":\"prf_trace2.data\", \"txt_file\":\"prf_trace2.txt\", \"tag\":\"%cur_tag%\", \"type\":\"PERF\"}," >> $ODIR/file_list.json
echo "   {\"bin_file\":\"tc_trace.dat\",   \"txt_file\":\"tc_trace.txt\",  \"tag\":\"%cur_tag%\", \"type\":\"TRACE_CMD\"}," >> $ODIR/file_list.json
echo "   {\"bin_file\":\"\", \"txt_file\":\"prf_energy2.txt\", \"wait_file\":\"wait.txt\", \"tag\":\"%cur_tag%\", \"type\":\"LUA\"}," >> $ODIR/file_list.json
echo "   {\"bin_file\":\"spin.txt\", \"txt_file\":\"\", \"wait_file\":\"\", \"tag\":\"%cur_tag%\", \"type\":\"LUA\", \"lua_file\":\"spin.lua\", \"lua_rtn\":\"spin\"} " >> $ODIR/file_list.json

echo "  ]} " >> $ODIR/file_list.json


chmod a+rw $ODIR/*

#sudo ../perf.sh script -I --header -i perf.data -F hw:comm,tid,pid,time,cpu,event,ip,sym,dso,symoff,period -F trace:comm,tid,pid,time,cpu,event,trace,ip,sym,period -i perf.data > perf.txt
