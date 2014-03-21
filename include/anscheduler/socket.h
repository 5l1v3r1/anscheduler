#ifndef __ANSCHEDULER_SOCKET_H__
#define __ANSCHEDULER_SOCKET_H__

#include "types.h"

/**
 * Creates a new socket and assigns it to the current task.
 * @return The socket link in the task's sockets linked list. NULL if any part
 * of socket allocation failed. This socket will be referenced.
 * @critical
 */
socket_link_t * anscheduler_socket_new();

/**
 * Finds the socket for a socket descriptor in the current task. The returned
 * socket will be referenced.
 * @critical
 */
socket_link_t * anscheduler_socket_for_descriptor(uint64_t desc);

/**
 * Reference a socket.
 * @critical
 */
bool anscheduler_socket_reference(socket_link_t * socket);

/**
 * Dereference a socket.
 * @critical
 */
void anscheduler_socket_dereference(socket_link_t * socket);

/**
 * Send a message to the socket.
 * @param socket A referenced socket link.
 * @param msg The message to push to the socket.
 * @return true when the message was sent, false when the buffer was full.
 * When false is returned, you are responsible for freeing the message.
 * @critical
 */
bool anscheduler_socket_msg(socket_link_t * socket, socket_msg_t * msg);

/**
 * Allocates a socket message with specified data. Maximum length for the
 * data is 0xfe8 bytes. May return NULL if the message could not be allocated.
 * @critical
 */
socket_msg_t * anscheduler_socket_msg_data(void * data, uint64_t len);

/**
 * Returns the next message on the queue, or NULL if no messages are pending.
 * @param socket A referenced socket link.
 * @critical
 */
socket_msg_t * anscheduler_socket_read(socket_link_t * socket);

/**
 * Connects a socket to a different task.
 * @param socket A referenced socket link.
 * @param task A referenced task.
 * @critical This function connects to the task instantly, so when it returns
 * the connection has been made and (possibly) the task has been notified of
 * the connection.
 */
void anscheduler_socket_connect(socket_link_t * socket, task_t * task);

/**
 * Closes a socket from the current task.  This may not cause the underlying
 * socket to be terminated, but it *will* notify all connected endpoints of
 * the disconnect.
 * @param socket A referenced socket link. Once this is dereferenced, the
 * socket close notification will go through, and the socket may potentially
 * be freed all together.
 * @critical
 */
void anscheduler_socket_close(socket_link_t * socket, uint64_t code);

#endif
