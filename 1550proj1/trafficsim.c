/* A simple test program for Project 1 up() and down() syscalls.
 * The output of this program should always start with:
 *     This line has to print first
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
  //Create a semaphore and initialize it to 0
  char key[32] = "CS1550 is fun!";
  long sem_id = create(0, "sem", key);

  //Create two child processes
  int pid = fork(); // Create the first child process
  if (pid==0)
  { // I am the first child
    sleep(1); // Give a chance for the second child to perform down() first (hopefully!)
    down(sem_id);
    fprintf(stderr, "This line has to print last\n");
    up(sem_id);
  } else { // I am the parent
    pid = fork(); // Create a second child process
    if(pid == 0)
    { // I am the second child
      down(sem_id);
      fprintf(stderr, "This line has to print second\n");
    } else { // I am the parent
      sleep(3); // Give a chance for the children to run (hopefully!)
      /*
       * Now, both child processes are hopefully waiting on the semaphore.
       * The 2nd child should be the first entry (head) of queue
       * The 1st should be the last entry (tail) of the queue
       * Parent process keeps going...
       */
      fprintf(stderr, "This line has to print first\n");

      up(sem_id); // Current head of queue (2nd child) is waken up
      sleep(1); // Ensure that the 2nd child can finish its printf before the 1st child is waken up
      up(sem_id); // Current head of queue (1st child) is waken up

      // Release child 1's and child 2's resources to prevent them from being 'orphan' or 'zombie'
      wait(NULL);
      wait(NULL);
      close(sem_id);
    }
  }

  return 0;
}
