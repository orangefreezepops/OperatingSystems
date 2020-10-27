/*
  compile: gcc -g -m32 -o museumsim -I /u/OSLab/kdf11/linux-2.6.23.1/include/ museumsim.c

  copy: scp kdf11@thoth.cs.pitt.edu:/u/OSLab/kdf11/museumsim .

  test inputs:  ./museumsim -m 1 -k 1 -pv 100 -dv 1 -sv 1 -pg 100 -dg 1 -sg 2
                ./museumsim -m 10 -k 1 -pv 100 -dv 1 -sv 1 -pg 100 -dg 1 -sg 2
                ./museumsim -m 20 -k 2 -pv 100 -dv 1 -sv 1 -pg 100 -dg 1 -sg 2
*/

#include <linux/unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/time.h>

#define MIN(x, y) (((x) < (y)) ? (x) : (y))

struct shared_mem {
  int tickets;
  int museumGuideEnter;
  int museumGuideLeave;
  int visitorsGivenTours;
  int visitorsInMuseum;
  int guidesInMuseum;
  int visitorsOutside;
  int guidesOutside;
};

struct timeval startTime;

bool museumOpen = false;
int capacity = 20;

int visitorCnt = 0;
int tourGuideCnt = 0;
int ticketsAvailable = 0;
int visitorFollowingProbability = 0;
int visitorDelay = 0;
int visitorSeed = 0;
int tourGuideFollowingProbability = 0;
int tourGuideDelay = 0;
int tourGuideSeed = 0;

long pid = 0;

long gmutex_sem = 0;
long vmutex_sem = 0;
long vwait_sem = 0;
long gwait_sem = 0;
long museum_mutex = 0;
long vdelay_mutex = 0;
long gdelay_mutex = 0;

struct shared_mem *museum = NULL;

long create(int value, char name[32], char key[32]) {
  return syscall(__NR_cs1550_create, value, name, key);
}

long open(char name[32], char key[32]) {
  return syscall(__NR_cs1550_open, name, key);
}

long down(long sem_id) {
  return syscall(__NR_cs1550_down, sem_id);
}

long up(long sem_id) {
  return syscall(__NR_cs1550_up, sem_id);
}

long close(long sem_id) {
  return syscall(__NR_cs1550_close, sem_id);
}

int currentTime(){
  struct timeval curTime;
  int time;
  gettimeofday(&curTime, NULL);
  time_t difference;
  difference = (((curTime.tv_sec) + curTime.tv_usec / 1000000) - ((startTime.tv_sec) + startTime.tv_usec / 1000000));
  //difference = (curTime.tv_sec - startTime.tv_sec);
  time = (int) difference;
  return time;
}

void visitorArrives(int vID){
  int time = currentTime();
  fprintf(stderr, "Visitor %d arrives at time %d\n", vID, time);
  up(museum_mutex);
  if (museum->tickets == 0){
    return;                             //leave if no tickets available
  }
  if (museum->visitorsInMuseum == 10 && vID < museum->tickets){
    down(vwait_sem);                    //if the museum is at capacity wait for visitors to finsih their tours
  }
  museum->visitorsOutside--;            //bring a visitor from outside into the museum
  down(museum_mutex);
  up(vmutex_sem);                       //signal that visitor has arrived
}

void tourGuideArrives(int tg){
  int time = currentTime();
  fprintf(stderr, "Tour guide %d arrives at time %d\n", tg, time);
  up(museum_mutex);
  museum->guidesOutside++;              //increment the guides outside
  down(museum_mutex);
  up(gmutex_sem);                       //signal that guide has arrived
}

void tourMuseum(int vID){
  int time = currentTime();
  fprintf(stderr, "Visitor %d tours the museum at time %d\n", vID, time);
  up(museum_mutex);                     //protect shared variables
  museum->visitorsGivenTours++;         //increment number of visitors given tours
  museum->visitorsInMuseum++;           //increment the current number of visitors in the museum
  down(museum_mutex);                   //stop protecting shared variables
  sleep(2);                             //tour for 2 seconds
  up(vwait_sem);                       //signal visitor finished the tour
}

