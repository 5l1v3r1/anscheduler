#include <anscheduler/task.h>
#include <anscheduler/functions.h>
#include <anscheduler/loop.h> // for kernel threads
#include <anscheduler/thread.h> // for deallocation
#include "util.h" // for idxset

/**
 * @critical
 */
static task_t * _create_bare_task();

/**
 * @critical
 */
static bool _map_first_4mb(task_t * task);

/**
 * @critical
 */
static bool _copy_task_code(task_t * task, void * ptr, uint64_t len);

/**
 * @critical
 */
static void _dealloc_task_code(task_t * task, uint64_t pageCount);

/**
 * @critical
 */
static void _generate_kill_job(task_t * task);

/**
 * @noncritical
 */
static void _free_task_method(task_t * task);

/**
 * Like _dealloc_task_code(), but noncritical; dynamically calculates the
 * size of the code, unlike _dealloc_task_code().
 * @noncritical
 */
static void _dealloc_task_code_async(task_t * task);

/******************
 * Implementation *
 ******************/

task_t * anscheduler_task_create(void * code, uint64_t len) {
  task_t * task;
  if (!(task = _create_bare_task())) {
    return NULL;
  }
  
  if (!_copy_task_code(task, code, len)) {
    anidxset_free(&task->descriptors);
    anidxset_free(&task->stacks);
    anscheduler_vm_root_free(task->vm);
    anscheduler_free(task);
    return NULL;
  }
  
  return task;
}

task_t * anscheduler_task_fork(task_t * aTask) {
  task_t * task;
  if (!(task = _create_bare_task())) {
    return NULL;
  }
  
  task->codeRetainCount = aTask->codeRetainCount;
  
  // map each page until we reach a blank page
  uint64_t i = ANSCHEDULER_TASK_CODE_PAGE;
  for (; i < ANSCHEDULER_TASK_KERN_STACKS_PAGE; i++) {
    anscheduler_lock(&aTask->vmLock);
    uint16_t flags;
    uint64_t page = anscheduler_vm_lookup(aTask->vm, i, &flags);
    if (!(flags & ANSCHEDULER_PAGE_FLAG_PRESENT)) {
      anscheduler_unlock(&aTask->vmLock);
      break;
    }
    anscheduler_unlock(&aTask->vmLock);
    
    anscheduler_lock(&task->vmLock);
    if (!anscheduler_vm_map(task->vm, i, page, flags)) {
      anscheduler_unlock(&task->vmLock);
      
      // it failed
      anidxset_free(&task->descriptors);
      anidxset_free(&task->stacks);
      anscheduler_vm_root_free(task->vm);
      anscheduler_free(task);
      return NULL;
    }
    anscheduler_unlock(&task->vmLock);
  }
  
  anscheduler_inc(aTask->codeRetainCount);
  return task;
}

void anscheduler_task_kill(task_t * task) {
  // emulate a test-and-or operation
  anscheduler_lock(&task->killLock);
  task->isKilled = true;
  uint64_t ref = task->refCount;
  anscheduler_unlock(&task->killLock);
  
  if (ref) return; // wait for it to get released
  _generate_kill_job(task);
}

bool anscheduler_task_reference(task_t * task) {
  // emulate a test-and-add operation
  anscheduler_lock(&task->killLock);
  if (task->isKilled) {
    anscheduler_unlock(&task->killLock);
    return false;
  }
  task->refCount++;
  anscheduler_unlock(&task->killLock);
  return true;
}

void anscheduler_task_dereference(task_t * task) {
  anscheduler_lock(&task->killLock);
  if (!--task->refCount) {
    if (task->isKilled) {
      anscheduler_unlock(&task->killLock);
      return _generate_kill_job(task);
    }
  }
  anscheduler_unlock(&task->killLock);
}

/******************
 * Helper Methods *
 ******************/

static task_t * _create_bare_task() {
  // allocate memory for task structure
  task_t * task = anscheduler_alloc(sizeof(task_t));
  if (!task) return NULL;
  anscheduler_zero(task, sizeof(task_t));
  
  if (!(task->vm = anscheduler_vm_root_alloc())) {
    anscheduler_free(task);
    return NULL;
  }
  
  if (!anscheduler_idxset_init(&task->descriptors)) {
    anscheduler_vm_root_free(task->vm);
    anscheduler_free(task);
    return NULL;
  }
  
  if (!anscheduler_idxset_init(&task->stacks)) {
    anidxset_free(&task->descriptors);
    anscheduler_vm_root_free(task->vm);
    anscheduler_free(task);
    return NULL;
  }
  
  if (!_map_first_4mb(task)) {
    anidxset_free(&task->descriptors);
    anidxset_free(&task->stacks);
    anscheduler_vm_root_free(task->vm);
    anscheduler_free(task);
    return NULL;
  }
  
  return task;
}

