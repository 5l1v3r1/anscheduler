#include "context.h"
#include "threading.h"
#include <string.h>

void anscheduler_thread_run(task_t * task, thread_t * thread) {
  cpu_info * info = antest_get_current_cpu_info();
  if (info->isLocked != (bool)thread->state.cpuLocked) {
    info->isLocked = (bool)thread->state.cpuLocked;
  }
  antest_thread_run(task, thread);
}

void anscheduler_set_state(thread_t * thread,
                           void * stack,
                           void * ip,
                           void * arg1) {
  bzero(&thread->state, sizeof(thread->state));
  
  __asm__("pushfq\npop %0" : "=r" (thread->state.flags));
  thread->state.rip = (uint64_t)ip;
  thread->state.rsp = (uint64_t)stack;
  thread->state.rbp = (uint64_t)stack;
  thread->state.rdi = (uint64_t)arg1;
}

bool anscheduler_save_return_state(thread_t * thread) {
  uint64_t retAddr, rbp;
  __asm__("movq 8(%%rbp), %0\nmov %%rbp, %1" : "=r" (retAddr), "=r" (rbp));
  printf("saving return to %llx, rbp = %llx\n", retAddr, rbp);
  
  cpu_info * info = antest_get_current_cpu_info();
  thread->state.cpuLocked = (uint64_t)info->isLocked;
  bool res = antest_save_return_state(thread);
  
  if (res) {
    uint64_t nretAddr;
    __asm__("mov 8(%%rbp), %0\nmov %%rbp, %1" : "=r" (nretAddr), "=r" (rbp));
    printf("return to %llx, old addr was %llx, rbp is %llx\n", nretAddr, retAddr, rbp);
  }
  return res;
}
