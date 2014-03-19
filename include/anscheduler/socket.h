#ifndef __ANSCHEDULER_SOCKET_H__
#define __ANSCHEDULER_SOCKET_H__

#include "types.h"

/**
 * Creates a new socket and assigns it to the current task.
 * @return The socket link in the task's sockets linked list. NULL if any part
 * of socket allocation failed.
 * @critical
 */
socket_link_t * anscheduler_socket_new();

/**
 * Send a message to the socket by copying the data pointed to by `data`.
 * @critical
 */
void anscheduler_socket_msg(socket_t * socket, void * data, uint64_t len);

/**
 * Returns the next message on the queue, or NULL if no messages are pending.
 * @critical
 */
socket_msg_t * anscheduler_socket_read(socket_t * socket);

/**
 * Connects a socket to a different task.
 * @param task A referenced task.
 * @critical This function connects to the task instantly, so when it returns
 * the connection has been made and (possibly) the task has been notified of
 * the connection.
 */
void anscheduler_socket_connect(socket_t * socket, task_t * task);

/**
 * Closes a socket from the current task.  This may not cause the underlying
 * socket to be terminated, but it *will* notify all connected endpoints
 * of the close event.
 * @critical Note that if there are a lot of socket endpoints, they will
 * all be notified, and since this runs in a critical section that may result
 * in an uncomfortable pause; thus, it is recommended that sockets be limited
 * to two endpoints, but that is at your discression.
 */
void anscheduler_socket_close(socket_link_t * socket);

#endif
