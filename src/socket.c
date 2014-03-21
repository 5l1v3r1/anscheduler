#include <anscheduler/socket.h>
#include <anscheduler/functions.h>
#include <anscheduler/task.h>
#include <anscheduler/loop.h>

static uint8_t _descriptor_hash(uint64_t desc);
static void _socket_add(socket_link_t * link);
static void _socket_remove(socket_link_t * link);
static void _socket_destroy(socket_link_t * link);
static void _free_socket(socket_t * socket);
static void _wakeup_poller(socket_link_t * link);

socket_link_t * anscheduler_socket_new() {
  socket_t * socket = anscheduler_alloc(sizeof(socket_t));
  if (!socket) return NULL;
  socket_link_t * link = anscheduler_alloc(sizeof(socket_link_t));
  if (!link) {
    anscheduler_free(socket);
    return NULL;
  }
  anscheduler_zero(socket, sizeof(socket_t));
  anscheduler_zero(link, sizeof(socket_link_t));
  
  task_t * task = anscheduler_cpu_get_task();
  link->socket = socket;
  link->isConnector = true;
  socket->connector = task;
  
  anscheduler_lock(&task->descriptorsLock);
  uint64_t desc = anidxset_get(&task->descriptors);
  anscheduler_unlock(&task->descriptorsLock);
  link->descriptor = desc;
  link->task = task;
  link->refCount = 1;
  
  _socket_add(link);
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
  anscheduler_lock(&socket->closeLock);
  socket->refCount--;
  if (!socket->refCount && socket->isClosed) {
    anscheduler_unlock(&socket->closeLock);
    _socket_destroy(socket);
    return;
  }
  anscheduler_unlock(&socket->closeLock);
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

void anscheduler_socket_close(socket_link_t * socket, uint64_t code) {
  anscheduler_lock(&socket->closeLock);
  socket->isClosed = true;
  socket->closeCode = code;
  anscheduler_unlock(&socket->closeLock);
}

static uint8_t _descriptor_hash(uint64_t desc) {
  return (uint8_t)(desc & 0xf);
}

static void _socket_add(socket_link_t * link) {
  task_t * task = link->task;
  uint8_t hash = _descriptor_hash(link->descriptor);
  anscheduler_lock(&task->socketsLock);
  link->next = task->sockets[hash];
  link->last = NULL;
  if (task->sockets[hash]) {
    task->sockets[hash]->last = link;
  }
  task->sockets[hash] = link;
  anscheduler_unlock(&task->socketsLock);
}

static void _socket_remove(socket_link_t * link) {
  task_t * task = link->task;
  uint8_t hash = _descriptor_hash(link->descriptor);
  anscheduler_lock(&task->socketsLock);
  if (!link->last) {
    task->sockets[hash] = link->next;
  } else {
    link->last->next = link->next;
  }
  if (link->next) link->next->last = link->last;
  anscheduler_unlock(&task->socketsLock);
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
  
  if (hasOtherEnd) {
    socket_msg_t * msg = anscheduler_alloc(sizeof(socket_msg_t));
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
  task_t * otherEnd = NULL;
  anscheduler_lock(&link->socket->connRecLock);
  if (link->isConnector) {
    otherEnd = link->socket->receiver;
  } else {
    otherEnd = link->socket->connector;
  }
  if (otherEnd) {
    if (!anscheduler_task_reference(otherEnd)) {
      otherEnd = NULL;
    }
  }
  anscheduler_unlock(&link->socket->connRecLock);
  if (!otherEnd) return;
  
  anscheduler_lock(&otherEnd->threadsLock);
  thread_t * th = otherEnd->firstThread;
  while (th) {
    if (__sync_fetch_and_and(&th->isPolling, 0)) {
      anscheduler_unlock(&otherEnd->threadsLock);
      thread_t * curThread = anscheduler_cpu_get_thread();
      bool res = anscheduler_save_return_state(curThread);
      if (res) return; // resuming
      anscheduler_loop_switch(otherEnd, th);
    }
    th = th->next;
  }
  anscheduler_unlock(&otherEnd->threadsLock);
}
