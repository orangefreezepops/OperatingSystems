#!/usr/bin/env python3
import sys
import math

pagetable0 = []     #page table object for process 0
pagetable1 = []     #page table object for process 1
queue0 = []         #queue for process 0 pages
queue1 = []         #queue for process 1 pages
pagesize = 0
pagesizestr = ''
offset = 0
pagenum = 0
numframes = 0
numframes0 = 0
numframes1 = 0
diskwrites = 0
pagefaults = 0
memaccesses = 0
tracefile = ''
index = 0

def setup():
    #global varibales I'll need to use
    global numframes
    global numframes0
    global numframes1
    global pagesize         #actual size in bits
    global pagesizestr      #argumant passed in
    global offset           #number of offset bits
    global pagenum          #page number portion of the hex string
    global tracefile

    # command arguments: vmsim -n <numframes> -p <page size in KB> -s <memory split> <tracefile>
    if not len(sys.argv) == 8:
        print('\n    invalid arguments given')
        print('    should be in the form: python vmsim.py -n <numframes> -p <page size in KB/MB> -s <memory split> <tracefile>\n')

    numframes = int(sys.argv[2])

    pagesizestr = sys.argv[4]
    if pagesizestr == '4':
        pagesize = 4096
    if pagesizestr == '8':
        pagesize = 8192
    if pagesizestr == '4096':
        pagesize = 32000000
    
    #calculate bits for the offset and page number
    offset = int(math.log2(pagesize))    #will be the least significant bits
    pagenum = int(32 - offset)           #will be the most significant bits

    #print('offset bits', offset)
    #print('page num bits', pagenum)
    
    # get memory split
    memsplit = sys.argv[6]
    partition = memsplit.split(':')
    split0 = int(partition[0])
    split1 = int(partition[1])

    #split and assign the frames according to ratio
    splitSum = split0 + split1
    frameDiv = numframes / splitSum
    numframes0 = math.ceil(frameDiv * split0)   #idk arbitrary jdgement just in case they arent pretty splits
    numframes1 = math.floor(frameDiv * split1) 

    # get trace file
    tracefile = sys.argv[7]

def second_chance():
    # second chance page replacement algorithm

    #set up process metric variables
    global memaccesses
    global tracefile

    # open the trace file
    with open(tracefile) as file:
        lines = file.readlines()
        #read each line
        for line in lines :
            #split by spaces, each line should have 3 pieces
            access = line.split(' ')
            inst = access[0]                         #instruction type
            addr = (int(access[1], 0)) >> offset     #address shifted by offest bc we just care about page num
            proc = int(access[2])                    #process
            
            #bin_pn = bin(addr)
            #bin_addr = bin(addr << 12)
            #full_addr = addr << offset
            #sanity check
            #print('type: ', type, ' addr: ', addr, ' proc: ', proc)
            #print('addr ', addr)
            #print('full addr ', full_addr)
            #print('binary addr ', bin_addr)
            #print('binary page num ', bin_pn)

            #create page table entry for each line
            new_entry = Entry(addr, 0, 0)
            if proc == 0:
                memaccesses += 1
                proc_zero(new_entry, inst)
            
            if proc == 1:
                memaccesses += 1
                proc_one(new_entry, inst)

def proc_zero(new_entry, inst):
    #set up performance metric variables
    global pagefaults
    global diskwrites
    global numframes0
    global pagetable0
    global queue0
    global index

    #check the length of the page table
    #if the number of entries < num frames for proc0
    if len(pagetable0) < numframes0:
        #is the address for the entry in the address queue
        if new_entry.get_address() not in queue0:
            #if its not in the pt add to pagetable
            #if a store set dirty bit
            if inst == 's':
                new_entry.set_dirtybit(1)
            pagetable0.append(new_entry)
            queue0.append(new_entry.get_address())
            #didnt exist so page fault ++
            pagefaults += 1
        else:
            #loop through page table
            #is the address for new entry == to any of the addresses in the pt
            for e in pagetable0:
                if e.get_address() == new_entry.get_address():
                    #if so set the ref bit to 1 because its been referenced
                    e.set_refbit(1)
                    #check if the instruction is a store
                    if (inst == 's'):
                        #if it is set the dirty bit
                        e.set_dirtybit(1)

    #if the number of entries >= the page frames for proc0
    else:
        #check that the address isnt in the page queue
        if new_entry.get_address() not in queue0:
            #if not we need to evict something
            #loop through local pagetable list obj
            for e in list(pagetable0):
                if (e.get_refbit() == 1):
                    #give him another shot
                    #make a new entry with an updated ref bit
                    update = Entry(e.get_address(), 0, e.get_dirtybit())
                    #remove the entry from its current position in page table 
                    #and add the updated entry to the end of the list
                    pagetable0.remove(e)
                    pagetable0.append(update)
                #found a 0
                else:
                    break
            
            #the first position will be the next evictable page
            #FIFO style 
            if pagetable0[0].get_dirtybit() == 1:
                #if it is you will have to write to disk
                diskwrites += 1
            #remove that sucka
            queue0.remove(pagetable0[0].get_address())
            pagetable0.remove(pagetable0[0])
            #done, evicted

            #get ready to add the new entry
        
            #if its a store instruction
            if inst == 's':
                #set the dirty bit
                new_entry.set_dirtybit(1)
            
            #add the new entry now
            pagetable0.append(new_entry)
            queue0.append(new_entry.get_address())
            #didnt exist so page fault ++
            pagefaults += 1
        
        #if the address is in the page table
        else:   
            #loop through page table
            for e in pagetable0:
                #find where new entry == current pt entry addresses 
                if e.get_address() == new_entry.get_address():
                    #if found, set the ref bit to 1 because its been referenced
                    e.set_refbit(1)
                    #check if the instruction is a store
                    if inst == 's':
                        #if it is set the dirty bit
                        e.set_dirtybit(1)

    #write to disk when you need to access an address thats not in memory 
    #and you run out of frames-> change the dirty bit and incriment
    #(when you evict a page that has a dirty bit == 1)

