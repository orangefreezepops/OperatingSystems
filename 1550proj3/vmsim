#!/usr/bin/env python3
import sys
import math
from collections import deque

#globals for both page replacement schemes
pagetable0 = []     #page table object for proc 0
pagetable1 = []     #page table object for proc 1

#keeping queues helps locate address rather and entire Entry objects
#e.g. if the same address gets accessed but the instructions are different
#the PTE wont be found in the page table because they arent the same object,
#these queues mitigate that problem
queue0 = []         #queue to keep track of proc 0 addresses
queue1 = []         #queue to keep track of proc 1 addresses

#hash tables for OPT page replacement pre-processing
opthash0 = {}       #hash table for opt proc 0
opthash1 = {}       #hash table for opt proc 1

#General globals/metrics 
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
algorithm = ''

def setup():
    #global varibales I'll need to use
    global numframes        #number of frames total
    global numframes0       #number of frames given to proc0
    global numframes1       #number of frames given to proc1
    global pagesize         #actual size in bits
    global pagesizestr      #argumant passed in
    global offset           #number of offset bits
    global pagenum          #page number portion of the hex string
    global algorithm        #page replacement algorithm to run
    global tracefile        #file being read

    # command arguments: vmsim -a <lru|opt> -n <numframes> -p <page size in KB> -s <memory split> <tracefile>
    if not len(sys.argv) == 10:
        print('\n    invalid arguments given')
        print('    should be in the form: python vmsim.py -a <lru|opt> -n <numframes> -p <page size in KB/MB> -s <memory split> <tracefile>\n')

    algorithm = sys.argv[2]

    numframes = int(sys.argv[4])

    pagesizestr = sys.argv[6]
    if pagesizestr == '4':
        pagesize = 4096
    if pagesizestr == '8':
        pagesize = 8192
    if pagesizestr == '4096':
        pagesize = 4194304
    
    #calculate bits for the offset and page number
    offset = int(math.log2(pagesize))    #will be the least significant bits
    pagenum = int(32 - offset)           #will be the most significant bits
    
    # get memory split
    memsplit = sys.argv[8]
    partition = memsplit.split(':')
    split0 = int(partition[0])
    split1 = int(partition[1])

    #split and assign the frames according to ratio
    splitsum = split0 + split1
    framediv = numframes / splitsum
    
    #idk arbitrary judgement just in case they arent pretty splits
    numframes0 = math.floor(framediv * split0)   
    numframes1 = math.ceil(framediv * split1) 

    # get trace file
    tracefile = sys.argv[9]

    #load hashtables if optimal was chosen
    if algorithm == 'opt':
        load_hash_tables()

#loading the hash tables
def load_hash_tables():
    global opthash0
    global opthash1
    global tracefile

    # open the trace file
    with open(tracefile) as file:
        lines = file.readlines()
        #read each line
        linecount = 0
        for line in lines :
            #increment the line number
            linecount += 1

            #split by spaces, each line should have 3 pieces
            access = line.split(' ')
            addr = (int(access[1], 0)) >> offset     #address shifted by offest bc we just care about page num
            proc = int(access[2])                    #process

            #if process 0, use hashtable 0
            if proc == 0:
                #if its a new address, add the "key": address "value": linenumber 
                #pair with the line number being a linked list 
                if addr not in opthash0:
                    #initialized deque
                    opthash0[addr] = deque()
                    #append the line
                    opthash0[addr].append(linecount)
                #if the address already resident in the hashtable
                else:
                    #append this linenumber to the end of the value list
                    opthash0[addr].append(linecount)
            
            #if process 1, use hashtable 1
            if proc == 1:
                #same deal as above but for hashtable 1
                if addr not in opthash1:
                    opthash1[addr] = deque()
                    opthash1[addr].append(linecount)
                else:
                    opthash1[addr].append(linecount)

    #print(opthash0)
    #print(opthash1)

