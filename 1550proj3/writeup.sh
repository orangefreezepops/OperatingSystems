#!/bin/bash
echo "--------------------------LRU----------------------------"
echo "16 pages 4KB size"
echo "3:5"
./vmsim -a lru -n 16 -p 4 -s 3:5 1.trace
echo " "
echo "5:3"
./vmsim -a lru -n 16 -p 4 -s 5:3 1.trace
echo " "
echo "7:9"
./vmsim -a lru -n 16 -p 4 -s 7:9 1.trace
echo " "
echo "9:7"
./vmsim -a lru -n 16 -p 4 -s 9:7 1.trace
echo " "
echo "1024 pages 4KB size"
echo " "
echo "3:5"
./vmsim -a lru -n 1024 -p 4 -s 3:5 1.trace
echo " "
echo "5:3"
./vmsim -a lru -n 1024 -p 4 -s 5:3 1.trace
echo " "
echo "7:9"
./vmsim -a lru -n 1024 -p 4 -s 7:9 1.trace
echo " "
echo "9:7"
./vmsim -a lru -n 1024 -p 4 -s 9:7 1.trace
echo " "
echo "16 pages 4MB size"
echo " "
echo "3:5"
./vmsim -a lru -n 16 -p 4096 -s 3:5 1.trace
echo " "
echo "5:3"
./vmsim -a lru -n 16 -p 4096 -s 5:3 1.trace
echo " "
echo "7:9"
./vmsim -a lru -n 16 -p 4096 -s 7:9 1.trace
echo " "
echo "9:7"
./vmsim -a lru -n 16 -p 4096 -s 9:7 1.trace
echo " "
echo "1024 pages 4MB size"
echo " "
echo "3:5"
./vmsim -a lru -n 1024 -p 4096 -s 3:5 1.trace
echo " "
echo "5:3"
./vmsim -a lru -n 1024 -p 4096 -s 5:3 1.trace
echo " "
echo "7:9"
./vmsim -a lru -n 1024 -p 4096 -s 7:9 1.trace
echo " "
echo "9:7"
./vmsim -a lru -n 1024 -p 4096 -s 9:7 1.trace
echo " "
echo "--------------------------OPT----------------------------"
echo "16 pages 4KB size"
echo " "
echo "3:5"
./vmsim -a opt -n 16 -p 4 -s 3:5 1.trace
echo " "
echo "5:3"
./vmsim -a opt -n 16 -p 4 -s 5:3 1.trace
echo " "
echo "7:9"
./vmsim -a opt -n 16 -p 4 -s 7:9 1.trace
echo " "
echo "9:7"
./vmsim -a opt -n 16 -p 4 -s 9:7 1.trace
echo " "
echo "1024 pages 4KB size"
echo " "
echo "3:5"
./vmsim -a opt -n 1024 -p 4 -s 3:5 1.trace
echo " "
echo "5:3"
./vmsim -a opt -n 1024 -p 4 -s 5:3 1.trace
echo " "
echo "7:9"
./vmsim -a opt -n 1024 -p 4 -s 7:9 1.trace
echo " "
echo "9:7"
./vmsim -a opt -n 1024 -p 4 -s 9:7 1.trace
echo " "
echo "16 pages 4MB size"
echo " "
echo "3:5"
./vmsim -a opt -n 16 -p 4096 -s 3:5 1.trace
echo " "
echo "5:3"
./vmsim -a opt -n 16 -p 4096 -s 5:3 1.trace
echo " "
echo "7:9"
./vmsim -a opt -n 16 -p 4096 -s 7:9 1.trace
echo " "
echo "9:7"
./vmsim -a opt -n 16 -p 4096 -s 9:7 1.trace
echo " "
echo "1024 pages 4MB size"
echo " "
echo "3:5"
./vmsim -a opt -n 1024 -p 4096 -s 3:5 1.trace
echo " "
echo "5:3"
./vmsim -a opt -n 1024 -p 4096 -s 5:3 1.trace
echo " "
echo "7:9"
./vmsim -a opt -n 1024 -p 4096 -s 7:9 1.trace
echo " "
echo "9:7"
./vmsim -a opt -n 1024 -p 4096 -s 9:7 1.trace
