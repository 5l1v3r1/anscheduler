/**
 * Test that the scheduler can run a single thread.
 */

#include "env/user_thread.h"
#include "env/threading.h"
#include <anscheduler/thread.h>
#include <anscheduler/task.h>
#include <stdio.h>

void proc_enter(void * unused);
void user_thread_test();

int main() {
  // create one CPU
  antest_launch_thread(NULL, proc_enter);
  
  // launch one task
  char useless[2];
  task_t * task = anscheduler_task_create(useless, 2);
  anscheduler_task_launch(task);
  
  thread_t * thread = anscheduler_thread_create(task);
  antest_configure_user_thread(thread, user_thread_test);
  
  anscheduler_thread_add(task, thread);
  anscheduler_task_dereference(task);
  
  return 0;
}

void proc_enter(void * unused) {
  while (1) {
    anscheduler_cpu_halt();
  }
}

void user_thread_test() {
  printf("running user thread!\n");
  exit(0);
}
