#include <linux/smp_lock.h>
#include <stdbool.h>
#include <linux/sched.h>

struct queue_t{
    struct queue_t *next;           //pointer to next process in queue
    int size;                       //size of the process queue
    struct task_struct *cur_proc;   //the process
};

struct cs1550_sem
{
   int value;                       //semaphores value
   long sem_id;                     //semaphores ID
   spinlock_t lock;                 //spinlock to protect semaphores process queue
   char key[32];                    //semaphores key
   char name[32];
   struct cs1550_sem *next;         //reference to the next semaphore in the list
   struct queue_t *proc_q;          //semaphores process queue
};
