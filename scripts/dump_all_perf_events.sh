#!/bin/bash

SCR_DIR=`dirname "$(readlink -f "$0")"`
SAV_DIR=$SCR_DIR/../input_files/
echo "scr_dir= '$SCR_DIR'"

rm $SAV_DIR/perf_event_list_dump.txt

sudo /usr/bin/find /sys/kernel/debug/tracing/events/ -name format -exec $SCR_DIR/dump_fmt.sh {} \; >> $SAV_DIR/perf_event_list_dump.txt

