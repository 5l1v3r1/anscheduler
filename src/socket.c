#include <anscheduler/socket.h>
#include <anscheduler/functions.h>
#include <anscheduler/task.h>
#include <anscheduler/loop.h>

static uint8_t _descriptor_hash(uint64_t desc);
static void _socket_add(socket_link_t * link);
static void _socket_destroy(socket_link_t * link);
static void _free_socket(socket_t * socket);

/**
 * Dereferences a socket, pushing a closed message to the message queue if it
 * was closed and the ref count reached 0. If closed, false is returned,
 * otherwise true.
 * @param link A referenced link.
 * @critical
 */
static bool _socket_dereference(socket_link_t * link);

/**
 * @param link A link, which will be dereferenced before return
 * @critical -> @noncritical -> @critical
 */
static void _wakeup_poller(socket_link_t * link);

/**
 * Finds the link for the other end of a socket. If NULL is not returned,
 * then both the other link AND the other task will be referenced.
 * @critical
 */
static socket_link_t * _find_other_end(socket_link_t * link);

static socket_link_t * _task_add_new_link(socket_t * socket,
                                          task_t * task,
                                          bool isConnector);
static void _task_add_pending(socket_link_t * link);
static void _task_remove_pending(socket_link_t * link);

socket_link_t * anscheduler_socket_new() {
  socket_t * socket = anscheduler_alloc(sizeof(socket_t));
  if (!socket) return NULL;
  anscheduler_zero(socket, sizeof(socket_t));
  
  task_t * task = anscheduler_cpu_get_task();
  socket_link_t * link = _task_add_new_link(socket, task, true);
  if (!link) {
    anscheduler_free(socket);
  }
  return link;
}

socket_link_t * anscheduler_socket_for_descriptor(uint64_t desc) {
  task_t * task = anscheduler_cpu_get_task();
  uint8_t hash = _descriptor_hash(desc);
  anscheduler_lock(&task->socketsLock);
  socket_link_t * link = task->sockets[hash];
  while (link) {
    if (link->descriptor == desc) {
      bool result = anscheduler_socket_reference(link);
      anscheduler_unlock(&task->socketsLock);
      return result ? link : NULL;
    }
    link = link->next;
  }
  anscheduler_unlock(&task->socketsLock);
  return NULL;
}

bool anscheduler_socket_reference(socket_link_t * socket) {
  anscheduler_lock(&socket->closeLock);
  if (socket->isClosed) {
    anscheduler_unlock(&socket->closeLock);
    return false;
  }
  socket->refCount++;
  anscheduler_unlock(&socket->closeLock);
  return true;
}

void anscheduler_socket_dereference(socket_link_t * socket) {
  
}

bool anscheduler_socket_msg(socket_link_t * socket, socket_msg_t * msg) {
  anscheduler_lock(&socket->socket->msgsLock);
  uint64_t * destCount = NULL;
  socket_msg_t ** destFirst = NULL, ** destLast = NULL;
  if (socket->isConnector) {
    destCount = &socket->socket->forReceiverCount;
    destFirst = &socket->socket->forReceiverFirst;
    destLast = &socket->socket->forReceiverLast;
  } else {
    destCount = &socket->socket->forConnectorCount;
    destFirst = &socket->socket->forConnectorFirst;
    destLast = &socket->socket->forConnectorLast;
  }
  
  if (msg->type == ANSCHEDULER_MSG_TYPE_DATA) {
    if ((*destCount) >= ANSCHEDULER_MAX_MSG_BUFFER) {
      anscheduler_unlock(&socket->socket->msgsLock);
      return false;
    }
  }
  
  (*destCount)++;
  msg->last = *destLast;
  if (*destLast) (*destLast)->next = msg;
  else (*destFirst) = msg;
  (*destLast) = msg;
  
  anscheduler_unlock(&socket->socket->msgsLock);
  
  _wakeup_poller(socket);
  return true;
}

socket_msg_t * anscheduler_socket_msg_data(void * data, uint64_t len) {
  if (len > 0xfe8) return NULL;
  socket_msg_t * msg = anscheduler_alloc(sizeof(socket_msg_t));
  if (!msg) return NULL;
  
  msg->type = ANSCHEDULER_MSG_TYPE_DATA;
  msg->len = len;
  
  const char * src = (const char *)data;
  int i;
  for (i = 0; i < len; i++) {
    msg->message[i] = src[i];
  }
  return msg;
}

socket_msg_t * anscheduler_socket_read(socket_link_t * socket) {
  anscheduler_lock(&socket->socket->msgsLock);
  
  uint64_t * destCount = NULL;
  socket_msg_t ** destFirst = NULL, ** destLast = NULL;
  if (socket->isConnector) {
    destCount = &socket->socket->forReceiverCount;
    destFirst = &socket->socket->forReceiverFirst;
    destLast = &socket->socket->forReceiverLast;
  } else {
    destCount = &socket->socket->forConnectorCount;
    destFirst = &socket->socket->forConnectorFirst;
    destLast = &socket->socket->forConnectorLast;
  }
  
  if (!(*destCount)) {
    anscheduler_unlock(&socket->socket->msgsLock);
    return NULL;
  }
  
  (*destCount)--;
  socket_msg_t * msg = *destFirst;
  if (msg->next) { 
    (*destFirst) = msg->next;
    msg->next->last = NULL;
  } else {
    (*destLast) = ((*destFirst) = NULL);
  }
  
  anscheduler_unlock(&socket->socket->msgsLock);
  return msg;
}

