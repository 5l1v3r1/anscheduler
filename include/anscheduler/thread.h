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
thread_t * anscheduler_thread_create(task_t * task);

/**
 * Adds a thread to a task.
 * @param task A referenced task.
 * @param thread The thread te queue and begin executing.
 * @critical
 */
void anscheduler_thread_add(task_t * task, thread_t * thread);

/**
 * Sets the current thread as 'polling' for events matching `flags`. If an
 * event has been received and masked already, the mask is returned and the
 * thread is not paused. Otherwise, zero is returned and the thread is set to
 * the 'polling' state.
 * @discussion You should save the state of the thread and switch to a CPU
 * stack before calling this.  However, if this doesn't return a mask of 0,
 * feel more than free to jump right back into the thread. Otherwise, just run
 * the run loop.
 * @critical
 */
uint64_t anscheduler_thread_poll(uint64_t flags);

/**
 * Call this to exit the current thread, presumably in a syscall handler.
 * @noncritical This potentially frees lots of memory, so it should be called
 * from a thread's execution state in kernel mode.
 */
void anscheduler_thread_exit();

/**
 * Deallocates a thread's user stack, locking the CPU when needed.
 * @noncritical This function can be interrupted or terminated at any time;
 * no memory will be leaked if proper cleanup is done later.
 */
void anscheduler_thread_deallocate(task_t * task, thread_t * thread);

/**
 * Returns the beginning of the kernel stack for a given thread or NULL if one
 * is not mapped.
 * @critical
 */
void * anscheduler_thread_kernel_stack(task_t * task, thread_t * thread);

#endif
