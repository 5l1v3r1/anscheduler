/**
 * Test that the scheduler can run a single thread.
 */

#include "env/user_thread.h"
#include "env/threading.h"
#include "env/context.h"
#include <anscheduler/thread.h>
#include <anscheduler/task.h>
#include <anscheduler/loop.h>
#include <stdio.h>
#include <unistd.h>

#define THREADS_PER_CPU 4
#define CPU_COUNT 4

static int threadsDone = 0;

void proc_enter(void * unused);
void create_a_thread();
void thread_body();

int main() {
  // create one CPU
  int i;
  for (i = 0; i < CPU_COUNT; i++) {
    antest_launch_thread(NULL, proc_enter);
  }
  
  while (1) {
    sleep(0xffffffff);
  }
  
  return 0;
}

void proc_enter(void * unused) {
  int i;
  for (i = 0; i < THREADS_PER_CPU; i++) {
    create_a_thread();
  }
  
  anscheduler_loop_run();
}

void create_a_thread() {
  char useless[2];
  task_t * task = anscheduler_task_create(useless, 2);
  anscheduler_task_launch(task);
  
  thread_t * thread = anscheduler_thread_create(task);
  antest_configure_user_thread(thread, thread_body);
  
  anscheduler_thread_add(task, thread);
  anscheduler_task_dereference(task);
}

void thread_body() {
  int i;
  for (i = 0; i < 10; i++) {
    anscheduler_cpu_halt();
  }
  uint64_t dest = CPU_COUNT * THREADS_PER_CPU;
  if (__sync_add_and_fetch(&threadsDone, 1) == dest) {
    printf("all threads completed!\n");
    //exit(0);
  }
  anscheduler_cpu_lock();
  printf("calling exit...\n");
  anscheduler_task_exit(0);
}