bool anscheduler_socket_connect(socket_link_t * socket, task_t * task) {
  // create a new file descriptor on the other end
  socket_link_t * link = _task_add_new_link(socket->socket, task, false);
  if (!link) return false;
  
  // allocate the connect message
  socket_msg_t * msg = anscheduler_alloc(sizeof(socket_msg_t));
  if (!msg) anscheduler_abort("Failed to allocate connect message");
  msg->type = ANSCHEDULER_MSG_TYPE_CONNECT;
  msg->len = 8;
  (*((uint64_t *)msg->message)) = link->descriptor;
  if (!anscheduler_socket_msg(link, msg)) {
    anscheduler_abort("Failed to send socket connect message");
  }
  return true;
}

void anscheduler_socket_close(socket_link_t * socket, uint64_t code) {
  anscheduler_lock(&socket->closeLock);
  socket->isClosed = true;
  socket->closeCode = code;
  anscheduler_unlock(&socket->closeLock);
}



static void _socket_destroy(socket_link_t * link) {
  _socket_remove(link);
  
  bool hasOtherEnd = false;
  anscheduler_lock(&link->socket->connRecLock);
  if (link->isConnector) {
    link->socket->connector = NULL;
    hasOtherEnd = link->socket->receiver != NULL;
  } else {
    link->socket->receiver = NULL;
    hasOtherEnd = link->socket->connector != NULL;
  }
  anscheduler_unlock(&link->socket->connRecLock);
  
  anscheduler_lock(&link->task->pendingLock);
  _task_remove_pending(link);
  anscheduler_unlock(&link->task->pendingLock);
  
  if (hasOtherEnd) {
    socket_msg_t * msg = anscheduler_alloc(sizeof(socket_msg_t));
    if (!msg) anscheduler_abort("Failed to allocate closed message");
    msg->type = link->closeCode;
    msg->len = 8;
    (*((uint64_t *)msg->message)) = link->closeCode;
    anscheduler_socket_msg(link, msg);
  } else {
    _free_socket(link->socket);
  }
  anscheduler_free(link);
}

static void _free_socket(socket_t * socket) {
  // no need to lock anything anymore
  socket_msg_t * msg = socket->forConnectorFirst;
  while (msg) {
    anscheduler_free(msg);
    msg = msg->next;
  }
  msg = socket->forReceiverFirst;
  while (msg) {
    anscheduler_free(msg);
    msg = msg->next;
  }
  anscheduler_free(socket);
}

static void _wakeup_poller(socket_link_t * link) {
  if (__sync_fetch_and_or(link->isSending, 1)) return;
  socket_link_t * otherLink = _find_other_end(link);
  anscheduler_socket_dereference(link);
  if (!otherEnd) return;
  
  // add this guy to the pending list
  anscheduler_lock(&otherTask->pendingLock);
  _task_add_pending(otherEnd);
  anscheduler_unlock(&otherTask->pendingLock);
  
  // execute whatever polling thread there might be
  anscheduler_lock(&otherTask->threadsLock);
  thread_t * th = otherTask->firstThread;
  while (th) {
    if (__sync_fetch_and_and(&th->isPolling, 0)) {
      anscheduler_unlock(&otherTask->threadsLock);
      thread_t * curThread = anscheduler_cpu_get_thread();
      anscheduler_socket_dereference(otherEnd);
      bool res = anscheduler_save_return_state(curThread);
      if (res) return;
      anscheduler_loop_switch(otherTask, th);
    }
    th = th->next;
  }
  anscheduler_unlock(&otherTask->threadsLock);
  
  // if we get to this point, we have not found any polling threads, so we
  // must dereference the task.
  anscheduler_socket_dereference(otherEnd);
  anscheduler_task_dereference(otherTask);
}

static socket_link_t * _find_other_end(socket_link_t * link) {
  socket_link_t * otherEnd = NULL;
  
  anscheduler_lock(&link->socket->connRecLock);
  if (link->isConnector) {
    otherEnd = link->socket->receiver;
  } else {
    otherEnd = link->socket->connector;
  }
  
  // reference the other end socket AND task!
  if (otherEnd) {
    if (!anscheduler_socket_reference(otherEnd)) {
      otherEnd = NULL;
    } else {
      if (!anscheduler_task_reference(otherEnd->task)) {
        anscheduler_socket_dereference(otherEnd);
        otherEnd = NULL;
      }
    }
  }
  anscheduler_unlock(&link->socket->connRecLock);
  return otherEnd;
}

static socket_link_t * _task_add_new_link(socket_t * socket,
                                          task_t * task,
                                          bool isConnector) {
  
}

static void _task_add_pending(socket_link_t * link) {
  task_t * task = link->task;
  if (link->pendingNext || !link->pendingLast) return;
  if (task->firstPending == link) return;
  link->pendingNext = task->firstPending;
  link->pendingLast = NULL;
  task->firstPending = link;
}

static void _task_remove_pending(socket_link_t * link) {
  task_t * task = link->task;
  if (task->firstPending == link) {
    task->firstPending = link->pendingNext;
    if (link->pendingNext) {
      link->pendingNext->pendingLast = NULL;
    }
    return;
  }
  if (!link->pendingLast) return;
  link->pendingLast->pendingNext = link->pendingNext;
  if (link->pendingNext) {
    link->pendingNext->pendingLast = link->pendingLast;
  }
}
