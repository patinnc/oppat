#!/bin/bash

MTUNE=" -mtune=cortex-a53 "
MTUNE=

OSTYPE=`uname`
STATIC1=" -static "
STATIC2=" -Wl,--whole-archive "
STATIC3=" -Wl,--no-whole-archive "
if [ "$OSTYPE" == "Darwin" ]; then
 STATIC1=
 STATIC2=
 STATIC3=
fi
echo "OS= $OSTYPE"
g++ $MTUNE -std=c++11 -Iinc -O4 -g spin_wait/clocks.cpp src/utils.cpp -o bin/clocks.x -lpthread
g++ $MTUNE -std=c++11 $STATIC1  -Iinc -O4 -g spin_wait/spin.cpp src/utils.cpp src/utils2.cpp spin_wait/trace_marker.cpp -o bin/spin.x -pthread $STATIC2 -lpthread $STATIC3
ls -l bin/spin.x

g++ $MTUNE -std=c++11 -Iinc -O3 -g spin_wait/wait.cpp src/utils.cpp -o bin/wait.x
ls -l bin/wait.x


