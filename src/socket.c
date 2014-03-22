#include <anscheduler/socket.h>
#include "socketmap.h"

typedef struct {
  socket_msg_t * message;
  socket_desc_t * descriptor; // referenced
} msginfo_t;

/**
 * @critical
 */
static socket_desc_t * _create_descriptor(socket_t * socket,
                                                task_t * task,
                                                bool isConnector);

/**
 * @noncritical Run from a kernel thread, though.
 */
static void _socket_hangup(socket_descriptor_t * socket);

/**
 * Send a socket message to the other end of a socket.
 * @param dest A referenced socket. When a socket is referenced, you °know*
 * that the destination task will not be deallocated, although it may be
 * killed.
 * @critical
 */
static bool _push_message(socket_descriptor_t * dest, socket_msg_t * msg);

/**
 * Wakes up the task for a socket descriptor. If the task has been killed,
 * this will not complete it's job. No matter what, the passed descriptor's
 * reference will be consumed.
 * @critical -> @noncritical -> @critical
 */
static void _wakeup_endpoint(socket_descriptor_t * dest);

/**
 * @noncritical Run from a kernel thread
 */
static void _async_msg(msginfo_t * info);

socket_desc_t * anscheduler_socket_new() {
  socket_t * socket = anscheduler_alloc(sizeof(socket_t));
  if (!socket) return NULL;
  anscheduler_zero(socket, sizeof(socket_t));
  return _create_descriptor(socket, anscheduler_cpu_get_task(), true);
}

socket_desc_t * anscheduler_socket_for_descriptor(uint64_t desc) {
  task_t * task = anscheduler_cpu_get_task();
  return anscheduler_descriptor_find(task, desc);
}

bool anscheduler_socket_reference(socket_desc_t * socket) {
  anscheduler_lock(&socket->closeLock);
  if (socket->isClosed) {
    anscheduler_unlock(&socket->closeLock);
    return false;
  }
  socket->refCount++;
  anscheduler_unlock(&socket->closeLock);
  return true;
}

void anscheduler_socket_dereference(socket_desc_t * socket) {
  anscheduler_lock(&socket->closeLock);
  if (socket->isClosed && !socket->refCount) {
    anscheduler_unlock(&socket->closeLock);
    return false;
  }
  if (!(socket->refCount--) && socket->isClosed) {
    anscheduler_unlock(&socket->closeLock);
    
    // we know the task is still alive because the socket is still in the
    // task's socket list as of now, and the task cannot die until
    // every socket it owns has died.
    anscheduler_task_not_pending(socket->task, socket);
    anscheduler_descriptor_delete(socket->task, socket);
    
    // now, we don't know the task is alive, but it doesn't matter anymore
    anscheduler_loop_push_kernel(socket, (void (*)(void *))_socket_hangup);
    return;
  }
  anscheduler_unlock(&socket->closeLock);
}

bool anscheduler_socket_msg(socket_desc_t * socket,
                            socket_msg_t * msg) {
  // gain a reference to the other end of the socket
  socket_desc_t * otherEnd = NULL;
  anscheduler_lock(&socket->connRecLock);
  if (socket->isConnector) {
    otherEnd = socket->receiver;
  } else {
    otherEnd = socket->connector;
  }
  if (otherEnd) {
    otherEnd = anscheduler_socket_reference(otherEnd) ? otherEnd : NULL;
  }
  anscheduler_unlock(&socket->connRecLock);
  
  if (!otherEnd) return false;
  if (!_push_message(otherEnd, msg)) {
    anscheduler_socket_dereference(otherEnd);
    return false;
  }
  
  anscheduler_socket_dereference(socket); // cannot hold a ref across this
  _wakeup_endpoint(otherEnd);
  return true;
}

