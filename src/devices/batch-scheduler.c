/* Tests cetegorical mutual exclusion with different numbers of threads.
 * Automatic checks only catch severe problems like crashes.
 */
#include <stdio.h>
#include "tests/threads/tests.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "lib/random.h" //generate random numbers

#define BUS_CAPACITY 3
#define SENDER 0
#define RECEIVER 1
#define NORMAL 0
#define HIGH 1
#define OCCUPIED_TIME 10

/* toggle for debug message, somehow the test won't passed the make check if this is enabled */
#define DEBUG_VERBOSE 0

/* semaphore initiation */
struct semaphore    slotMutex;
struct semaphore    priSendFlag;
struct semaphore    priRecvFlag;

/* use lock for mutex, because we want just only one thread which
   has an access to the critical section, whether to acquire/lock it or
   to release/unlock it */
struct lock         mutex;

/* busOperation is a global variable for the current bus direction */
unsigned int        busOperation;

/* number of finished thread */
unsigned int        finishedThread;

/* number of total Task*/
unsigned int        totalTask;

/*
 *	initialize task with direction and priority
 *	call o
 * */
typedef struct {
	int direction;
	int priority;
} task_t;

/*
 *  the batch scheduler test routine declaration
* The metaphor for this solution is lika a bridge of passing cars. There is receiving and sending cars
* and depending on which the direction is, the system allows the cars followd by the direction to pass by.
* But if the other direction has higher priority, the direction will change and the cars with the higher priority will pass. 
 * */
void batchScheduler(    unsigned int num_tasks_send, 
                        unsigned int num_task_receive,
                        unsigned int num_priority_send, 
                        unsigned int num_priority_receive);

void senderTask(void *);
void receiverTask(void *);
void senderPriorityTask(void *);
void receiverPriorityTask(void *);

/* task for watching other threads */
void watcher(void *);

/*Task requires to use the bus and executes methods below*/
void oneTask(task_t task);
/* task tries to use slot on the bus */
void getSlot(task_t task);
/* task processes data on the bus either sending or receiving based on the direction*/
void transferData(task_t task);
/* task release the slot */
void leaveSlot(task_t task);

/* initializes semaphores */ 
void init_bus(void){ 
 
    random_init((unsigned int)123456789); 
    
    if (DEBUG_VERBOSE)
    { 
        msg("INIT BUS...");
    }
 
    /* FIXME implement */

    /* init the semaphores */
    sema_init (&slotMutex, BUS_CAPACITY);
    sema_init (&priSendFlag, 0);
    sema_init (&priRecvFlag, 0);

    /* init the lock */
    lock_init (&mutex);
}

/*
 *  Creates a memory bus sub-system  with num_tasks_send + num_priority_send
 *  sending data to the accelerator and num_task_receive + num_priority_receive tasks
 *  reading data/results from the accelerator.
 *
 *  Every task is represented by its own thread. 
 *  Task requires and gets slot on bus system (1)
 *  process data and the bus (2)
 *  Leave the bus (3).
 */

void batchScheduler(    unsigned int num_tasks_send, 
                        unsigned int num_task_receive,
                        unsigned int num_priority_send, 
                        unsigned int num_priority_receive)
{
    unsigned int i;
    char taskNameWatcher[16];

    if (DEBUG_VERBOSE)  
    {
        msg("BATCH SCHEDULER TASK");
        msg("start with number of slot is %d", slotMutex.value);
    }

    /* FIXME implement */

    finishedThread = 0;

    /* create sending tasks */
    if (num_tasks_send > 0)
    {
        for (i=0; i<num_tasks_send; i++)
        {
            char taskName[16];
            snprintf (taskName, sizeof taskName, "Tx %d", i);
            thread_create (taskName, PRI_DEFAULT, senderTask, NULL);
        }
    }

    /* create receiving tasks */
    if (num_task_receive > 0)
    {
        for (i=0; i<num_task_receive; i++)
        {
            char taskName[16];
            snprintf (taskName, sizeof taskName, "Rx %d", i);
            thread_create (taskName, PRI_DEFAULT, receiverTask, NULL);
        }
    }

    /* create priority sending tasks */
    if (num_priority_send > 0)
    {
        for (i=0; i<num_priority_send; i++)
        {
            char taskName[16];
            snprintf (taskName, sizeof taskName, "prTx %d", i);
            thread_create (taskName, PRI_DEFAULT, senderPriorityTask, NULL);
        }
    }   

    /* create priority receiving tasks */
    if (num_priority_receive > 0)
    {
        for (i=0; i<num_priority_receive; i++)
        {
            char taskName[16];
            snprintf (taskName, sizeof taskName, "prRx %d", i);
            thread_create (taskName, PRI_DEFAULT, receiverPriorityTask, NULL);
        }
    }

    /* total task which equal to total threads */
    totalTask = num_tasks_send + num_priority_send + num_task_receive + num_priority_receive;

    /* span new one dedicated thread just to watch unfinished threads */
    snprintf (taskNameWatcher, sizeof taskNameWatcher, "watcher", NULL);
    thread_create (taskNameWatcher, PRI_DEFAULT, watcher, NULL);
}