void openMuseum(int tg){
  int time = currentTime();
  fprintf(stderr, "Tour guide %d opens the museum for tours at time %d\n", tg, time);
  museumOpen = true;                    //set museum open flag to true
  /*if (tg != 0){
    //if this isn't the first tour guide, meaning visitors have already been served
    //allow the next batch of visitors to enter the museum and tour
    up(gdelay_mutex); //signal that the next guide to open the museum arrived
  }*/
  up(gmutex_sem);                       //signal that guide opened the musuem
}

void tourGuideLeaves(int tg){
  int time = currentTime();
  fprintf(stderr, "Tour guide %d leaves the museum at time %d\n", tg, time);
  up(museum_mutex);
  museum->guidesInMuseum--;             //decrement the number of guides in the museum
  down(museum_mutex);

  up(museum_mutex);
  if(museum->guidesInMuseum == 0){      //if the guides all left
    down(museum_mutex);
    museumOpen = false;                 //close the museum so the next guide can open it
  }
  up(gwait_sem);                        //signal tour guide left
}

void visitorLeaves(int vID){
  int time = currentTime();
  fprintf(stderr, "Visitor %d leaves the museum at time %d\n", vID, time);
  up(museum_mutex);
  museum->visitorsInMuseum--;           //decrement the number of visitors in the museum
  down(museum_mutex);

  up(museum_mutex);
  //if the # of visitors reaches tickets - 10 (10 served), or there are no visitors inside
  if (museum->visitorsInMuseum == museum->tickets - 10 || museum->visitorsInMuseum == 0) {
    down(museum_mutex);
    up(gwait_sem);                      //signal that the last visitor for this guide left
  }
  up(vwait_sem);                        //signal visitor finished the tour
}

void visitorArrivalProcess(){
  srand(visitorSeed);
  int i = 0;
  int value = 0;

  for (i = 0; i < visitorCnt; i ++){
    pid = fork();
    if (pid == 0){
      int visitorID = i;                              //visitor ID
      visitorArrives(visitorID);

      up(museum_mutex);                               //protect shared var
      if (museum->guidesOutside == 0){                //if the visitor arrives first
        down(museum_mutex);                           //unprotect shared var
        down(gmutex_sem);                             //wait for the guide to arrive
      }

      up(museum_mutex);
      //if this visitor can't fit in the museum
      /*if (i > museum->guidesInMuseum*10 && i < museum->tickets){
        down(gdelay_mutex);         //wait for the current visitors to finish
                                    //and the delayed guides to arrive and open the museum
      }*/
      down(museum_mutex);

      up(museum_mutex);
      if (i < museum->tickets){                       //if this visitor was able to receive a ticket
        tourMuseum(visitorID);                        //tour
      }
      down(museum_mutex);

      visitorLeaves(visitorID);                       //leave the museum
      exit(0);
    } else {
      value = rand() % 100 + 1;
      if (value > visitorFollowingProbability) {
        //fprintf(stderr, "wait for visitor child %d\n", i);
        sleep(visitorDelay);
      }
    }
  }
  for (i = 0; i < visitorCnt; i ++){
    wait(NULL);
    //fprintf(stderr, "wait for visitor child %d\n", i);
  }
}

