#ifndef __ANSCHEDULER_FUNCTIONS_H__
#define __ANSCHEDULER_FUNCTIONS_H__

/**
 * These are platform-specific methods which must be implemented by whatever
 * platform you wish to use anscheduler on. It is expected that an
 * implementation for each of these functions be present at runtime.
 */

#include "types.h"

#define ANSCHEDULER_PAGE_FLAG_PRESENT 1
#define ANSCHEDULER_PAGE_FLAG_WRITE 2
#define ANSCHEDULER_PAGE_FLAG_USER 4
#define ANSCHEDULER_PAGE_FLAG_GLOBAL 0x100
#define ANSCHEDULER_PAGE_FLAG_UNALLOC 0x200

/*******************
 * General Purpose *
 *******************/

/**
 * Allocates at least `size` bytes. For kernels with page-sized allocators
 * only, this should return NULL if `size` is greater than the maximum page
 * size. Additionally, this address must be aligned on a 4KB page boundary.
 * @return Address to the beginning of the new buffer, or NULL on failure.
 * @critical
 */
void * anscheduler_alloc(uint64_t size);
void anscheduler_free(void * buffer);

void anscheduler_lock(uint64_t * ptr);
void anscheduler_unlock(uint64_t * ptr);

void anscheduler_abort(const char * error);
void anscheduler_zero(void * buf, int len);
void anscheduler_inc(uint64_t * ptr);

/*********************
 * Context Switching *
 *********************/

/**
 * @param task Must have a reference to it.
 */
void anscheduler_thread_run(task_t * task, thread_t * thread);

/*********
 * Timer *
 *********/

void anscheduler_timer_set(uint32_t ticks);
void anscheduler_timer_set_far();
void anscheduler_timer_cancel();
uint64_t anscheduler_get_time();
uint64_t anscheduler_second_length();

/***************
 * CPU Context *
 ***************/

/**
 * Enter a critical section.
 * @noncricital Why would you call this if not from a noncritical section?
 */
void anscheduler_cpu_lock();

/**
 * Leave a critical section.
 * @critical You should only unlock the CPU if you have a reason to!.
 */
void anscheduler_cpu_unlock();

/**
 * @critical
 */
task_t * anscheduler_cpu_get_task();

/**
 * @critical
 */
thread_t * anscheduler_cpu_get_thread();

void anscheduler_cpu_set_task(task_t * task);
void anscheduler_cpu_set_thread(thread_t * thread);

void anscheduler_cpu_notify_invlpg(task_t * task);
void anscheduler_cpu_notify_dead(task_t * task);

/**
 * Calls a function `fn` with an argument `arg` using a stack dedicated to
 * this CPU. This is useful for the last part of any thread kill, when the
 * thread's own kernel stack must be freed.
 * @critical
 */
void anscheduler_cpu_stack_run(void * arg, void (* fn)(void * a));

/**
 * @noncritical Calling from a critical section would hang this CPU.
 */
void anscheduler_cpu_halt(); // wait until timer or interrupt

/******************
 * Virtual Memory *
 ******************/

/**
 * @noncritical This should be relatively simple and static.
 */
uint64_t anscheduler_vm_physical(uint64_t virt); // global VM translation

/**
 * @noncritical See anscheduler_vm_physical().
 */
uint64_t anscheduler_vm_virtual(uint64_t phys); // global VM translation

void * anscheduler_vm_root_alloc();
bool anscheduler_vm_map(void * root,
                        uint64_t vpage,
                        uint64_t dpage,
                        uint16_t flags);
void anscheduler_vm_unmap(void * root, uint64_t vpage);
uint64_t anscheduler_vm_lookup(void * root,
                               uint64_t vpage,
                               uint16_t * flags);
void anscheduler_vm_root_free(void * root);

#endif
