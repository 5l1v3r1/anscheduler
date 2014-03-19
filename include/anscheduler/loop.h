#ifndef __ANSCHEDULER_LOOP_H__
#define __ANSCHEDULER_LOOP_H__

/**
 * Initialize the scheduling loop. This will allocate various structures for
 * the task queue.
 * @critical
 */
void anscheduler_loop_initialize();

/**
 * Pushes the current thread back to the run loop for another time. This must
 * be called before running anscheduler_loop_run() function. However,
 * anscheduler_loop_switch() calls this automatically.
 * @critical
 */
void anscheduler_loop_push_cur();

/**
 * Removes a thread from the run loop if it is in the queue at all.
 * @param thread The thread which has been killed.
 */
void anscheduler_loop_delete(thread_t * thread);

/**
 * Enters the scheduling loop.  This function should never return.
 * @critical
 */
void anscheduler_loop_run();

/**
 * Add a kernel thread to the scheduler queue.
 * @critical
 */
void anscheduler_loop_push_kernel(void * arg, void (* fn)(void * arg));

/**
 * Switches from this thread to a different thread.  In order to call this
 * method, you must have already set isPolling back to 0 so that no other task
 * will attempt to switch into this one.
 * @param task A referenced task
 * @param thread The thread in the task
 * @critical
 */
void anscheduler_loop_switch(task_t * task, thread_t * thread);

#endif