int main(int argc, char *argv[]){
  //-m
  visitorCnt = atoi(argv[2]);

  //-k
  tourGuideCnt = atoi(argv[4]);

  //-pv
  visitorFollowingProbability = atoi(argv[6]);

  //-dv
  visitorDelay = atoi(argv[8]);

  //-sv
  visitorSeed = atoi(argv[10]);

  //-pg
  tourGuideFollowingProbability = atoi(argv[12]);

  //-dg
  tourGuideDelay = atoi(argv[14]);

  //-sg
  tourGuideSeed = atoi(argv[16]);

  char key0[32] = "visitorSemaphore";
  char name0[32] = "vsem";
  char key1[32] = "guideSemaphore";
  char name1[32] = "gsem";
  char key2[32] = "visitorWait";
  char name2[32] = "vwait";
  char key3[32] = "guideWait";
  char name3[32] = "gwait";
  char keym[32] = "museumMutex";
  char namem[32] = "mmutex";
  char keyvd[32] = "vDelayMutex";
  char namevd[32] = "vdelay";
  char keygd[32] = "gDelayMutex";
  char namegd[32] = "gdelay";

  vmutex_sem = create(1, name0, key0);
  gmutex_sem = create(1, name1, key1);
  vwait_sem = create(0, name2, key2);
  gwait_sem = create(0, name3, key3);
  museum_mutex = create(0, namem, keym);
  vdelay_mutex = create(0, namevd, keyvd);
  gdelay_mutex = create(0, namegd, keygd);

  //if any of the semaphore creations return -1 abort bc theyre still in kernel memory
  if (vmutex_sem == -1 || gmutex_sem == -1 || vwait_sem == -1 || gwait_sem == -1 || museum_mutex == -1 || vdelay_mutex == -1 || gdelay_mutex == -1){
    fprintf(stderr, "One of your semaphores returned -1 bc its already in kernel memory, try rebooting QEMU\n");
    exit(0);
  }
  int i,j,k;

  museum = (struct shared_mem*)mmap(NULL, sizeof(struct shared_mem), PROT_READ|PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, 0, 0);

  museum->visitorsOutside = visitorCnt;
  museum->guidesOutside = tourGuideCnt;
  museum->tickets = 10*tourGuideCnt;
  museum->visitorsGivenTours = 0;
  museum->guidesInMuseum = 0;
  museum->visitorsInMuseum = 0;

  gettimeofday(&startTime, NULL);

  pid = fork();
  if(pid == 0){
//-----------------------VISITOR PROCESS----------------------------------------
    visitorArrivalProcess();
    exit(0);
  } else {
//-----------------------GUIDE PROCESS------------------------------------------
    srand(tourGuideSeed);
    for (i = 0; i < tourGuideCnt; i ++){
      int pid = fork();
      if (pid == 0){
        int tgID = i;                             //tour guide ID

        tourGuideArrives(tgID);                   //tour guide arrives at museum

        up(museum_mutex);
        if (museum->visitorsOutside == 0 && !museumOpen){        //if there are no Visitors outside
          down(vmutex_sem);                       //wait for visitors to arrive
          openMuseum(tgID);                       //open the museum
          museum->guidesInMuseum++;               //increment number of guides in museum
        } else if (!museumOpen){                  //if there are visitors waiting at the museum and the museum is not open
          openMuseum(tgID);                       //open the museum
          museum->guidesInMuseum++;
          //if the museum is already open and there are less than 2 guides
        } else if (museumOpen && museum->guidesInMuseum < 2){
          museum->guidesInMuseum++;               //increment number of guides in museum
        } else if (museumOpen && museum->guidesInMuseum == 2) {
          //wait til a guides leave
          down(gwait_sem);
          openMuseum(tgID);                       //open the museum
          museum->guidesInMuseum++;               //increment number of guides in museum
        }
        down(museum_mutex);

        up(museum_mutex);
        if (museum->visitorsInMuseum >= 10){       //if there are more than 10 visitors
          down(museum_mutex);
          for(j = 0; j < 10; j++){
            down(vwait_sem);                      //wait for the visitors this tour guide can handle to finish
          }
        } else {                                  //else if there are less than the max visitors for this guide
          for(k = 0; k < museum->visitorsInMuseum; k++){
            down(museum_mutex);
            down(vwait_sem);                      //wait for all of the visitors to finish
          }
        }

        up(museum_mutex);
        if (museum->visitorsGivenTours != MIN(visitorCnt, 10)){
          down(museum_mutex);
          down(gwait_sem);                        //wait for the signal that the last visitor finished
        }

        tourGuideLeaves(tgID);                    //leave togeter
        exit(0);                                  //exit process
      } else {
        int value = rand() % 100 + 1;
        if (value > tourGuideFollowingProbability){
          sleep(tourGuideDelay);
        }
      }
    }
    for (i=0; i < tourGuideCnt; i ++){
      wait(NULL);                             //wait for each of the guides
    }
  }
  wait(NULL);
  //close all the semaphores
  close(gmutex_sem);
  close(vmutex_sem);
  close(gwait_sem);
  close(vwait_sem);
  close(museum_mutex);
  close(vdelay_mutex);
  close(gdelay_mutex);
  exit(0);
}