void watcher(void *aux UNUSED)
{
    /* waiting routine. let the host waits for every threads to finished */
    if (DEBUG_VERBOSE)
        msg ("total task is %d", totalTask);
  
    while (finishedThread != totalTask)
    {
        if (DEBUG_VERBOSE)
            msg ("waiting %d-threads to finished", (totalTask-finishedThread));
        
        timer_sleep(20);
    }
     
    if (DEBUG_VERBOSE)     
        msg ("all %d-threads are finished", finishedThread);  

    thread_exit();
}

/* Normal task,  sending data to the accelerator */
void senderTask(void *aux UNUSED){
        task_t task = {SENDER, NORMAL};
        oneTask(task);
}

/* High priority task, sending data to the accelerator */
void senderPriorityTask(void *aux UNUSED){
        task_t task = {SENDER, HIGH};
        oneTask(task);
}

/* Normal task, reading data from the accelerator */
void receiverTask(void *aux UNUSED){
        task_t task = {RECEIVER, NORMAL};
        oneTask(task);
}

/* High priority task, reading data from the accelerator */
void receiverPriorityTask(void *aux UNUSED){
        task_t task = {RECEIVER, HIGH};
        oneTask(task);
}

/* abstract task execution*/
void oneTask(task_t task) {
    getSlot(task);
    transferData(task);
    leaveSlot(task);
}

/* task tries to get slot on the bus subsystem */
void getSlot(task_t task) 
{
    struct semaphore *waitingList;

    if (DEBUG_VERBOSE) 
    {
        msg("thread %s is getting slot...", thread_current()->name);
    }

    /* create a queue list for high priority */
    if (task.priority == HIGH)
    {
        if (task.direction == SENDER)
        {
            /* if there's a high priority task doing sending, increase the semaphore for priSendFlag */
            sema_up (&priSendFlag);
    
            if (DEBUG_VERBOSE)              
            {
                msg("now priority flag is %d", priSendFlag.value);
            }
            
        }
        else
        {
            /* if there's a high priority task doing receiving, increase the semaphore priRecvFlag */
            sema_up (&priRecvFlag);
            
            if (DEBUG_VERBOSE)  
            {          
                msg("now priority flag is %d", priRecvFlag.value);  
            }
        }
    }
    
    /* check the waiting list in the other side of bus
       basically, inform a task about high priority tasks which now are waiting in other side */
    if (task.direction == SENDER)
        waitingList = &priRecvFlag;
    else
        waitingList = &priSendFlag;
    
    /* let the threads execute the getSlot routine. in this case, we're not using queue, instead we're using
       while loop to simulate waiting state */
    while (true)
    {
        /* entering critical section */
        lock_acquire(&mutex);

        if (DEBUG_VERBOSE)
        {
            msg("inside critical section", thread_current()->name);
        }

        /* these are cases a task can utilize a slot:
            1. the bus are empty, which means slotMutex.value is equal to initial semaphore value (BUS_CAPACITY) 
            2. previous slot changed the busOperation, and current task can join the slot if the busOperation is same with its.
           
           However, those two cases are dependent on their priority and waitingList on the other side as well
           
            3. high priority task don't have to check if there's a other high priority task in waitingList at the other side, as they
               have same level of priority
            4. but, normal priority tasks can obtain the slot only if there's no high priority task waiting in other side */
        if ((slotMutex.value == BUS_CAPACITY || busOperation == task.direction) && (task.priority == HIGH || waitingList->value == 0))
        {
            if (DEBUG_VERBOSE)
            {            
                msg("inside critical section", thread_current()->name);  
            }

            /* update the busOperation by the task who got inside the critical section */
            busOperation = task.direction;

            /* officially utilize the slot by decreasing the slot semaphore */
            sema_down(&slotMutex);

            /* update the high priority waiting list if a high priority task already got a slot */
            if (task.priority == HIGH)
            {
                if (task.direction == SENDER)
                    sema_down(&priSendFlag);
                else
                    sema_down(&priRecvFlag);
            }

            /* leaving critical section */
            lock_release(&mutex);

            break;
        }
        
        /* leaving critical section */
        lock_release(&mutex);

        /* let the scheduler gives a chance to other threads */
        thread_yield();
    }
}

/* task processes data on the bus send/receive */
void transferData(task_t task) 
{
    int64_t random;

    if (DEBUG_VERBOSE)   
    {
        msg("thread %s got one slot...");
        msg("thread %s is transferring data...", thread_current()->name);
    }

    random = (int64_t)random_ulong() % OCCUPIED_TIME + 1;
    timer_sleep(random);

    if (DEBUG_VERBOSE) 
    {
        msg("thread %s is done...");    
    }
}

/* task releases the slot */
void leaveSlot(task_t task) 
{
    if (DEBUG_VERBOSE)    
    {   
        msg("thread %s is updating the slot...", thread_current()->name);  
        msg("thread %s is leaving slot...", thread_current()->name);
    }
    
    /* free one slot of the bus */
    sema_up(&slotMutex);
    
    /* update the number of finished thread */
    finishedThread++;

    if (DEBUG_VERBOSE) 
    {
        msg("semaphore val %d", slotMutex.value);
    }

    /* just for precautions, force kill the thread */    
    thread_exit();
}
