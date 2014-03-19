#ifndef __ANSCHEDULER_TYPES_H__
#define __ANSCHEDULER_TYPES_H__

#include <anscheduler_consts.h> // should declare int types, etc.
#include <anscheduler_state.h> // you must create this
#include <anidxset.h>

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
  
  // list of open sockets
  uint64_t socketsLock;
  socket_link_t * firstSocket;
  
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
} __attribute__((packed));

struct thread_t {
  thread_t * next, * last;
  thread_t * queueNext, * queueLast;
  task_t * task;
  
  uint64_t nextTimestamp;
  uint64_t stack;
  
  uint64_t interestsLock; // applies to the following 3 flags
  uint64_t interests; // (1 << n) masked for each IRQ# and 2^63 for socket
  uint64_t pending; // interests pending
  uint64_t isPolling; // 1 if waiting for an interest
  
  anscheduler_state state;
} __attribute__((packed));

struct socket_t {
  task_t * connector, * receiver;
  
  uint64_t msgsLock;
  socket_msg_t * firstMsg;
} __attribute__((packed));

struct socket_link_t {
  socket_link_t * next, * last;
  socket_t * socket;
  uint64_t descriptor;
} __attribute__((packed));

struct socket_msg_t {
  socket_msg_t * next;
  uint64_t type;
  uint64_t length;
  // data array after this point in the object (0x18 bytes in)
  
  // message types:
  // 0 = connect
  // 1 = data
  // 2 = closed by remote
  // 3 = remote killed by external task
  // 4 = remote exit
  // 5 = remote killed because of memory fault
} __attribute__((packed));

#endif
