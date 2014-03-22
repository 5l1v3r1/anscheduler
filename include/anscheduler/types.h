#ifndef __ANSCHEDULER_TYPES_H__
#define __ANSCHEDULER_TYPES_H__

#include <anscheduler_consts.h> // should declare int types, etc.
#include <anscheduler_state.h> // you must create this
#include <anidxset.h>

#define ANSCHEDULER_MSG_TYPE_CONNECT 0
#define ANSCHEDULER_MSG_TYPE_DATA 1

#define ANSCHEDULER_MAX_MSG_BUFFER 0x8

typedef struct task_t task_t;
typedef struct thread_t thread_t;
typedef struct socket_t socket_t;
typedef struct socket_link_t socket_link_t;
typedef struct socket_msg_t socket_msg_t;

struct task_t {
  task_t * next, * last;
  
  uint64_t pid;
  uint64_t uid;
  
  // when this task exits, *codeRetainCount-- is performed. If the new
  // value is 0, then the code data of this task is deallocated and so is
  // the pointer to codeRetainCount.
  uint64_t * codeRetainCount;
  
  // virtual memory structure
  uint64_t vmLock;
  void * vm;
  
  // hash map of open sockets
  uint64_t socketsLock;
  socket_link_t * sockets[0x10];
  
  // list of sockets with pending messages
  uint64_t pendingLock;
  socket_link_t * firstPending;
  
  // list of threads in this task
  uint64_t threadsLock;
  thread_t * firstThread;
  
  // the stack indexes (for layout in virtual memory)
  uint64_t stacksLock;
  anidxset_root_t stacks;
  
  // the index set for allocating socket descriptors
  uint64_t descriptorsLock;
  anidxset_root_t descriptors;
  
  uint64_t killLock;
  uint64_t refCount; // when this reaches 0 and isKilled = 1, kill this task
  uint64_t isKilled; // 0 or 1, starts at 0
  uint64_t killReason;
} __attribute__((packed));

struct thread_t {
  thread_t * next, * last;
  thread_t * queueNext, * queueLast;
  task_t * task;
  
  uint64_t nextTimestamp;
  uint64_t stack;
  
  bool isPolling; // 1 if waiting for a message (or an interrupt)
  char reserved[3]; // for alignment
  uint32_t irqs; // if this is the interrupt thread, these will be masked
  
  anscheduler_state state;
} __attribute__((packed));

/**
 * An internal data structure which stores a message queue and points to the
 * two socket endpoints, the connector and the receiver.
 */
struct socket_t {
  uint64_t connRecLock; // locks the two fields that follow
  socket_link_t * connector, * receiver;
  
  uint64_t msgsLock; // locks the next 6 fields
  uint64_t forConnectorCount;
  socket_msg_t * forConnectorFirst, * forConnectorLast;
  uint64_t forReceiverCount;
  socket_msg_t * forReceiverFirst, * forReceiverLast;
} __attribute__((packed));

struct socket_link_t {
  socket_link_t * next, * last; // in the task
  socket_link_t * pendingNext, * pendingLast; // in the task's pending list
  
  socket_t * socket; // underlying socket
  uint64_t descriptor; // task specific fd
  
  uint64_t isConnector; // true = connector, false = receiver
  task_t * task; // task which owns the socket link
  
  uint64_t closeLock; // applies to next three fields
  uint64_t isClosed; // true when close() has been requested
  uint64_t refCount; // when 0 and isClosed = true, shutdown
  uint64_t closeCode; // status code for close message
} __attribute__((packed));

struct socket_msg_t {
  socket_msg_t * next, * last;
  uint64_t type;
  uint64_t len;
  char message[0xfe8]; // 0x1000 - 0x18
  
  // message types:
  // 0 = connect
  // 1 = data
  // 2 = closed by remote
  // 3 = remote killed by external task
  // 4 = remote exit
  // 5 = remote killed because of memory fault
} __attribute__((packed));

#endif
