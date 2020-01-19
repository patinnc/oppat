#!/bin/bash

TRC_CMD=~/bin/trace-cmd
PRF_CMD=~/perf.sh
BASE=disk_rdwr
BASE=disk_rd
BASE=disk_wr
BASE=disk_wr_dir
BASE=disk_rd_dir
BASE=disk_rdwr_dir
if [ "x$1" != "x" ]; then
 echo "got arg1= $1"
 BASE=$1
fi
PFX=lnx
NUM_CPUS=`cat /proc/cpuinfo | grep processor |wc -l`
SCR_DIR=`dirname "$(readlink -f "$0")"`
BIN_DIR=$SCR_DIR/../bin/
echo "SCR_DIR= $SCR_DIR and BIN_DIR=$BIN_DIR"

ODIR=../oppat_data/$PFX/$BASE
mkdir -p $ODIR

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
#sudo nohup ../trace-cmd/tracecmd/trace-cmd record -e sched:sched_switch 2>&1 > trace_cmd_out.txt &
#sudo ../trace-cmd/tracecmd/trace-cmd record -e sched:sched_switch > trace_cmd_out.txt &

# trace-cmd clocks:
# [local] global counter uptime perf mono mono_raw boot x86-tsc
#$TRC_CMD record -C local -e sched:sched_switch -o trc$BASE.dat > trace_cmd_out.txt &
#$TRC_CMD record -C local -e thermal:thermal_power_cpu_get_power -e power:cpu_frequency -e power:cpu_idle -o trc$BASE.dat > trace_cmd_out.txt &
#$TRC_CMD record -C local -e thermal:thermal_power_cpu_get_power -e power:cpu_idle -o trc$BASE.dat > trace_cmd_out.txt &
#$TRC_CMD record -C mono -e syscalls:sys_enter_write -e syscalls:sys_exit_write -e syscalls:sys_enter_read -e syscalls:sys_exit_read -e power:powernv_throttle -e i915:intel_gpu_freq_change -e i915:i915_flip_complete -e thermal:thermal_temperature -e power:cpu_idle -o trc$BASE.dat > trace_cmd_out.txt &

$TRC_CMD record -C mono -e block:block_rq_issue -e block:block_rq_insert -e block:block_rq_complete -e power:powernv_throttle -e i915:intel_gpu_freq_change -e ext4:ext4_direct_IO_enter -e ext4:ext4_direct_IO_exit -e ext4:ext4_da_write_begin -e ext4:ext4_da_write_end -e ext4:ext4_write_begin -e ext4:ext4_write_end -e i915:i915_flip_complete -e thermal:thermal_temperature -e power:cpu_idle -o $ODIR/tc_trace.dat > $ODIR/trace_cmd_out.txt &
PID_TRC_CMD=$!


# it takes a few seconds to get the trace cmd trheds up
ck_cmd_pid_threads_oper $TRC_CMD $PID_TRC_CMD 1 -le $NUM_CPUS
echo started $TRC_CMD

#$PRF_CMD record -a -g -e sched:sched_switch -e "{cycles,instructions}"  -o prf.data sleep 0.5
#$PRF_CMD record -a -g -F 992 -e "{cpu-clock,cycles,instructions}"  -o prf_$BASE.data sleep 0.5
#$PRF_CMD record -a -g  -e "{cpu-clock/freq=992/,cycles,instructions}"  -o prf_$BASE.data ./spin.x -t 4 -w spin
#$PRF_CMD record -a -g  -e "{ref-cycles,cycles,instructions}"  -o prf_$BASE.data ./spin.x  -t 4 -w spin
#$PRF_CMD record -a -g  -e ref-cycles,cycles,instructions  -o prf_$BASE.data ./spin.x  -t 4 -w spin
#$PRF_CMD record -a -g -e sched:sched_switch -e "{cpu-clock/freq=992/,ref-cycles,cycles,instructions}:G"  -o prf_$BASE.data ./spin.x  -t 4 -w spin
#$PRF_CMD record -a -g -F 997 -e "{ref-cycles,cycles,instructions}"  -o prf_$BASE.data ./spin.x  -t 4 -w spin
#$PRF_CMD record -a -g -e sched:sched_switch -e "{ref-cycles/freq=103/,cycles,instructions}"  -o prf_$BASE.data ./spin.x  -t 4 -w spin
# ../perf.sh stat -a -e power/energy-pkg/,power/energy-cores/,cycles -v -I 1000 sleep 1000
#  sudo ../perf.sh stat -a -e power/energy-pkg/,power/energy-cores/,power/energy-gpu/,power/energy-ram/  -I 1000 -x '\t'  sleep 1000
WAIT_FILE=wait.pid.txt
rm $WAIT_FILE
$PRF_CMD stat -a -e power/energy-pkg/,power/energy-cores/,power/energy-gpu/,power/energy-ram/,uncore_cbox_0/clockticks/,uncore_cbox_1/clockticks/,uncore_imc/data_reads/,uncore_imc/data_writes/ -I 20 -x "\t" -o $ODIR/prf_energy.txt $BIN_DIR/wait.x $ODIR/prf_energy2.txt > $ODIR/wait.txt &
while [ ! -f $WAIT_FILE ]
do
  echo waiting for $WAIT_FILE to exist
  sleep 0.1
