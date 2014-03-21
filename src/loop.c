#include <anscheduler/loop.h>

static uint64_t loopLock = 0;
static thread_t * firstThread = NULL;
static thread_t * lastThread = NULL;

void anscheduler_loop_push_cur() {
  thread_t * thread = anscheduler_cpu_get_thread();
  task_t * task = anscheduler_cpu_get_task();
  anscheduler_cpu_set_task(NULL);
  anscheduler_cpu_set_thread(NULL);
  
  anscheduler_loop_push(thread);
  
  if (task) anscheduler_task_dereference(task);
}

void anscheduler_loop_push(thread_t * thread) {
  // if the task has been killed, we won't push it
  if (thread->task) {
    anscheduler_lock(&thread->task->killLock);
    if (thread->task->isKilled) {
      anscheduler_unlock(&thread->task->killLock);
      return;
    }
    anscheduler_unlock(&thread->task->killLock);
  }
  
  anscheduler_lock(&loopLock);
  if (lastThread) {
    lastThread->next = thread;
    thread->last = lastThread;
    thread->next = NULL;
    lastThread = thread;
  } else {
    nextThread = (lastThread = thread);
    thread->next = (thread->last = NULL);
  }
  anscheduler_unlock(&loopLock);
}