static bool _map_first_4mb(task_t * task) {
  uint64_t i;
  for (i = 0; i < 0x400; i++) {
    uint64_t flags = ANSCHEDULER_PAGE_FLAG_PRESENT
      | ANSCHEDULER_PAGE_FLAG_WRITE
      | ANSCHEDULER_PAGE_FLAG_GLOBAL;
    if (!anscheduler_vm_map(task->vm, i, i, flags)) return false;
  }
  return true;
}

static bool _copy_task_code(task_t * task, void * ptr, uint64_t len) {
  uint64_t pageCount = (len >> 12) + (uint64_t)((len & 0xfff) != 0);
  uint64_t i, flags = ANSCHEDULER_PAGE_FLAG_PRESENT
    | ANSCHEDULER_PAGE_FLAG_WRITE
    | ANSCHEDULER_PAGE_FLAG_USER;
  
  // allocate each page for the task, and then copy the code into each page
  // one by one
  for (i = 0; i < pageCount; i++) {    
    void * buff = anscheduler_alloc(0x1000);
    if (!buff) {
      // free pages up to here
      _dealloc_task_code(task, i);
      return false;
    }
    
    // map the page
    uint64_t phys = anscheduler_vm_physical(((uint64_t)buff) >> 12);
    if (!anscheduler_vm_map(task->vm, i + ANSCHEDULER_TASK_CODE_PAGE,
                            phys, flags)) {
      anscheduler_free(buff);
      _dealloc_task_code(task, i);
      return false;
    }
    
    // copy the data
    uint16_t i;
    if (i != pageCount - 1 || (len & 0xfff) == 0) {
      uint64_t * bytes = (uint64_t *)buff;
      uint64_t * source = (uint64_t *)(ptr + (i << 12));
      for (i = 0; i < 0x200; i++) {
        bytes[i] = source[i];
      }
    } else {
      uint8_t * bytes = (uint8_t *)buff;
      uint8_t * source = (uint8_t *)(ptr + (i << 12));
      for (i = 0; i < (len & 0xfff); i++) {
        bytes[i] = source[i];
      }
    }
  }
  
  return true;
}

static void _dealloc_task_code(task_t * task, uint64_t pageCount) {
  uint64_t i;
  for (i = 0; i < pageCount; i++) {
    uint16_t flag = 0;
    uint64_t page = anscheduler_vm_lookup(task->vm,
                                          i + ANSCHEDULER_TASK_CODE_PAGE,
                                          &flag);
    if (!(flag & ANSCHEDULER_PAGE_FLAG_PRESENT)) {
      continue;
    }
    uint64_t vir = anscheduler_vm_virtual(page);
    anscheduler_free((void *)(vir << 12));
    anscheduler_vm_unmap(task->vm, i + ANSCHEDULER_TASK_CODE_PAGE);
  }
}

static void _generate_kill_job(task_t * task) {
  // Note: By the time we get here, we know nothing is running our task and
  // nothing will ever run it again.
  
  // Remove the task's threads from the queue. There is no need to lock the
  // threads lock anymore, because nothing could possibly be doing anything
  // to the threads array of this unreferenced task.
  thread_t * thread = task->firstThread;
  while (thread) {
    anscheduler_loop_delete(thread);
    thread = thread->next;
  }
  
  // Generate a kernel thread and pass it our task.
  anscheduler_loop_push_kernel(task, (void (*)(void *))_free_task_method);
}

static void _free_task_method(task_t * task) {
  // close the task's sockets, release the task's code (and free it, maybe),
  // free each thread's kernel and user stack
  
  // release the task's code
  if (!__sync_sub_and_fetch(task->codeRetainCount, 1)) {
    _dealloc_task_code_async(task);
    anscheduler_cpu_lock();
    anscheduler_free(task->codeRetainCount);
    anscheduler_cpu_unlock();
  }
  
  // free each thread and all its resources
  while (task->firstThread) {
    thread_t * thread = task->firstThread;
    task->firstThread = thread->next;
    anscheduler_thread_deallocate(task, thread);
    anscheduler_cpu_lock();
    void * stack = anscheduler_thread_kernel_stack(task, thread);
    anscheduler_free(stack);
    anscheduler_free(thread);
    anscheduler_cpu_unlock();
  }
  
  // TODO: close sockets here, once they are implemented and I understand
  // how they will work better.
  
  anscheduler_cpu_lock();
  anscheduler_loop_delete_cur_kernel();
}

static void _dealloc_task_code_async(task_t * task) {
  uint64_t i = ANSCHEDULER_TASK_CODE_PAGE;
  for (; i < ANSCHEDULER_TASK_KERN_STACKS_PAGE; i++) {
    anscheduler_cpu_lock();
    
    // no need to lock the vm structure anymore
    uint16_t flag = 0;
    uint64_t page = anscheduler_vm_lookup(task->vm, i, &flag);
    if (!(flag & ANSCHEDULER_PAGE_FLAG_PRESENT)) {
      anscheduler_cpu_unlock();
      break;
    }
    
    uint64_t vir = anscheduler_vm_virtual(page);
    anscheduler_free((void *)(vir << 12));
    anscheduler_vm_unmap(task->vm, i);
    
    anscheduler_cpu_unlock();
  }
}