def proc_one(new_entry, inst):
    #set up performance metric variables
    global pagefaults 
    global diskwrites
    global numframes1
    global pagetable1
    global queue1
    global index

    #check the length of the page table
    #if the number of entries < num frames for proc0
    if len(pagetable1) < numframes1:
        #is the address for the entry in the address queue
        if new_entry.get_address() not in queue1:
            #if its not in the pt add to pagetable
            #if a store set dirty bit
            if inst == 's':
                new_entry.set_dirtybit(1)
            pagetable1.append(new_entry)
            queue1.append(new_entry.get_address())
            #didnt exist so page fault ++
            pagefaults += 1
        else:
            #loop through page table
            #is the address for new entry == to any of the addresses in the pt
            for e in pagetable1:
                if e.get_address() == new_entry.get_address():
                    #if so set the ref bit to 1 because its been referenced
                    e.set_refbit(1)
                    #check if the instruction is a store
                    if (inst == 's'):
                        #if it is set the dirty bit
                        e.set_dirtybit(1)

    #if the number of entries >= the page frames for proc1
    else:
        if new_entry.get_address() not in queue1:
            #if not we need to evict something
            #loop through local pagetable list obj
            for e in list(pagetable1):
                if (e.get_refbit() == 1):
                    #give him another shot
                    #make a new entry with an updated ref bit
                    update = Entry(e.get_address(), 0, e.get_dirtybit())
                    #remove the entry from its current position in page table 
                    #and add the updated entry to the end of the list
                    pagetable1.remove(e)
                    pagetable1.append(update)
                #found a 0
                else:
                    break
            
            #the first position will be the next evictable page
            #FIFO style 
            if pagetable1[0].get_dirtybit() == 1:
                #if it is you will have to write to disk
                diskwrites += 1
            #remove that sucka
            queue1.remove(pagetable1[0].get_address())
            pagetable1.remove(pagetable1[0])
            #done, evicted

            #get ready to add the new entry        
            #if its a store instruction
            if inst == 's':
                #set the dirty bit
                new_entry.set_dirtybit(1)
            
            #add the new entry now
            pagetable1.append(new_entry)
            queue1.append(new_entry.get_address())
            #didnt exist so page fault ++
            pagefaults += 1
        
        #if the adress is in the page table
        else:   
            #loop through queue
            #is the address for new entry == to any of the addresses in the queue
            for e in pagetable1:
                if e.get_address() == new_entry.get_address():
                    #if so set the ref bit to 1 because its been referenced
                    e.set_refbit(1)
                    #check if the instruction is a store
                    if inst == 's':
                        #if it is set the dirty bit
                        e.set_dirtybit(1)
    

def print_statistics():
    # printing metrics for the algorithm
    print('Algorithm: Second Chance')
    print('Number of frames: ' + str(numframes))
    print('Page size: ' + pagesizestr + ' KB')
    print('Total memory accesses: ' + str(memaccesses))
    print('Total page faults: ' + str(pagefaults))
    print('Total writes to disk: ' + str(diskwrites))


#---------------Entry Class bc Auto Grader doesn't like multiple files---------meh---
class Entry:
    def __init__(self, address, refbit, dirtybit):
        #address 
        self.__address = address
        #reference bit
        self.__refbit = refbit
        #dirty bit
        self.__dirtybit = dirtybit

    #getters and setters
    def get_address(self):
        return self.__address

    def set_address(self, addr):
        self.__address = addr

    def get_refbit(self):
        return self.__refbit

    def set_refbit(self, rb):
        self.__refbit = rb

    def get_dirtybit(self):
        return self.__dirtybit

    def set_dirtybit(self, db):
        self.__dirtybit = db

# main routine
if __name__ == "__main__":
    setup()
    second_chance()
    print_statistics()
