#!/bin/bash

MTUNE=" -mtune=cortex-a53 "

g++ $MTUNE -std=c++11 -Iinc -O4 -g spin_wait/clocks.cpp -o bin/clocks.x -lpthread
g++ $MTUNE -std=c++11 -Iinc -O4 -g spin_wait/spin.cpp src/utils.cpp spin_wait/trace_marker.cpp -o bin/spin.x -lpthread
ls -l bin/spin.x

g++ $MTUNE -std=c++11 -Iinc -O3 -g spin_wait/wait.cpp src/utils.cpp -o bin/wait.x
ls -l bin/wait.x