def choose_page_replacement():
    #set up process metric variables
    global memaccesses
    global tracefile
    global algorithm
    global pagefaults
    global diskwrites
    
    #process 0
    global numframes0
    global pagetable0
    global queue0
    global opthash0
    
    #process 1
    global numframes1
    global pagetable1
    global queue1
    global opthash1


    # open the trace file
    with open(tracefile) as file:
        lines = file.readlines()
        #read each line
        for line in lines :
            #split by spaces, each line should have 3 pieces
            access = line.split(' ')
            instruction = access[0]                  #instruction type
            addr = (int(access[1], 0)) >> offset     #address shifted by offest bc we just care about page num
            proc = int(access[2])                    #process

            #create page table entry for each line
            new_entry = Entry(addr, 0)

            #split up runs by process 
            if proc == 0:
                memaccesses += 1
                if algorithm == 'lru':
                    lru_proc(new_entry, instruction, pagetable0, numframes0, queue0)
                if algorithm == 'opt':
                    opt_proc(new_entry, instruction, pagetable0, numframes0, queue0, opthash0)
            
            if proc == 1:
                memaccesses += 1
                if algorithm == 'lru':
                    lru_proc(new_entry, instruction, pagetable1, numframes1, queue1)
                if algorithm == 'opt':
                    opt_proc(new_entry, instruction, pagetable1, numframes1, queue1, opthash1)


def lru_proc(new_entry, instruction, pagetable, numframes, queue):
    #set up performance metric variables
    global pagefaults
    global diskwrites
    
    #check length of the page table
    #if its less than the allotted frames for process 0 we can add without eviction
    if len(pagetable) < numframes:
        #is the address already in the LRU queue?
        if new_entry.get_address() not in queue:
            #if not
            #check if the instructon is a store 
            if instruction == 's':
                #set dirty bit
                new_entry.set_dirtybit(1)

            #add it to the page table and the queue of used addresses
            pagetable.append(new_entry)
            queue.append(new_entry.get_address())

            #since it didn't exist
            pagefaults += 1

        #otherwise check the pagetable for the matching address
        else:
            #loop through pagetable
            for pte in pagetable:
                #found a match
                if pte.get_address() == new_entry.get_address():

                    #set the dirty bit to the existing entries dirty flag
                    new_entry.set_dirtybit(pte.get_dirtybit())

                    #check instruction type
                    if instruction == 's':
                        new_entry.set_dirtybit(1)

                    #if the entry is already in the page table
                    #remove it from its current position and add to the end
                    pagetable.remove(pte)
                    pagetable.append(new_entry)

    #when the number of pages is at the maximum allowed
    else:
        #check to see if the address is already present in the queue
        if new_entry.get_address() not in queue:
            #we need to evict a page
            #in this case the front of the pagetable will be the last recently used

            #check to see if that pte was dirty
            if pagetable[0].get_dirtybit() == 1:
                diskwrites += 1
            
            #remove the sucka from pagetable and queue
            queue.remove(pagetable[0].get_address())
            pagetable.remove(pagetable[0])

            #now we need to add the new entry
            #check the instruction
            if instruction == 's':
                new_entry.set_dirtybit(1)
            
            #add to pagetable and queue
            pagetable.append(new_entry)
            queue.append(new_entry.get_address())

            #increase page fault
            pagefaults += 1
        
        #the address existed
        else:
            #same type of process as before
            #just update its positioning
            for pte in pagetable:
                if pte.get_address() == new_entry.get_address():
                    #check the instruction again because it might be
                    #different this time

                    new_entry.set_dirtybit(pte.get_dirtybit())

                    #if its changed to a dirty, set it
                    if instruction == 's':
                        new_entry.set_dirtybit(1)

                    #remove it from its current position and add to the end
                    pagetable.remove(pte)
                    pagetable.append(new_entry)


