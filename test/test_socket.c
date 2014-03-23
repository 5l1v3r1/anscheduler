/**
 * Test that the scheduler can run a single thread.
 */

#include "env/user_thread.h"
#include "env/threading.h"
#include "env/alloc.h"
#include "env/context.h"
#include <anscheduler/thread.h>
#include <anscheduler/task.h>
#include <anscheduler/loop.h>
#include <anscheduler/socket.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <assert.h>

void proc_enter(void * flag);
void create_thread(void (* method)());
void server_thread();
void client_closer_thread();
void client_closee_thread();
void client_keepalive_thread();
void * check_for_leaks(void * arg);

void thread_poll_syscall(void * unused);

int main() {
  // create one CPU
  // antest_launch_thread(NULL, proc_enter);
  antest_launch_thread((void *)1, proc_enter);
  
  while (1) {
    sleep(0xffffffff);
  }
  
  return 0;
}

void proc_enter(void * flag) {
  if (flag) {
    create_thread(server_thread);
    create_thread(client_closer_thread);
    //create_thread(client_closee_thread);
    //create_thread(client_keepalive_thread);
  }
  anscheduler_loop_run();
}

void create_thread(void (* method)()) {
  char useless[2];
  task_t * task = anscheduler_task_create(useless, 2);
  anscheduler_task_launch(task);
  
  thread_t * thread = anscheduler_thread_create(task);
  antest_configure_user_thread(thread, method);
  
  anscheduler_thread_add(task, thread);
  anscheduler_task_dereference(task);
}

void server_thread() {
  // here, listen for sockets
  anscheduler_cpu_lock();
  thread_t * thread = anscheduler_cpu_get_thread();
  anscheduler_cpu_unlock();
  while (1) {
    anscheduler_cpu_lock();
    bool res = anscheduler_save_return_state(thread);
    printf("got res %d\n", res);
    anscheduler_cpu_unlock();
    if (res) {
      // TODO: here, process incoming packets
      printf("got message!\n");
      continue;
    }
    anscheduler_cpu_lock();
    anscheduler_cpu_stack_run(NULL, thread_poll_syscall);
  }
}

void client_closer_thread() {
  // here, open a socket, send "I don't love you", and close it
  anscheduler_cpu_lock();
  socket_desc_t * desc = anscheduler_socket_new();
  uint64_t fd = desc->descriptor;
  task_t * target = anscheduler_task_for_pid(0);
  assert(target != NULL);
  assert(target != anscheduler_cpu_get_task());
  bool result = anscheduler_socket_connect(desc, target);
  assert(result);
  
  // this is a new critical section
  desc = anscheduler_socket_for_descriptor(fd);
  assert(desc != NULL);
  
  // TODO: send message here
  
  anscheduler_task_exit(0);
}

void client_closee_thread() {
  // here, open a socket, send "you wanna go on a date?", and wait for close
  anscheduler_cpu_lock();
  anscheduler_task_exit(0);
}

void client_keepalive_thread() {
  // here, open a socket, send "marry me", and wait for it to close
  anscheduler_cpu_lock();
  anscheduler_task_exit(0);
}

void * check_for_leaks(void * arg) {
  sleep(1);
  // one PID pool + 2 CPU stacks = 3 pages!
  if (antest_pages_alloced() != 3) {
    fprintf(stderr, "leaked 0x%llx pages\n", antest_pages_alloced() - 3);
    exit(1);
  }
  exit(0);
  return NULL;
}

void thread_poll_syscall(void * unused) {
  task_t * task = anscheduler_cpu_get_task();
  if (!anscheduler_thread_poll()) {
    anscheduler_thread_run(task, anscheduler_cpu_get_thread());
  } else {
    anscheduler_cpu_set_task(NULL);
    anscheduler_cpu_set_thread(NULL);
    anscheduler_task_dereference(task);
    anscheduler_loop_run();
  }
}