void anscheduler_socket_msg_async(socket_desc_t * socket,
                                  socket_msg_t * msg) {
  // allocate information
  if (!anscheduler_socket_reference(socket)) {
    anscheduler_free(msg);
    return;
  }
  
  msginfo_t * info = anscheduler_alloc(sizeof(msginfo_t));
  if (!info) {
    anscheduler_abort("failed to allocate msginfo_t");
  }
  
  info->message = msg;
  info->socket = socket;
  anscheduler_loop_push_kernel(info, (void (*)(void *))_async_msg);
}

socket_msg_t * anscheduler_socket_msg_data(const void * data, uint64_t len) {
  if (len >= 0xfe8) return NULL;
  socket_msg_t * msg = anscheduler_alloc(sizeof(socket_msg_t));
  if (!msg) return NULL;
  
  msg->type = ANSCHEDULER_MSG_TYPE_DATA;
  msg->len = len;
  const uint8_t * source = (const uint8_t *)data;
  int i;
  for (i = 0; i < len; i++) {
    msg->message[i] = source[i];
  }
  
  return msg;
}

socket_msg_t * anscheduler_socket_read(socket_desc_t * socket) {
  uint64_t * lock, * count;
  socket_msg_t ** first, ** last;
  if (dest->isConnector) {
    lock = &dest->socket->forConnectorLock;
    count = &dest->socket->forConnectorCount;
    first = &dest->socket->forConnectorFirst;
    last = &dest->socket->forConnectorLast;
  } else {
    lock = &dest->socket->forReceiverLock;
    count = &dest->socket->forReceiverCount;
    first = &dest->socket->forReceiverFirst;
    last = &dest->socket->forReceiverLast;
  }
  
  anscheduler_lock(lock);
  if ((*count) == 0) {
    anscheduler_unlock(lock);
    return NULL;
  }
  socket_msg_t * res = *first;
  if (!next->next) {
    (*first) = ((*last) = NULL);
  } else {
    (*first) = next->next;
  }
  (*count)--;
  res->next = NULL;
  anscheduler_unlock(lock);
  return res;
}

bool anscheduler_socket_connect(socket_desc_t * socket, task_t * task) {
  // generate another link
  socket_desc_t * link = _create_descriptor(socket->socket, task, false);
  if (!link) {
    return false;
  }
  
  anscheduler_task_dereference(task);
  anscheduler_socket_dereference(link);
  
  socket_msg_t * msg = anscheduler_alloc(sizeof(socket_msg_t));
  if (!msg) {
    anscheduler_abort("failed to allocate connect message");
  }
  msg->type = ANSCHEDULER_MSG_TYPE_CONNECT;
  msg->len = 0;
  
  // if we cannot send the message, we need to release our resources
  if (!anscheduler_socket_msg(socket, msg)) {
    anscheduler_free(msg);
    anscheduler_socket_dereference(socket);
  }
  return true;
}

void anscheduler_socket_close(socket_desc_t * socket, uint64_t code) {
  anscheduler_lock(&socket->closeLock);
  socket->isClosed = true;
  socket->closeCode = code;
  anscheduler_unlock(&socket->closeLock);
}

static socket_desc_t * _create_descriptor(socket_t * socket,
                                          task_t * task,
                                          bool isConnector) {
  socket_desc_t * desc = anscheduler_alloc(sizeof(socket_desc_t));
  if (!desc) return NULL;
  
  anscheduler_zero(desc, sizeof(socket_desc_t));
  desc->socket = socket;
  desc->task = task;
  desc->refCount = 1;
  desc->isConnector = isConnector;
  
  anscheduler_lock(&connRecLock);
  if (isConnector) {
    socket->connector = desc;
  } else {
    socket->receiver = desc;
  }
  anscheduler_unlock(&connRecLock);
  
  return desc;
}

