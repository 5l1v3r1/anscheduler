#include "socketmap.h"
#include <anscheduler/functions.h>
#include <anscheduler/sockets.h>

static uint8_t _descriptor_hash(uint64_t desc);

void anscheduler_descriptor_set(task_t * task, socket_desc_t * desc) {
  uint8_t hash = _descriptor_hash(desc->descriptor);
  anscheduler_lock(&task->socketsLock);
  desc->last = NULL;
  if (!task->sockets[hash]) {
    task->sockets[hash] = desc;
    desc->next = NULL;
  } else {
    task->sockets[hash]->last = desc;
    desc->next = task->sockets[hash];
    task->sockets[hash] = desc;
  }
  anscheduler_unlock(&task->socketsLock);
}

/**
 * @critical
 */
void anscheduler_descriptor_delete(task_t * task,
                                   socket_desc_t * desc) {
  uint8_t hash = _descriptor_hash(desc->descriptor);
  anscheduler_lock(&task->socketsLock);
  if (desc->last) {
    desc->last->next = desc->next;
  } else {
    task->sockets[hash] = desc->next;
  }
  if (desc->next) desc->next->last = desc->last;
  desc->next = (desc->last = NULL);
  anscheduler_unlock(&task->socketsLock);
}

socket_desc_t * anscheduler_descriptor_find(task_t * task, uint64_t desc) {
  uint8_t hash = _descriptor_hash(desc);
  anscheduler_lock(&task->socketsLock);
  socket_desc_t * obj = task->sockets[hash];
  while (obj) {
    if (obj->desc == desc) {
      obj = anscheduler_socket_reference(obj) ? obj : NULL;
      anscheduler_unlock(&task->socketsLock);
      return obj;
    }
  }
  anscheduler_unlock(&task->socketsLock);
  return NULL;
}

void anscheduler_task_pending(task_t * task, socket_desc_t * desc) {
  anscheduler_lock(&task->pendingLock);
  if (desc->pendingLast || desc->pendingNext || task->firstPending == desc) {
    anscheduler_unlock(&task->pendingLock);
    return NULL;
  }
  
  if (task->firstPending) {
    task->firstPending->pendingLast = desc;
  }
  desc->pendingNext = task->firstPending;
  task->firstPending = desc;
  
  anscheduler_unlock(&task->pendingLock);
}

void anscheduler_task_not_pending(task_t * task, socket_desc_t * desc) {
  anscheduler_lock(&task->pendingLock);
  if (!desc->pendingLast && !desc->pendingNext) {
    if (task->firstPending != desc) {
      anscheduler_unlock(&task->pendingLock);
      return;
    }
  }
  if (!desc->pendingLast) {
    task->firstPending = desc->pendingNext;
  } else {
    desc->pendingLast->pendingNext = desc->pendingNext;
  }
  if (desc->pendingNext) {
    desc->pendingNext->pendingLast = desc->pendingLast;
  }
  anscheduler_unlock(&task->pendingLock);
}

socket_desc_t * anscheduler_task_pop_pending(task_t * task) {
  anscheduler_lock(&task->pendingLock);
  socket_desc_t * p = task->firstPending;
  while (p) {
    task->firstPending = p->pendingNext;
    if (anscheduler_socket_reference(p)) {
      anscheduler_unlock(&task->pendingLock);
      return p;
    }
    p = p->pendingNext;
  }
  anscheduler_unlock(&task->pendingLock);
  return NULL;
}

static uint8_t _descriptor_hash(uint64_t desc) {
  return (uint8_t)(desc & 0xf);
}
