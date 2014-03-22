#include "ipc_shutdown.h"
#include <anscheduler/socket.h>
#include <anscheduler/functions.h>

static uint8_t _descriptor_hash(uint64_t desc);

/**
 * Remove the link `link` from its task's descriptor hashmap.
 * @critical
 */
static void _socket_remove(socket_link_t * link);

/**
 * Add the link `link` to its task's descriptor hashmap.
 * @critical
 */
static void _socket_add(socket_link_t * link);

/**
 * References the remote task and socket link of a link. If this returns
 * NULL, then something about the other end is being shutdown--either the
 * socket, or the task itself.
 * @critical
 */
static socket_link_t * _find_other_end(socket_link_t * link);

socket_link_t * anscheduler_link_create(socket_t * socket,
                                        task_t * task,
                                        bool isConnector) {
  socket_link_t * link = anscheduler_alloc(sizeof(socket_link_t));
  if (!link) return NULL;
  
  anscheduler_zero(link, sizeof(socket_link_t));

  link->socket = socket;
  link->isConnector = isConnector;
  anscheduler_lock(&socket->connRecLock);
  if (isConnector) socket->connector = link;
  else socket->receiver = link;
  anscheduler_unlock(&socket->connRecLock);

  anscheduler_lock(&task->descriptorsLock);
  uint64_t desc = anidxset_get(&task->descriptors);
  anscheduler_unlock(&task->descriptorsLock);
  link->descriptor = desc;
  link->task = task;
  link->refCount = 1;

  _socket_add(link);
  return link;
}

bool anscheduler_socket_dereference_nowakeup(socket_link_t * link,
                                             bool * otherEnd) {
  anscheduler_lock(&socket->closeLock);
  if (socket->isClosed && socket->refCount == 0) {
    anscheduler_unlock(&socket->closeLock);
    return false;
  }
  socket->refCount--;
  if (!socket->refCount && socket->isClosed) {
    anscheduler_unlock(&socket->closeLock);
    
    // destroy the socket
    _socket_remove(socket);
    anscheduler_lock(&link->socket->connRecLock);
    if (link->isConnector) {
      link->socket->connector = NULL;
      (*otherEnd) = link->socket->receiver != NULL;
    } else {
      link->socket->receiver = NULL;
      (*otherEnd) = link->socket->connector != NULL;
    }
    anscheduler_unlock(&link->socket->connRecLock);
  
    anscheduler_lock(&link->task->pendingLock);
    _task_remove_pending(link);
    anscheduler_unlock(&link->task->pendingLock);
    return true;
  }
  anscheduler_unlock(&socket->closeLock);
  return false;
}

socket_msg_t * anscheduler_shutdown_message(socket_link_t * link) {
  socket_msg_t * msg = anscheduler_alloc(sizeof(socket_msg_t));
  if (!msg) anscheduler_abort("Failed to allocate closed message");
  msg->type = link->closeCode;
  msg->len = 8;
  (*((uint64_t *)msg->message)) = link->closeCode;
  return msg;
}

void anscheduler_complete_shutdown(socket_link_t * link, bool otherEnd) {
  if (otherEnd) {
    socket_msg_t * msg = anscheduler_shutdown_message(link);
    if (!anscheduler_socket_msg(link, msg)) {
      anscheduler_free(msg);
    }
  } else {
    anscheduler_free(link->socket);
    anscheduler_free(link);
  }
}

void anscheduler_link_wakeup(socket_link_t * thisEnd) {
  socket_link_t * otherLink = _find_other_end(link);
  anscheduler_socket_dereference(link);
  if (!otherEnd) return;
}

static uint8_t _descriptor_hash(uint64_t desc) {
  return (uint8_t)(desc & 0xf);
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
