#!/usr/bin/bash
cd ./build
#./createimage --extended bootblock main lock2 print1 sleep print2 lock1 timer
./createimage --extended bootblock main lock2 print1 sleep thread_test fly print2 lock1 timer
#./createimage --extended bootblock main print1 print2 lock1 lock2 fly
cd ..