#include "types.h"

#define ANSCHEDULER_PAGE_FLAG_PRESENT 1
#define ANSCHEDULER_PAGE_FLAG_WRITE 2
#define ANSCHEDULER_PAGE_FLAG_USER 4
#define ANSCHEDULER_PAGE_FLAG_GLOBAL 0x100

/*******************
 * General Purpose *
 *******************/

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

