#include "pidmap.h"
#include <anscheduler/functions.h>

static uint64_t pmLock = 0;
static bool pmInitialized = 0;
static task_t * pidHashmap[0x100];

static uint64_t ppLock = 0;
static anidxset_root_t pidPool;
static bool ppInitialized = 0;

uint64_t anscheduler_pidmap_alloc_pid() {
  anscheduler_lock(&ppLock);
  if (!ppInitialized) {
    if (!anscheduler_idxset_init(&pidPool)) {
      anscheduler_abort("failed to initialize PID pool");
    }
    ppInitialized = true;
  }
  uint64_t result = anidxset_get(&pidPool);
  anscheduler_unlock(&ppLock);
  return result;
}

void anscheduler_pidmap_free_pid(uint64_t pid) {
  anscheduler_lock(&ppLock);
  if (!ppInitialized) {
    anscheduler_abort("cannot free PID when not initialized");
  }
  anscheduler_unlock(&ppLock);
}

void anschedulrer_pidmap_set(task_t * task) {
  
}

void anschedulrer_pidmap_unset(task_t * task) {
  
}

task_t anschedulrer_pidmap_get(uint64_t pid) {
  
}
