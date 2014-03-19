#ifndef __ANSCHEDULER_TASK_H__
#define __ANSCHEDULER_TASK_H__

#include "types.h"

/**
 * Creates a task by allocating a new page table and mapping+copying new pages
 * in from `code` of length `len`.
 * @return a new task, or NULL if allocation failed.
 * @critical The copy operation is all done from a critical section, so this
 * really should only be done once at boot with the bootstrap task. After that
 * every new execution should be done with task_fork_code().
 */
task_t * anscheduler_task_create(void * code, uint64_t len);

/**
 * "Forks" a task. All this really means is that the task's code section is
 * copied. Pretty much no other data is retained.
 * @return a new task, or NULL if allocation failed.
 * @param task The task whose code to reference. A reference must be held for
 * this task.
 * @critical Referencing the old code section is much less slow since the only
 * thing to do is create page table entries.
 */
task_t * anscheduler_task_fork(task_t * task);

/**
 * Adds a task's to the scheduling queue.
 * @param task A reference is not needed here since the task is presumed not
 * to be part of the scheduling queue yet.
 * @critical
 */
void anscheduler_task_launch(task_t * task);

/**
 * Requests that a task be removed from the scheduling queue and notifies all
 * CPUs to stop running it.
 * @param task This task must have a reference to it.
 * @critical
 */
void anscheduler_task_kill(task_t * task);

/**
 * While references are held to a task, it cannot be killed.
 * @critical
 */
void anscheduler_task_reference(task_t * task);

/**
 * See task_reference(). Note that if the reference count gets to 0 and the
 * task has been killed, a kill system thread will be generated for the task
 * in this function.
 * @critical
 */
void anscheduler_task_dereference(task_t * task);

#endif
