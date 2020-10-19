/* A simple test program for Project 1 up() and down() syscalls.
 * The output of this program should always be that entering and
 *  exiting the Critical Section is never preempted.
 */
#include <sys/mman.h>
#include <linux/unistd.h>
#include <sys/resource.h>
#include <stdio.h>
#include <math.h> // Need to add -lm flag: "gcc -m32 -lm -o trafficsim-mutex ..."

#define LOOPS1 1 // workload for each process
#define LOOPS2 1 // number of iterations

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

int main()
{
  //Create a semaphore and initialize it to 1
  char key[32] = "CS1550 is fun!";
  long sem_id = create(1, "mutex", key);

  int i, j;
  float m=1.0;

  //Create two child processes
  int pid = fork(); // Create the first child process
  if (pid==0)  { // I am the first child
    for (i=0; i < LOOPS2; i++) {
      down(sem_id);
      fprintf(stderr, "Child 1 process entering critical section, iteration %i.\n", i);
      for (j=0; j < LOOPS1; j++) {
        m = m*log(j*i*149384);
        m = exp(m);
      }
      fprintf(stderr, "Child 1 process exiting CS, iteration %i. j=%i\n", i, j);
      up(sem_id);
    }
  } else { // I am the parent
    pid = fork(); // Create a second child process
    if(pid == 0) { // I am the second child
      for (i=0; i < LOOPS2; i++) {
        down(sem_id);
        fprintf(stderr, "Child 2 process entering critical section, iteration %i.\n", i);
        for (j=0; j < LOOPS1; j++) {
          m = m*log(j*i*149384);
          m = exp(m);
        }
        fprintf(stderr, "Child 2 process exiting CS, iteration %i. j=%i\n", i, j);
        up(sem_id);
      }
    } else { // I am the parent
      for (i=0; i < LOOPS2; i++) {
        down(sem_id);
        fprintf(stderr, "parent process entering critical section, iteration %i.\n", i);
        for (j=0; j < LOOPS1; j++) {
          m = m*log(j*i*149384);
          m = exp(m);
        }
        fprintf(stderr, "parent process exiting CS, iteration %i. j=%i\n", i, j);
        up(sem_id);
      }
      // Release child 1's and child 2's resources to prevent them from being 'orphan'
      wait(NULL);
      wait(NULL);
      close(sem_id);
    }
  }
  return 0;
}