static void _socket_hangup(socket_descriptor_t * socket) {
  anscheduler_cpu_lock();
  
  // generate the hangup message
  socket_t * sock = socket->socket;
  
  // release the socket descriptor from the socket
  socket_descriptor_t * otherEnd = NULL;
  bool shouldFree = false;
  anscheduler_lock(&sock->connRecLock);
  if (socket->isConnector) {
    otherEnd = socket->receiver;
  } else {
    otherEnd = socket->connector;
  }
  if (otherEnd) {
    otherEnd = anscheduler_socket_reference(otherEnd) ? otherEnd : NULL;
  }
  if (!otherEnd) {
    // hangup now
    if (socket->isConnector) socket->connector = NULL;
    else socket->receiver = NULL;
    shouldFree = socket->receiver == socket->connector;
  }
  anscheduler_unlock(&sock->connRecLock);
  
  if (!otherEnd) {
    anscheduler_free(socket);
    if (shouldFree) {
      anscheduler_free(sock);
    }
  } else {
    socket_msg_t * msg = anscheduler_alloc(sizeof(socket_msg_t));
    if (!msg) {
      anscheduler_abort("failed to allocate close message!");
    }
    msg->type = ANSCHEDULER_MSG_TYPE_CLOSE;
    msg->len = 8;
    (*((uint64_t *)msg->message)) = socket->closeCode;
    
    _push_message(otherEnd, msg);
    _wakeup_endpoint(otherEnd);
    
    // by this point, the other end may have freed up everything
    anscheduler_lock(&sock->connRecLock);
    if (socket->isConnector) {
      socket->connector = NULL;
    } else {
      socket->receiver = NULL;
    }
    shouldFree = socket->receiver == socket->connector;
    anscheduler_unlock(&sock->connRecLock);
    anscheduler_free(socket);
    if (shouldFree) {
      anscheduler_free(sock);
    }
  }
  
  anscheduler_loop_delete_cur_kernel();
}

static bool _push_message(socket_descriptor_t * dest, socket_msg_t * msg) {
  uint64_t * lock, * count;
  socket_msg_t ** first, ** last;
  if (dest->isConnector) {
    lock = &dest->socket->forConnectorLock;
    count = &dest->socket->forConnectorCount;
    first = &dest->socket->forConnectorFirst;
    last = &dest->socket->forConnectorLast;
  } else {
    lock = &dest->socket->forReceiverLock;
    count = &dest->socket->forReceiverCount;
    first = &dest->socket->forReceiverFirst;
    last = &dest->socket->forReceiverLast;
  }
  
  anscheduler_lock(lock);
  if (msg->type == ANSCHEDULER_MSG_TYPE_DATA) {
    if (*count >= ANSCHEDULER_SOCKET_MSG_MAX) {
      anscheduler_unlock(lock);
      return false;
    }
  }
  
  // add it to the linked list
  (*count)++;
  msg->next = NULL;
  if (!(*last)) {
    (*last) = ((*first) = msg);
  } else {
    (*last)->next = msg;
    (*last) = msg;
  }
  anscheduler_unlock(lock);
}

static void _wakeup_endpoint(socket_descriptor_t * dest) {
  if (!anscheduler_task_reference(dest->task)) {
    anscheduler_socket_dereference(dest);
    return;
  }
  
  task_t * task = dest->task;
  anscheduler_task_pending(task, dest);
  anscheduler_socket_dereference(dest);
  
  anscheduler_lock(&task->threadList);
  thread_t * thread = task->firstThread;
  while (thread) {
    if (__sync_fetch_and_and(&thread->isPolling, 0)) {
      anscheduler_unlock(&task->threadList);
      thread_t * curThread = anscheduler_cpu_get_thread();
      bool res = anscheduler_save_return_state(curThread);
      if (res) return;
      anscheduler_loop_switch(task, thread);
    }
    thread = thread->next;
  }
  anscheduler_unlock(&task->threadList);
  
  // no polling threads were found
  anscheduler_task_dereference(dest->task);
}

static void _async_msg(msginfo_t * _info) {
  anscheduler_cpu_lock();
  
  msginfo_t info = *_info;
  anscheduler_free(_info);
  if (!anscheduler_socket_msg(info.socket, info.message)) {
    anscheduler_free(info.message);
    anscheduler_socket_dereference(info.socket);
  }
  anscheduler_loop_delete_cur_kernel();
}
