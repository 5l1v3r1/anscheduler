#include <anscheduler_consts.h> // should declare int types, etc.
#include <anscheduler_state.h> // you must create this
#include <anidxset.h>

typedef struct task_t task_t;
typedef struct thread_t thread_t;
typedef struct socket_t socket_t;
typedef struct socket_link_t socket_link_t;

struct task_t {
  task_t * next, * last;
  
  uint64_t pid;
  uint64_t uid;
  
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
} __attribute__((packed));

struct thread_t {
  thread_t * next, * last;
  task_t * task;
  uint64_t nextTimestamp;
  uint64_t stack;
  
  uint64_t interestsLock; // applies to the following 3 flags
  uint64_t interests; // (1 << n) masked for each IRQ# and 2^63 for socket
  uint64_t pending; // a flag is set whenever an external notification comes in
  uint64_t isActive; // 0 if waiting for an interest
  
  anscheduler_state state;
} __attribute__((packed));

struct socket_t {
  uint64_t lock;
  uint64_t retainCount;
  socket_link_t * firstLink;
} __attribute__((packed));

struct socket_link_t {
  socket_link_t * next, * last;
  socket_t * socket;
  uint64_t descriptor; // unused for socket_t's reference
} __attribute__((packed));
