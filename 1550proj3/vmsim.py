#!/usr/bin/env python
import sys
import argparse
import pagetable


def setup():
    # command arguments: vmsim -n <numframes> -p <page size in KB> -s <memory split> <tracefile>
    if not((len(sys.argv) == 8)):
        print('\n    invalid arguments given')
        print('    should be in the form: vmsim.py -n <numframes> -p <page size in KB> -s <memory split> <tracefile>\n')

    # get numframes
    global numframes
    numframes = int(sys.argv[2])

    # get page size
    global pagesize
    pagesize = int(sys.argv[4])

    # get memory split
    global memsplit, split1, split2
    memsplit = sys.argv[6]
    partition = memsplit.split(':')
    split1 = int(partition[0])
    split2 = int(partition[1])

    # get trace file
    global tracefile
    tracefile = sys.argv[7]
    # open the trace file
    # with open(tracefile)

    print('the arguments are as follows: -n ', numframes, ' -p ',
            pagesize, ' -s ', split1, ':', split2, ' ', tracefile)

def second_chance():
    # second chance page replacement algorithm
    print('second chance')

def print_statistics():
    # printing metrics for the algorithm
    print('in here')

# main routine
if __name__ == "__main__":
    setup()
    second_chance()
    print_statistics()
