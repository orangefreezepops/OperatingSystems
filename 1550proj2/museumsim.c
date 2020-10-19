/*
  compile: gcc -g -m32 -o museumsim -I /u/OSLab/kdf11/linux-2.6.23.1/include/ museumsim.c
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

long mutex_sem = 0;
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

void visitorLeaves(){
  int time = currentTime();
  down(mutex_sem);
  fprintf(stderr, "Visitor %d leaves the museum at time %d\n", museum->visitorNum, time);
  museum->visitorsInMuseum--;
  up(mutex_sem);
}

void visitorArrives(){
  int time = currentTime();
  down(mutex_sem);
  fprintf(stderr, "Visitor %d arrives at time %d\n", museum->visitorNum, time);
  //leave if no tickets available
  if (museum->tickets == 0){
    visitorLeaves();
  }
  //wait if the museum isnt open ort the museum is capacity
  if (!museumOpen || museum->visitorsInMuseum == museum->guidesInMuseum*10){
    wait(NULL);
  }
  museum->tickets--;
  museum->visitorsInMuseum++;
  up(mutex_sem);
}

void tourMuseum(){
  int time = currentTime();
  down(mutex_sem);
  fprintf(stderr, "Visitor %d tours the museum at time %d\n", museum->visitorNum, time);
  museum->visitorsGivenTours++;
  sleep(2);
  up(mutex_sem);
}

void tourGuideArrives(){
  int time = currentTime();
  down(mutex_sem);
  fprintf(stderr, "Tour guide %d arrives at time %d\n", museum->tourGuideNum, time);
  if((!museumOpen && museum->visitorsOutside == 0) || (museumOpen && museum->guidesInMuseum == 2)){
    wait(NULL);
  }
  museum->tickets += 10;
  museum->guidesInMuseum++;
  up(mutex_sem);
}

void openMuseum(){
  int time = currentTime();
  down(mutex_sem);
  fprintf(stderr, "Tour guide %d opens the museum for tours at time %d\n", museum->tourGuideNum, time);
  museumOpen = true;
  up(mutex_sem);
}

void tourGuideLeaves(){
  int time = currentTime();

  down(mutex_sem);
  fprintf(stderr, "Tour guide %d leaves the museum at time %d\n", museum->tourGuideNum, time);
  museum->guidesInMuseum--;
  up(mutex_sem);
}

void visitorArrivalProcess(){
  srand(visitorSeed);
  int i = 0;
  int value = 0;

  for (i = 0; i < visitorCnt; i ++){
    pid = fork();
    if (pid == 0){
      visitorArrives();
      tourMuseum();
      visitorLeaves();
      exit(pid);
    } else {
      value = rand() % 100 + 1;
      if (value > 50) {
        sleep(visitorFollowingProbability);
      }
      wait(NULL);
    }
    for (i = 0; i < visitorCnt; i ++){
      wait(NULL);
    }
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

  char key[32] = "semaphore";
  mutex_sem = create(1, "sem", key);
  int i;

  museum = (struct shared_mem*)mmap(NULL, sizeof(struct shared_mem), PROT_READ|PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, 0, 0);

  down(mutex_sem);
  museum->visitorsOutside = visitorCnt;
  museum->guidesOutside = tourGuideCnt;
  museum->tickets = 0;
  museum->visitorsGivenTours = 0;
  museum->guidesInMuseum = 0;
  museum->visitorsInMuseum = 0;
  museum->visitorNum = 0;
  museum->tourGuideNum = 0;


  gettimeofday(&startTime, NULL);

  pid = fork();
  if(pid == 0){
    visitorArrivalProcess();
    wait(NULL);
  } else {
    srand(tourGuideSeed);
    for (i = 0; i < tourGuideCnt; i ++){
      int pid = fork();
      if (pid == 0){
        //guides
        tourGuideArrives();
        if (museum->visitorsOutside > 0 && !museumOpen){
          openMuseum();
        }
//---------------------------------------------------------------------------------
        //do we have to wait for all visitors to leave or do it just once?
        while(museum->visitorsInMuseum != 0){
          wait(NULL);
        }
        if (museum->visitorsGivenTours < 10 && museum->tickets > 0){
          wait(NULL);
        }
        //handle guides leaving together??
        tourGuideLeaves();
//---------------------------------------------------------------------------------

        //exit process
        exit(pid);
      }
      int value = rand() % 100 + 1;
      if (value > 50){
        sleep(tourGuideFollowingProbability);
      }
      wait(NULL);
    }
  }
  for(i = 0; i < tourGuideCnt; i ++){
    wait(NULL);
  }
  up(mutex_sem);
}
