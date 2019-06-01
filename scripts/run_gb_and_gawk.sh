#!/bin/bash

gb_bin=$1
gwk_scr=$2
odir=$3
$gb_bin | gawk -v rd_md="0" -v odir="$odir" -f $gwk_scr
#~/geekbench/Geekbench-4.3.3-Linux/geekbench4 |gawk -f gb_rd_output.gawk
