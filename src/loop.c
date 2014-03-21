#include <anscheduler/loop.h>
#include <anscheduler/functions.h>
#include <anscheduler/task.h>

static uint64_t loopLock = 0;
static uint64_t queueCount = 0;
static thread_t * firstThread = NULL;
static thread_t * lastThread = NULL;

static thread_t * _next_thread(uint64_t * nextTick);
static void _push_unconditional(thread_t * thread);
static void _delete_cur_kernel(void * unused);

void anscheduler_loop_push_cur() {
  thread_t * thread = anscheduler_cpu_get_thread();
  task_t * task = anscheduler_cpu_get_task();
  if (task) anscheduler_cpu_set_task(NULL);
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
  
  queueCount--;
  
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
  queueCount++;
  anscheduler_unlock(&loopLock);
}

void anscheduler_loop_run() {
  uint64_t nextTick = 0;
  thread_t * thread = _next_thread(&nextTick);
  anscheduler_timer_set(nextTick);
  if (!thread) {
    anscheduler_cpu_unlock();
    while (1) anscheduler_cpu_halt();
  }
  anscheduler_cpu_set_task(thread->task);
  anscheduler_cpu_set_thread(thread);
  anscheduler_thread_run(thread->task, thread);
}

void anscheduler_loop_push_kernel(void * arg, void (* fn)(void * arg)) {
  thread_t * thread = anscheduler_alloc(sizeof(thread_t));
  if (!thread) {
    anscheduler_abort("Failed to allocate kernel thread");
    return;
  }
  void * stack = anscheduler_alloc(0x1000);
  if (!stack) {
    anscheduler_free(thread);
    anscheduler_abort("Failed to allocate kernel thread stack.");
    return;
  }
  anscheduler_zero(thread, sizeof(thread_t));
  thread->stack = (uint64_t)stack;
  anscheduler_set_state(thread, stack, fn, arg);
  anscheduler_loop_push(thread);
}

void anscheduler_loop_delete_cur_kernel() {
  anscheduler_cpu_stack_run(NULL, _delete_cur_kernel);
}

void anscheduler_loop_switch(task_t * task, thread_t * thread) {
  // TODO: switch to the CPU dedicated stack first!
  anscheduler_loop_push_cur();
  anscheduler_cpu_set_task(task);
  anscheduler_cpu_set_thread(thread);
  anscheduler_thread_run(thread->task, thread);
}

static thread_t * _next_thread(uint64_t * nextTick) {
  anscheduler_lock(&loopLock);
  uint64_t i, max = queueCount;
  (*nextTick) = anscheduler_get_time() + (anscheduler_second_length() >> 5);
  for (i = 0; i < max; i++) {
    thread_t * th = firstThread;
    firstThread = th->queueNext;
    if (th->nextTimestamp > anscheduler_get_time()) {
      _push_unconditional(th);
      if (th->nextTimestamp < *nextTick) {
        (*nextTick) = th->nextTimestamp;
      }
      continue;
    }
    
    queueCount--;
    if (th->task) {
      if (!anscheduler_task_reference(th->task)) {
        continue;
      }
    }
    anscheduler_unlock(&loopLock);
    return th;
  }
  
  anscheduler_unlock(&loopLock);
  return NULL;
}

static void _push_unconditional(thread_t * thread) {
  if (lastThread) {
    lastThread->queueNext = thread;
    thread->queueLast = lastThread;
    thread->queueNext = NULL;
    lastThread = thread;
  } else {
    lastThread = (firstThread = thread);
    thread->queueNext = (thread->queueLast = NULL);
  }
}

static void _delete_cur_kernel(void * unused) {
  thread_t * thread = anscheduler_cpu_get_thread();
  task_t * task = anscheduler_cpu_get_task();
  if (task) anscheduler_abort("_delete_cur_kernel in non-kernel thread!");
  anscheduler_cpu_set_thread(NULL);
  anscheduler_free((void *)thread->stack);
  anscheduler_free(thread);
  anscheduler_loop_run();
}