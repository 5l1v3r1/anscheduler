#ifndef __ANSCHEDULER_THREAD_H__
#define __ANSCHEDULER_THREAD_H__

#include "types.h"

/**
 * Creates a thread and all its associated resources.
 * @param task A referenced task.
 * @return a new thread, or NULL if allocation failed.
 * @critical This doesn't take too much time because the user stack is
 * allocated lazily.
 */
thread_t * anscheduler_thread_create(task_t * task, void * rip);

/**
 * Adds a thread to a task.
 * @param task A referenced task.
 * @param thread The thread te queue and begin executing.
 */
void anscheduler_thread_add(task_t * task, thread_t * thread);

#endif
