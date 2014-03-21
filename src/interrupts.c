#include <anscheduler/interrupts.h>
#include <anscheduler/loop.h>
#include <anscheduler/task.h>
#include <anscheduler/functions.h>

static thread_t * interruptThread = NULL;
static uint64_t threadLock = 0;

void anscheduler_page_fault(void * ptr, uint64_t _flags) {
  // TODO: rewrite this function to use the _flags argument.
  
  // Otherwise, kill the task.
  
  task_t * task = anscheduler_cpu_get_task();
  if (!task) anscheduler_abort("kernel thread caused page fault!");
  
  anscheduler_lock(&task->vmLock);
  uint16_t flags;
  uint64_t faultPage = ((uint64_t)ptr) >> 12;
  uint64_t entry = anscheduler_vm_lookup(task->vm, faultPage, &flags);
  if ((flags & ANSCHEDULER_PAGE_FLAG_UNALLOC) && !entry) {
    // allocate the page
    flags = ANSCHEDULER_PAGE_FLAG_USER
      | ANSCHEDULER_PAGE_FLAG_PRESENT
      | ANSCHEDULER_PAGE_FLAG_WRITE;
    void * ptr = anscheduler_alloc(0x1000);
    anscheduler_zero(ptr, 0x1000);
    anscheduler_vm_map(task->vm, faultPage, ((uint64_t)ptr) >> 12, flags);
  } else if (flags & ANSCHEDULER_PAGE_FLAG_PRESENT) {
    if (!(flags & ANSCHEDULER_PAGE_FLAG_USER)) {
      anscheduler_unlock(&task->vmLock);
      anscheduler_task_exit(ANSCHEDULER_TASK_KILL_REASON_MEMORY);
    }
  } else {
    anscheduler_unlock(&task->vmLock);
    anscheduler_task_exit(ANSCHEDULER_TASK_KILL_REASON_MEMORY);
  }
  anscheduler_unlock(&task->vmLock);
  anscheduler_thread_run(task, anscheduler_cpu_get_thread());
}

void anscheduler_irq(uint8_t irqNumber) {
  // while our threadLock is held, the interruptThread cannot be deallocated
  // so we don't have to worry about it going away.
  anscheduler_lock(&threadLock);
  if (!interruptThread) {
    anscheduler_unlock(&threadLock);
    return;
  }
  task_t * task = interruptThread->task;
  if (!anscheduler_task_reference(task)) {
    anscheduler_unlock(&threadLock);
    return;
  }
  
  anscheduler_or_32(&interruptThread->irqs, (1 << irqNumber));
  bool result = __sync_fetch_and_and(&interruptThread->isPolling, 0);
  anscheduler_unlock(&threadLock);
  
  if (result) { // the thread was polling!
    // we know our reference to interruptThread is still valid, because if
    // a thread is polling, it cannot be killed unless its task is killed, and
    // we hold a reference to its task.
    anscheduler_loop_switch(task, interruptThread);
  } else {
    anscheduler_task_dereference(task);
  }
}

thread_t * anscheduler_interrupt_thread() {
  anscheduler_lock(&threadLock);
  thread_t * result = interruptThread;
  anscheduler_unlock(&threadLock);
  return result;
}

void anscheduler_set_interrupt_thread(thread_t * thread) {
  anscheduler_lock(&threadLock);
  interruptThread = thread;
  anscheduler_unlock(&threadLock);
}

void anscheduler_interrupt_thread_cmpnull(thread_t * thread) {
  anscheduler_lock(&threadLock);
  if (interruptThread == thread) {
    interruptThread = NULL;
  }
  anscheduler_unlock(&threadLock);
}
