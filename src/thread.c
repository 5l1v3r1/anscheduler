#include <anscheduler/thread.h>
#include <anscheduler/task.h>
#include <anscheduler/functions.h>
#include <anscheduler/loop.h>

/**
 * @critical
 */
bool _alloc_kernel_stack(task_t * task, thread_t * thread);

/**
 * @critical
 */
bool _map_user_stack(task_t * task, thread_t * thread);

/**
 * @critical
 */
void _dealloc_kernel_stack(task_t * task, thread_t * thread);

thread_t * anscheduler_create_thread(task_t * task) {
  thread_t * thread = anscheduler_alloc(sizeof(thread_t));
  if (!thread) return NULL;
  
  // allocate a stack index and make sure we didn't go over the thread max
  anscheduler_lock(&task->stacksLock);
  uint64_t stack = anidxset_get(&task->stacks);
  anscheduler_unlock(&task->stacksLock);
  if (stack >= 0x100000) {
    // we should not put it back in the idxset because that'll just
    // contribute to the problem.
    anscheduler_free(thread);
    return NULL;
  }
  
  // setup the thread structure
  anscheduler_zero(thread, sizeof(thread_t));
  thread->task = task;
  thread->stack = stack;
  
  if (!_alloc_kernel_stack(task, thread)) {
    anscheduler_lock(&task->stacksLock);
    anidxset_put(&task->stacks, stack);
    anscheduler_unlock(&task->stacksLock);
    anscheduler_free(thread);
    return NULL;
  }
  
  // map the user stack
  if (!_map_user_stack(task, thread)) {
    _dealloc_kernel_stack(task, thread);
    anscheduler_lock(&task->stacksLock);
    anidxset_put(&task->stacks, stack);
    anscheduler_unlock(&task->stacksLock);
    anscheduler_free(thread);
    return NULL;
  }
  
  return thread;
}

void anscheduler_thread_add(task_t * task, thread_t * thread) {
  // add the thread to the task's linked list
  anscheduler_lock(&task->threadsLock);
  thread_t * next = task->firstThread;
  if (next) next->last = thread;
  task->firstThread = thread;
  thread->last = NULL;
  thread->next = next;
  anscheduler_unlock(&task->threadsLock);
  
  // add the thread to the queue
  anscheduler_loop_push(thread);
}

bool _alloc_kernel_stack(task_t * task, thread_t * thread) {
  // allocate the kernel stack
  void * buffer = anscheduler_alloc(0x1000);
  if (!buffer) return false;
  
  // map the kernel stack
  uint64_t kPage = ANSCHEDULER_TASK_KERN_STACKS_PAGE + thread->stack;
  uint64_t phyPage = anscheduler_vm_physical(((uint64_t)buffer) >> 12);
  uint16_t flags = ANSCHEDULER_PAGE_FLAG_PRESENT
    | ANSCHEDULER_PAGE_FLAG_WRITE;
  
  anscheduler_lock(&task->vmLock);
  if (!anscheduler_vm_map(task->vm, kPage, phyPage, flags)) {
    anscheduler_unlock(&task->vmLock);
    anscheduler_free(buffer);
    return false;
  }
  anscheduler_lock(&task->vmLock);
  return true;
}

bool _map_user_stack(task_t * task, thread_t * thread) {
  // map the user stack as to-allocate
  uint64_t flags = ANSCHEDULER_PAGE_FLAG_UNALLOC
    | ANSCHEDULER_PAGE_FLAG_USER
    | ANSCHEDULER_PAGE_FLAG_WRITE;
  uint64_t start = ANSCHEDULER_TASK_USER_STACKS_PAGE + (thread->stack << 8);
  
  uint64_t i;
  for (i = 0; i < 0x100; i++) {
    uint64_t page = i + start;
    anscheduler_lock(&task->vmLock);
    if (!anscheduler_vm_map(task->vm, page, 0, flags)) {
      // unmap all that we have mapped so far
      uint64_t j = 0;
      for (j = 0; j < i; j++) {
        page = j + start;
        anscheduler_vm_unmap(task->vm, page);
      }
      anscheduler_unlock(&task->vmLock);
      return false;
    }
    anscheduler_unlock(&task->vmLock);
  }
  
  return true;
}

void _dealloc_kernel_stack(task_t * task, thread_t * thread) {
  // get the phyPage and unmap it
  uint64_t page = ANSCHEDULER_TASK_KERN_STACKS_PAGE + thread->stack;
  anscheduler_lock(&task->vmLock);
  uint16_t flags;
  uint64_t phyPage = anscheduler_vm_lookup(task->vm, page, &flags);
  if (flags & ANSCHEDULER_PAGE_FLAG_PRESENT) {
    anscheduler_vm_unmap(task->vm, page);
  }
  anscheduler_unlock(&task->vmLock);
  
  if (flags & ANSCHEDULER_PAGE_FLAG_PRESENT) {
    uint64_t virPage = anscheduler_vm_virtual(phyPage);
    anscheduler_free((void *)(virPage << 12));
  }
}
