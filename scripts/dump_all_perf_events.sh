#!/bin/bash

SCR_DIR=`dirname "$(readlink -f "$0")"`
echo "scr_dir= '$SCR_DIR'"

if [ "x$1" == "x" ]; then
  SAV_DIR=$SCR_DIR/../input_files/
else
  SAV_DIR=$1
fi

chmod +x $SCR_DIR/dump_fmt.sh
rm $SAV_DIR/perf_event_list_dump.txt

sudo /usr/bin/find /sys/kernel/debug/tracing/events/ -name format -exec $SCR_DIR/dump_fmt.sh {} \; >> $SAV_DIR/perf_event_list_dump.txt

