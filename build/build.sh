#!/usr/bin/env sh

gcc -c -O2 -Wall -o tick.o ../src/tick.c -I../src -lpthread
ar -r libtick.a tick.o
ranlib libtick.a

g++ -O2 -Wall ../src/main.c -L. -ltick -lpthread
