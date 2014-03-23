#include "interrupts.h"
#include "threading.h"
#include "context.h"
#include <anscheduler/loop.h>

void antest_handle_timer_interrupt() {
  thread_t * th = anscheduler_cpu_get_thread();
  if (th) {
    bool retVal = anscheduler_save_return_state(th);
    if (retVal) return;
  }
  
  antest_get_current_cpu_info()->isLocked = true;
  
  // run the next thing!
  anscheduler_loop_resign();
}
