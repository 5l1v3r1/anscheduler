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
