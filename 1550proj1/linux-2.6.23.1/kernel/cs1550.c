/*
really annoying to have to keep typing these
scp kdf11@thoth.cs.pitt.edu:/u/OSLab/kdf11/linux-2.6.23.1/arch/i386/boot/bzImage .
scp kdf11@thoth.cs.pitt.edu:/u/OSLab/kdf11/linux-2.6.23.1/System.map .
cp bzImage /root/bzImage-devel
cp bzImage /root/System.map-devel

copy test files into qemu
scp kdf11@thoth.cs.pitt.edu:/u/OSLab/kdf11/trafficsim .
scp kdf11@thoth.cs.pitt.edu:/u/OSLab/kdf11/trafficsim-mutex .
scp kdf11@thoth.cs.pitt.edu:/u/OSLab/kdf11/trafficsim-strict-order .

compiling test files
gcc -g -m32 -o trafficsim -I /u/OSLab/`whoami`/linux-2.6.23.1/include/ trafficsim.c
gcc -g -m32 -lm -o trafficsim-mutex -I /u/OSLab/`whoami`/linux-2.6.23.1/include/ trafficsim-mutex.c
gcc -g -m32 -o trafficsim-strict-order -I /u/OSLab/`whoami`/linux-2.6.23.1/include/ trafficsim-strict-order.c

cp /u/OSLab/kdf11/linux-2.6.23.1/vmlinux .
gdb -iex "set auto-load safe-path ."
add-symbol-file /u/OSLab/kdf11/trafficsim 0x08048430

*/

#include <linux/cs1550.h>
#include <linux/unistd.h>
#include <linux/linkage.h>
#include <linux/sched.h>

//system wide spinlock
DEFINE_SPINLOCK(lock);

//globals
long semaphore_id = 0; //gives a semaphore its ID, incriment every creation

struct cs1550_sem *head = NULL;//linked list of semaphores

bool q_insert_tail(struct cs1550_sem *sem){

	struct queue_t *newt = NULL; //new tail process
	struct queue_t *temp = NULL;

  //allocating space for the node
	newt = (struct queue_t*)kmalloc(sizeof(struct queue_t), GFP_ATOMIC);
	if (newt == NULL){
		return false;
	}
	newt->cur_proc = current;
	if (sem->proc_q == NULL){
		sem->proc_q = newt;
		newt->size ++;
	} else {
		temp = sem->proc_q;
		while(temp->next != NULL){
			temp = temp->next;
		}
		temp->next = newt;
		newt->size ++;
	}
	return true;
}

int q_size(struct queue_t *q)
{
	if (q == NULL){
		return 0;
	} else {
		return q->size;
	}
}


/* This syscall creates a new semaphore and stores the provided key to protect access
to the semaphore. The integer value is used to initialize the semaphore's value. The
function returns the identifier of the created semaphore, which can be used to down
and up the semaphore. */
asmlinkage long sys_cs1550_create(int value, char name[32], char key[32]){
  //create new semaphore
  struct cs1550_sem *newSem = NULL;
	long result;

  //allocate space for the semaphore and semaphore queue
  newSem = (struct cs1550_sem*)kmalloc(sizeof(struct cs1550_sem), GFP_ATOMIC);

  if(value < 0){ //don't allow a semaphore to be initialized with a negative value
		kfree(newSem);
    return -1;
  }

  //initialize spinlock
  spin_lock_init(&(newSem->lock));

  //initialize properties of the semaphore
  newSem->value = value; //value given
  semaphore_id++; //incriment global
  newSem->sem_id = semaphore_id; //global ID

  strncpy(newSem->name, name, 32); //name given
  strncpy(newSem->key, key, 32); //key given

	newSem->next = NULL;

  spin_lock(&lock);
  //add new sem to the end of semaphore list
	if (head == NULL){
		//if queue is empty
		head = newSem;
	} else {
		//case for existing queue
		newSem->next = head;
		head = newSem;
	}
	result = newSem->sem_id;
  spin_unlock(&lock);
  //return new ID
  return result;
}
/* This syscall opens an already created semaphore by providing the semaphore name
 and the correct key. The function returns the identifier of the opened semaphore if
 the key matches the stored key or -1 otherwise. */
asmlinkage long sys_cs1550_open(char name[32], char key[32]){
	struct cs1550_sem *temp = NULL;
  spin_lock(&lock);
  if (head!= NULL){
    temp = head;
    while (temp != NULL){
      if(strncmp(temp->name, name, 32) == 0 && strncmp(temp->key, key, 32) == 0){
        spin_unlock(&lock);
        return temp->sem_id;
      }
      temp = temp->next;
    }
  }
  spin_unlock(&lock);
  return -1;
}

