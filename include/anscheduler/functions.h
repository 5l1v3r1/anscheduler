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
 * size.
 * @return Address to the beginning of the new buffer, or 0/NULL on failure.
 */
void * anscheduler_alloc(uint64_t size);
void anscheduler_free(void * buffer);

void anscheduler_lock(uint64_t * ptr);
void anscheduler_unlock(uint64_t * ptr);

/*********************
 * Context Switching *
 *********************/

void anscheduler_thread_run(thread_t * thread);

/*************
 * Scheduler *
 *************/

void anscheduler_timer_set(uint32_t ticks);
void anscheduler_timer_set_far();
void anscheduler_timer_cancel();
uint64_t anscheduler_get_time();
uint64_t anscheduler_second_length();

/*************
 * Functions *
 *************/

void anscheduler_cpu_lock(); // enter critical section
void anscheduler_cpu_unlock(); // leave critical section
task_t * anscheduler_cpu_get_task();
thread_t * anscheduler_cpu_get_thread();
void anscheduler_cpu_set_task(task_t * task);
void anscheduler_cpu_set_thread(thread_t * thread);

/******************
 * Virtual Memory *
 ******************/

void * anscheduler_vm_root_alloc();
void anscheduler_vm_map(void * root,
                        uint64_t vpage,
                        uint64_t dpage,
                        uint16_t flags);
void anscheduler_vm_unmap(void * root, uint64_t vpage);
void anscheduler_vm_root_free(void * root);