def opt_proc(new_entry, instruction, pagetable, numframes, queue, opthash):
    global pagefaults
    global diskwrites

    #if we can add a new page to the page table without eviction
    if len(pagetable) < numframes:

        #first check that it doesn't already exist
        if new_entry.get_address() not in queue:
            
            #we are good to add to the pagetable
            #check instruction
            if instruction == 's':
                #Set dirty bit if its a store
                new_entry.set_dirtybit(1)
            
            #add to the page table and the address queue
            pagetable.append(new_entry)
            queue.append(new_entry.get_address())

            #remove this entry from the addresses hash table list
            #because its been seen
            opthash[new_entry.get_address()].popleft()

            #increment page fault 
            pagefaults += 1

        #else the address already exists in the page table
        else:
            #loop through page table to find matching entry
            for pte in pagetable:
                if pte.get_address() == new_entry.get_address():
                    #set the dirtybit to the old pte's dirtybit
                    new_entry.set_dirtybit(pte.get_dirtybit())

                    #check if the bit has changed to dirty
                    if instruction == 's':
                        #Set dirty bit if its a store
                        new_entry.set_dirtybit(1)
                    
                    #same process as LRU so if OPT has a tie we can default back
                    #remove from currunt pos and add to end of the pagetable
                    pagetable.remove(pte)
                    pagetable.append(new_entry)

            #remove this entry from hashtable
            opthash[new_entry.get_address()].popleft()

    #this is where the fun happens
    #pagetable is at max capacity, we might need to evict
    else:
        #if the new entry is not in the page table already
        if new_entry.get_address() not in queue:
            #determine who will get evicted based on OPT algorithm
            evict = find_opt_evictable(pagetable, opthash)

            #see if the dirty bit is set and if so increment disk-writes
            if pagetable[evict].get_dirtybit() == 1:
                diskwrites += 1

            #evict the page
            queue.remove(pagetable[evict].get_address())
            pagetable.remove(pagetable[evict])

            #ready to add the new entry

            #check instruction
            if instruction == 's':
                new_entry.set_dirtybit(1)

            #add new entry and entry address to the page table and the address queue
            pagetable.append(new_entry)
            queue.append(new_entry.get_address())

            #remove this entry from the addresses hash table list like before
            if len(opthash[new_entry.get_address()]) > 0:
                opthash[new_entry.get_address()].popleft()

            #increment page fault 
            pagefaults += 1

        #else the page already exists
        else:
            #go through pagetable and find the match
            for pte in pagetable:
                if pte.get_address() == new_entry.get_address():
                    #match the dirty bits of the pte and new entry
                    new_entry.set_dirtybit(pte.get_dirtybit())

                    #update the dirty bit if its newly set
                    if instruction == 's':
                        new_entry.set_dirtybit(1)

                    #change the positioning for LRU tiebreaker
                    pagetable.remove(pte)
                    pagetable.append(new_entry)
                    
            #delete this access from the hashtable entry list
            if len(opthash[new_entry.get_address()]) > 0:
                opthash[new_entry.get_address()].popleft()
    #donezo

#find evictable page routine
def find_opt_evictable(pagetable, hashtable) -> int:

    evictable = 0       #the position of the page that will end up being evicted
    furthest_page = 0   #the value of next access line in the address hashtable
    index = 0           #which position we are at in the pagetable

    #loop through pagetable
    for pte in pagetable:
        #check the page table entries address value in the hashtable
        
        #if one of the addresses doesn't get accessed again
        if len(hashtable[pte.get_address()]) == 0:
            #this is a good option for eviction
            #if this is the case we will use LRU replacement 
            #and this entry falls earliest in the page table list so its the least recently used
            #go ahead and return to evict right away
            return index

        #otherwise we look through all pte's
        #we want the page who is accessed the furthest in the future
        if hashtable[pte.get_address()][0] > furthest_page:
            #set the newest furthest page value
            furthest_page = hashtable[pte.get_address()][0]
            evictable = index
        
        #after every pass increment which index we are at
        index += 1
        
    #after its found
    return evictable

def print_statistics():
    # printing metrics for the algorithm
    
    #format for autograder meh
    if algorithm == 'opt':
        print('Algorithm: OPT')
    if algorithm == 'lru':
        print('Algorithm: LRU')

    print('Number of frames: ' + str(numframes))
    print('Page size: ' + pagesizestr + ' KB')
    print('Total memory accesses: ' + str(memaccesses))
    print('Total page faults: ' + str(pagefaults))
    print('Total writes to disk: ' + str(diskwrites))


#---------------Entry Class bc Auto Grader doesn't like multiple files---------meh---
class Entry:
    def __init__(self, address, dirtybit):
        #address 
        self.__address = address
        #dirty bit
        self.__dirtybit = dirtybit

    #getters and setters
    def get_address(self):
        return self.__address

    def set_address(self, addr):
        self.__address = addr

    def get_dirtybit(self):
        return self.__dirtybit

    def set_dirtybit(self, db):
        self.__dirtybit = db

# main routine
if __name__ == "__main__":
    #setup globals
    setup()

    #decide which page replacement algorithm to run
    choose_page_replacement()

    #display results
    print_statistics()
