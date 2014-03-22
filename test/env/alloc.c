#include "alloc.h"
#include "threading.h"
#include <assert.h>

void * anscheduler_alloc(uint64_t size) {
  assert(antest_get_current_cpu_info()->isLocked);
  if (size > 0x1000) return NULL;
  
  void * buf;
  posix_memalign(&buf, 0x1000, size);
  return buf;
}

void anscheduler_free(void * buffer) {
  assert(antest_get_current_cpu_info()->isLocked);
  free(buffer);
}
