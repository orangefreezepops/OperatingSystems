/* A simple test program for Project 1 up() and down() syscalls.
 * The output of this program should always be:
 *     This line has to print first
 *     This line has to print second
 *     This line has to print last
 * no matter how long the processes sleep.
 */
#include <sys/mman.h>
#include <linux/unistd.h>
#include <stdio.h>
#include <sys/resource.h>

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
  //Create two semaphores and initialize them to 0
  char key[32] = "CS1550 is fun!";
  long sem_id = create(0, "sem", key);
  long nextsem_id = create(0, "next_sem", key);

  //Create two child processes
  int pid = fork(); // Create the first child process
  if (pid==0)
  { // I am the first child
    down(sem_id);
    fprintf(stderr, "This line has to print second\n");
    up(nextsem_id); // Current head of queue (1st child) is waken up
  } else { // I am the parent
    pid = fork(); // Create a second child process
    if(pid == 0)
    { // I am the second child
      down(nextsem_id);
      fprintf(stderr, "This line has to print last\n");
    } else
    { // I am the parent
      /*
       * Now, both child processes are waiting
       * Parent process keeps going...
       */
      fprintf(stderr, "This line has to print first\n");

      up(sem_id); //1st child is waken up

      // Release child 1's and child 2's resources to prevent them from being 'orphan'
      wait(NULL);
      wait(NULL);
      close(sem_id);
      close(nextsem_id);
    }
  }

  return 0;
}
