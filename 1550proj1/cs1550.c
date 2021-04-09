#include <linux/syscalls.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/spinlock.h>
#include <linux/stddef.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/cs1550.h>

//Global variables made for proj1

//intialize the semaphore list
static LIST_HEAD(sem_list);

//readers writes lock to protect the semaphore list
static DEFINE_RWLOCK(sem_rwlock);

//create semaphore ID numbering system
long semaphore_id = 0; //0 for the first semaphore -- will be incrimented every creation

/**
 * Creates a new semaphore. The long integer value is used to
 * initialize the semaphore's value.
 *
 * The initial `value` must be greater than or equal to zero.
 *
 * On success, returns the identifier of the created
 * semaphore, which can be used with up() and down().
 *
 * On failure, returns -EINVAL or -ENOMEM, depending on the
 * failure condition.
 */
SYSCALL_DEFINE1(cs1550_create, long, value)
{
	//intialize the semaphore
	struct cs1550_sem *sem = NULL; //new semaphore
	sem = (struct cs1550_sem*)kmalloc(sizeof(struct cs1550_sem), GFP_ATOMIC); //memory allocation
	
	//if the memory didnt allocate correctly
	if (sem == NULL){
		printk(KERN_WARNING "CREATE: no memory - semaphore not created");
		return -ENOMEM;
	}
	//if the semaphore is trying to be initialized with a value less than 0
	if (value < 0) {
		kfree(sem);		//free the memory
		printk(KERN_WARNING "CREATE: bad value - value < 0");
		return -EINVAL;	//return the error
	}

	//otherwise, everything is kosher so go ahead and initialize 
	//the semaphore and its attributes (id, value, lock, list, task list)
	sem->sem_id = semaphore_id;				//id
	sem->value = value;						//value
	spin_lock_init(&sem->lock);				//spinlock
	INIT_LIST_HEAD(&sem->list); 			//initialize previous and next pointers of list
	INIT_LIST_HEAD(&sem->waiting_tasks); 	//initialize head and tail of task list

	//protect the global FIFO queue with rwlock before adding the new semaphore
	write_lock(&sem_rwlock);
	list_add(&sem->list, &sem_list);	//add this semaphore's list to the global list
	semaphore_id++;						//increment the global semaphore ID for the next created one
	write_unlock(&sem_rwlock);
	

	return sem->sem_id;		//return the semaphores identifier
}

//adding entry to the queue 
int enqueue(struct cs1550_sem *down_sem){
	struct cs1550_task *new_task = NULL;
	new_task = (struct cs1550_task*)kmalloc(sizeof(struct cs1550_task), GFP_ATOMIC);
	
	//make sure the task memory could be allocated
	if (new_task == NULL){
		//if not return error
		return -ENOMEM;
	}
	new_task->task = current; //set the task structs task field to the the global current 
	//initialize the task next and prev pointers
	INIT_LIST_HEAD(&new_task->list); //initialize the task nodes list 
	list_add_tail(&new_task->list, &down_sem->waiting_tasks); //add the task to the list
	return 0; //success
}

/**
 * Performs the down() operation on an existing semaphore
 * using the semaphore identifier obtained from a previous call
 * to cs1550_create().
 *
 * This decrements the value of the semaphore, and *may cause* the
 * calling process to sleep (if the semaphore's value goes below 0)
 * until up() is called on the semaphore by another process.
 *
 * Returns 0 when successful, or -EINVAL or -ENOMEM if an error
 * occurred.
 */
SYSCALL_DEFINE1(cs1550_down, long, sem_id)
{
	struct cs1550_sem *sem = NULL; //the semaphore object being downed

	//protect the global semaphore list
	read_lock(&sem_rwlock); 			
	//iterate over the global sem list
	list_for_each_entry(sem, &sem_list, list){
		//if the sem_id passed in matches the semaphore's id at this index
		//this is the semaphore to down
		if (sem->sem_id == sem_id){
			//lock spinlock to operate on the semaphore
			spin_lock(&sem->lock);
			sem->value--;
			//check the value -- if no resource is available 
			if (sem->value < 0){
				//allocate and insert a task into the queue of waiting tasks 
				if (enqueue(sem) == 0){
					//rest easy sweet prince -- prepare to sleep
					set_current_state(TASK_INTERRUPTIBLE);

					//unlock spinlock and rwlock to avoid deadlock before sleeping
					spin_unlock(&sem->lock);
					read_unlock(&sem_rwlock);

					schedule(); //call scheduler, go to sleep
					return 0; 	//return success
				}
			} else {
				//in case that if condition isnt met you still need to unlock
				spin_unlock(&sem->lock);
				read_unlock(&sem_rwlock);
				return 0;
			}
		}
	}
	read_unlock(&sem_rwlock); //unlock global lock if the search didn't find the semaphore
	return -EINVAL; //down failed
}

