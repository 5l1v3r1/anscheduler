#include <anscheduler/types.h>

void anscheduler_thread_run(task_t * task, thread_t * thread);

void anscheduler_set_state(thread_t * thread,
                           void * stack,
                           void * ip,
                           void * arg1);

bool anscheduler_save_return_state(thread_t * thread);

/**
 * Implemented in assembly.
 */
void antest_thread_run(task_t * task, thread_t * thread);

/**
 * Implemented in assembly.
 */
bool antest_save_return_state(thread_t * thread);
