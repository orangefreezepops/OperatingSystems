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


struct shared_mem {
  int tickets;
  int visitorNum;
  int tourGuideNum;
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
  difference = curTime.tv_sec - startTime.tv_sec;
  time = (int) difference;
}

void visitorArrives(){
  int time = currentTime();
  fprintf(stderr, "Visitor %d arrives at time %d\n", museum->visitorNum, time);

  if (museum->tickets == 0){
    return;                             //leave if no tickets available
  }
  museum->tickets--;                    //decrement the number of tickets
  museum->visitorsOutside--;            //bring a visitor from outside into the museum
  up(vmutex_sem);                       //signal that visitor has arrived
}

void tourGuideArrives(){
  int time = currentTime();
  fprintf(stderr, "Tour guide %d arrives at time %d\n", museum->tourGuideNum, time);
  museum->guidesInMuseum++;             //increment number of guisdes in museum
  up(gmutex_sem);                       //signal that guide has arrived
}

void tourMuseum(){
  int time = currentTime();
  fprintf(stderr, "Visitor %d tours the museum at time %d\n", museum->visitorNum, time);
  museum->visitorsGivenTours++;         //increment number of visitors given tours
  museum->visitorsInMuseum++;           //increment the current number of visitors in the museum
  sleep(2);                             //tour for 2 seconds
  up(vwait_sem);                        //signal visitor finished the tour
}

void openMuseum(){
  int time = currentTime();
  fprintf(stderr, "Tour guide %d opens the museum for tours at time %d\n", museum->tourGuideNum, time);
  museumOpen = true;                    //set museum open flag to true
  up(gmutex_sem);                       //signal that guide opened the musuem
}

void tourGuideLeaves(){
  int time = currentTime();
  fprintf(stderr, "Tour guide %d leaves the museum at time %d\n", museum->tourGuideNum, time);
  museum->guidesInMuseum--;             //decrement the number of guides in the museum
  up(gwait_sem);                        //signal that the guide has left
}

void visitorLeaves(){
  int time = currentTime();
  fprintf(stderr, "Visitor %d leaves the museum at time %d\n", museum->visitorNum, time);
  museum->visitorsInMuseum--;           //decrement the number of visitors in the museum
  up(vwait_sem);                        //signal visitor finished the tour
}

void visitorArrivalProcess(){
  srand(visitorSeed);
  int i = 0;
  int value = 0;

  for (i = 0; i < visitorCnt; i ++){
    pid = fork();
    if (pid == 0){
      visitorArrives();
      if (museum->guidesOutside == 0){                //if there are no guides
        down(gmutex_sem);                             //wait for them to arrive
      }
      tourMuseum();
      visitorLeaves();
      exit(pid);
    } else {
      value = rand() % 100 + 1;
      if (value > visitorDelay) {
        sleep(visitorFollowingProbability);
      }
      wait(NULL);
      //fprintf(stderr, "wait for visitor child %d\n", i);
    }
    museum->visitorNum++;
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
  vmutex_sem = create(1, name0, key0);
  gmutex_sem = create(1, name1, key1);
  vwait_sem = create(0, name2, key2);
  gwait_sem = create(0, name3, key3);
  int i;

  museum = (struct shared_mem*)mmap(NULL, sizeof(struct shared_mem), PROT_READ|PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, 0, 0);

  museum->visitorsOutside = visitorCnt;
  museum->guidesOutside = tourGuideCnt;
  museum->tickets = 10*tourGuideCnt;
  museum->visitorsGivenTours = 0;
  museum->guidesInMuseum = 0;
  museum->visitorsInMuseum = 0;
  museum->visitorNum = 0;
  museum->tourGuideNum = 0;

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
        tourGuideArrives();                   //tour guide arrives at museum
        if (museum->visitorsOutside == 0){    //if there are no Visitors outside
          down(vmutex_sem);                   //wait for visitors to arrive
          openMuseum();                       //open the museum
        } else {                              //if there are visitors waiting at the museum and the museum is not open
          openMuseum();                       //open the museum
        }

        down(vwait_sem);                      //wait for the visitor to finish touring

        while (museum->visitorsGivenTours < visitorCnt){
          down(vwait_sem);
        }
        tourGuideLeaves();
        //down(gwait_sem);                      //wait fort the other guides to finish
        exit(pid);                            //exit process

      } else {
        int value = rand() % 100 + 1;
        if (value > tourGuideDelay){
          sleep(tourGuideFollowingProbability);
        }
        wait(NULL);                             //wait for each of the guides
        //fprintf(stderr, "wait for process %d\n", i);
      }
      museum->tourGuideNum++;
    }
    wait(NULL);
  }
  //fprintf(stderr, "End of Main\n");
  //close all the semaphores
  close(gmutex_sem);
  close(vmutex_sem);
  close(gwait_sem);
  close(vwait_sem);
  exit(0);
}