struct cs1550_sem *get_semaphore(long sem_id){
	struct cs1550_sem *temp = NULL;
  if (head != NULL){
    temp = head;
    while (temp != NULL){
      if(temp->sem_id == sem_id){
        return temp;
      }
      temp = temp->next;
    }
		if(temp == NULL){
			spin_unlock(&lock);
		}
  }
  return NULL;
}

int has_semaphore(long sem_id){
	struct cs1550_sem *temp = NULL;
  if (head != NULL){
    temp = head;
    while (temp != NULL){
      if(temp->sem_id == sem_id){
        return 0;
      }
      temp = temp->next;
    }
  }
  return -1;
}

/* This syscall implements the down operation on an already opened semaphore using
the semaphore identifier from a previous call to sys_cs1550_create or sys_cs1550_open.
The function returns 0 when successful or -1 otherwise (e.g., if the semaphore id
is invalid or if the queue is full). Please check the lecture slides for the
pseudo-code of the down operation. */
asmlinkage long sys_cs1550_down(long sem_id){
  struct cs1550_sem *down_sem = NULL;
  spin_lock(&lock); //protect sem list
  //check if semaphore exists
  if (has_semaphore(sem_id) == 0){
		spin_unlock(&lock);//unlock sem list
    //assign down_sem to the found semaphore;
    down_sem = get_semaphore(sem_id);

    //start operations on the semaphore
    spin_lock(&(down_sem->lock));//protect semaphore's queue
    down_sem->value --; //decrement sem value
    if(down_sem->value < 0){
			//insert int
      if (q_insert_tail(down_sem) == true){
				//unlock the process queue to avoid deadlock before putting to sleep
	      spin_unlock(&(down_sem->lock));
	      set_current_state(TASK_INTERRUPTIBLE); //rest easy sweet prince
	      schedule(); //find the next sucker who wants to do some work
				return 0;
			}
    } else {
			spin_unlock(&(down_sem->lock));//unlock the queue
		}
  }
  //down operation failed
  spin_unlock(&lock); //unlock global
  return -1;
}
/* This syscall implements the up operation on an already opened semaphore using the semaphore
identifier obtained from a previous call to sys_cs1550_create or sys_cs1550_open. The function
returns 0 when successful or -1 otherwise (e.g., if the semaphore id is invalid).
Please check the lecture slides for pseudo-code of the up operation. */
asmlinkage long sys_cs1550_up(long sem_id){
  struct cs1550_sem *up_sem = NULL; //up semaphore
	struct queue_t *temp = NULL;
  spin_lock(&lock);//global lock for sem_list operations
  if(has_semaphore(sem_id) == 0){ //if exisits
    up_sem = get_semaphore(sem_id); //retrieve
		spin_unlock(&lock); //unlock global
    //start semaphore ops
    spin_lock(&(up_sem->lock));
    up_sem->value ++;
    if (up_sem->value <= 0){
			temp = up_sem->proc_q;
      //remove the process
			if (up_sem->proc_q == NULL){
				//spin_unlock(&(up_sem->lock));
				return -1;
			} else {
				up_sem->proc_q = up_sem->proc_q->next;
			}
			wake_up_process(temp->cur_proc); //wakey wakey eggs and bacy
			up_sem->proc_q->size --; //decrement the size of the queue
			kfree(temp);
      spin_unlock(&(up_sem->lock));
      return 0;
    }
  }
  spin_unlock(&lock);
  return -1;
}
/* This syscall removes an already created semaphore from the system-wide semaphore list using
the semaphore identifier obtained from a previous call to sys_cs1550_create or sys_cs1550_open.
The function returns 0 when successful or -1 otherwise (e.g., if the semaphore id is invalid
 or the semaphore's process queue is not empty). */
asmlinkage long sys_cs1550_close(long sem_id){
  struct cs1550_sem *close = NULL; //semaphore to close
	struct cs1550_sem *temp = NULL;
  int process_list_size = 0;
  spin_lock(&lock);
  if (has_semaphore(sem_id) == 0){
    //semaphore was found
    close = get_semaphore(sem_id); //assign to the semaphore that was found
    //spin_lock(&(close->lock)); //lock the process queue for the semaphore
    process_list_size = q_size(close->proc_q);
    /*if(process_list_size > 0){
      //if the process queue isnt empty
      //spin_unlock(&(close->lock)); //unlock semaphore lock
      spin_unlock(&lock); //unlock global list lock
      return -1; //return failure
    }*/
    //spin_unlock(&(close->lock)); //unlock semaphore lock
		if (close->next == NULL){
			//then close was at the end of the list
			kfree(close);
		} else {
			temp = close->next;
			close->next = temp->next; //bipass close and attach prev to next
			kfree(temp);
			return 0; //return success
		}
    //spin_unlock(&lock); //unlock global lock

  }

  //semaphore was not found
  spin_unlock(&lock);
  return -1; //return failure
}