//removing entry from queue
struct cs1550_task* dequeue(struct cs1550_sem *up_sem){
	struct cs1550_task *sleeping_task = NULL; //cs1550_task structure pointer
	//get the first item in the waiting task list
	sleeping_task = list_first_entry(&up_sem->waiting_tasks, struct cs1550_task, list);
	return sleeping_task; //return the sleeping task so it can be woken up
}

/**
 * Performs the up() operation on an existing semaphore
 * using the semaphore identifier obtained from a previous call
 * to cs1550_create().
 *
 * This increments the value of the semaphore, and *may cause* the
 * calling process to wake up a process waiting on the semaphore,
 * if such a process exists in the queue.
 *
 * Returns 0 when successful, or -EINVAL if the semaphore ID is
 * invalid.
 */
SYSCALL_DEFINE1(cs1550_up, long, sem_id)
{
	struct cs1550_sem *sem = NULL; 				//semaphore to be upped 
	struct cs1550_task *sleeping_task = NULL; 	//currenlty sleeping task to be upped

	read_lock(&sem_rwlock); //lock the read lock to protect the gloabl list
	//iterate over the global sem list
	list_for_each_entry(sem, &sem_list, list){
		//if the sem_id passed in matches the semaphore's id at this index
		//this is the semaphore to down
		if (sem->sem_id == sem_id){
			//lock spinlock to operate on the semaphore
			spin_lock(&sem->lock);
			sem->value++; //increment the semaphores value indicating resource available
			//check the value -- if its still less than or equal to zero 
			//we'll need to wake up a task to start freeing the resource
			if (sem->value <= 0){
				//remove the head of the waiting tasks if list not empty
				if (!list_empty(&sem->waiting_tasks)){
					sleeping_task = dequeue(sem); //dequeue routine to retrieve the task
					if (sleeping_task != NULL){
						//wake up sleepy head
						wake_up_process(sleeping_task->task);
						list_del(&sleeping_task->list); //delete the first entry from the list
						kfree(sleeping_task); //free the woken up processes memory
						
						//unlock both locks to avoid deadlock
						spin_unlock(&sem->lock);
						read_unlock(&sem_rwlock);
						//return success
						return 0;
					}
				}
			} else {
				//the value  of the semaphore didnt call for a process to be woken up
				spin_unlock(&sem->lock);	//unlock the spinlock
				read_unlock(&sem_rwlock);	//unlock rwlock
				return 0; //its ok to stil return success because the sem exists
			}
		}
	}
	read_unlock(&sem_rwlock); //unlock global list lock if sem was never found
	return -EINVAL; //failure
}

/**
 * Removes an already-created semaphore from the system-wide
 * semaphore list using the identifier obtained from a previous
 * call to cs1550_create().
 *
 * Returns 0 when successful or -EINVAL if the semaphore ID is
 * invalid or the semaphore's process queue is not empty.
 */
SYSCALL_DEFINE1(cs1550_close, long, sem_id)
{
	struct cs1550_sem *sem = NULL;

	//lock the global list for writing
	write_lock(&sem_rwlock);
	list_for_each_entry(sem, &sem_list, list){
		//if the sem_id passed in matches the semaphore's id at this index
		//this is the semaphore to close
		if (sem->sem_id == sem_id){
			//lock spinlock to operate on the semaphore
			spin_lock(&sem->lock);
			//check to make sure the waiting tasks queue is empty
			if (list_empty(&sem->waiting_tasks)){
				list_del(&sem->list); //remove the semaphore from the gloabl list
				spin_unlock(&sem->lock); //unlock semaphore spinlock
				kfree(sem); //free the semaphore memory
				write_unlock(&sem_rwlock); //unlock global list
				return 0; //return success
			}
			//incase that if condition isnt met you still need to unlock
			spin_unlock(&sem->lock);
		}
	}
	write_unlock(&sem_rwlock); //if semaphore wasn't found you need to unlock global list
	return -EINVAL; //failure
}