done

#
#$PRF_CMD record -a  -e power:cpu_frequency/call-graph=no/ -g -e sched:sched_switch -e "{ref-cycles/freq=997/,cycles,instructions}"  -o prf_$BASE.data $BIN_DIR/spin.x  -t 4 -w spin
$PRF_CMD record -a -k CLOCK_MONOTONIC  -e cpu-clock,power:cpu_frequency/call-graph=no/ -g -e sched:sched_switch -e "{ref-cycles/freq=997/,cycles,instructions}"  -o $ODIR/prf_trace.data $BIN_DIR/spin.x -t 4 -w $BASE > $ODIR/spin.txt

kill -2 `cat $WAIT_FILE`
kill -2 $PID_TRC_CMD 

ck_cmd_pid_threads_oper $TRC_CMD $PID_TRC_CMD 2 -gt 1

#exit
#sudo ../perf.sh record -a -g -e sched:sched_switch   -o $BASE.data sleep 0.5
trcFopts=" -F trace:comm,tid,pid,time,cpu,event,trace,period,callindent "
hwFopts="     -F hw:comm,tid,pid,time,cpu,period,event,ip,sym,dso,symoff,brstack,brstacksym,flags "
trcFopts=" -F trace:comm,tid,pid,time,cpu,period,event,sym,dso,symoff,trace,brstack,brstacksym,flags,bpf-output,callindent"
trcFopts=" -F trace:comm,tid,pid,time,cpu,period,event,ip,sym,dso,symoff,trace,brstack,brstacksym,flags,bpf-output,callindent"
#trcFopts=

$PRF_CMD script -I --ns --header -f $hwFopts $trcFopts -i $ODIR/prf_trace.data > $ODIR/prf_trace.txt
#../perf.sh script -I --ns --header -f $hwFopts $trcFopts -D -i prf_trace.data > prf_d.txt
$TRC_CMD report -t -i $ODIR/tc_trace.dat > $ODIR/tc_trace.txt
#chmod a+rw $ODIR/tc*
#chmod a+rw $ODIR/*.json
#chmod a+rw $ODIR/*.txt
rm $WAIT_FILE
rm $ODIR/prf_*.data.old

$SCR_DIR/dump_all_perf_events.sh $ODIR

echo "{\"file_list\":[" > $ODIR/file_list.json
echo "   {\"cur_dir\":\"%root_dir%/oppat_data/$PFX/$BASE\"}," >> $ODIR/file_list.json
echo "   {\"cur_tag\":\"${PFX}_$BASE\"}," >> $ODIR/file_list.json
echo "   {\"bin_file\":\"prf_trace.data\", \"txt_file\":\"prf_trace.txt\", \"tag\":\"%cur_tag%\", \"type\":\"PERF\"}," >> $ODIR/file_list.json
echo "   {\"bin_file\":\"tc_trace.dat\",   \"txt_file\":\"tc_trace.txt\",  \"tag\":\"%cur_tag%\", \"type\":\"TRACE_CMD\"}," >> $ODIR/file_list.json
echo "   {\"bin_file\":\"prf_energy.txt\", \"txt_file\":\"prf_energy2.txt\", \"wait_file\":\"wait.txt\", \"tag\":\"%cur_tag%\", \"type\":\"LUA\"}" >> $ODIR/file_list.json
echo "  ]} " >> $ODIR/file_list.json

chmod a+rw $ODIR/*

#sudo ../perf.sh script -I --header -i perf.data -F hw:comm,tid,pid,time,cpu,event,ip,sym,dso,symoff,period -F trace:comm,tid,pid,time,cpu,event,trace,ip,sym,period -i perf.data > perf.txt
