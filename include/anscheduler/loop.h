#ifndef __ANSCHEDULER_LOOP_H__
#define __ANSCHEDULER_LOOP_H__

/**
 * Initialize the scheduling loop. This will allocate various structures for
 * the task queue.
 * @critical
 */
void anscheduler_loop_initialize();

/**
 * Pushes the current thread back to the run loop for another time.
 * @critical
 */
void anscheduler_loop_push_cur();

/**
 * Enters the scheduling loop.  This function should never return.
 * @critical
 */
void anscheduler_loop_run();

/**
 * Switches from this thread to a different thread.  In order to call this
 * method, you must have already set isPolling back to 0 so that no other task
 * will attempt to switch into this one.
 * @param task A referenced task
 * @param thread The thread in the task
 * @critical
 */
void anscheduler_loop_switch(task_t * task, thread_t * thread);

/**
 * There is an internal lock which needs to be held in order to change the
 * running state of any thread. This method locks that lock.
 * @critical
 */
void anscheduler_loop_lock();

/**
 * See anscheduler_loop_lock().
 * @critical
 */
void anscheduler_loop_unlock();

#endif
