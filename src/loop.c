#include <anscheduler/loop.h>
#include <anscheduler/functions.h>
#include <anscheduler/task.h>

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

void anscheduler_loop_delete(thread_t * thread) {
  anscheduler_lock(&loopLock);
  
  // see if it is really in the list at all
  if (!thread->queueNext && !thread->queueLast) {
    if (thread != firstThread) {
      anscheduler_unlock(&loopLock);
      return;
    }
  }
  
  // if it is first and/or last in the list...
  if (firstThread == thread) {
    firstThread = thread->queueNext;
  }
  if (lastThread == thread) {
    lastThread = thread->queueLast;
  }
  
  // normal doubly-linked-list removal
  if (thread->queueLast) {
    thread->queueLast->queueNext = thread->queueNext;
  }
  if (thread->queueNext) {
    thread->queueNext->queueLast = thread->queueLast;
  }
  
  anscheduler_unlock(&loopLock);
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
    lastThread->queueNext = thread;
    thread->queueLast = lastThread;
    thread->queueNext = NULL;
    lastThread = thread;
  } else {
    firstThread = (lastThread = thread);
    thread->queueNext = (thread->queueLast = NULL);
  }
  anscheduler_unlock(&loopLock);
}
