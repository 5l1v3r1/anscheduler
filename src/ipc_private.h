#include <anscheduler/types.h>

/**
 * Creates a socket link for a socket and adds it to a task. The socket link
 * is returned. It is your responsibility to send a connect message to the
 * link if one is needed.
 * @critical
 */
socket_link_t * anscheduler_link_create(socket_t * socket,
                                        task_t * task,
                                        bool isConnector);

/**
 * Dereferences a socket.  If the socket is closed, true will be returned,
 * otherwise false. If the socket is closed, it is your responsibility to send
 * any shutdown message you want. If a shutdown occurs, the link is removed
 * from the sockets list, but no message is sent to the other end.
 *
 * @param link A referenced link.
 * @param otherEnd If true is returned, this will be set to true if a close
 * message should be generated and sent. If this is false, then the
 * underlying socket should be closed.
 * @return true if closed, false if still open
 * @critical
 */
bool anscheduler_socket_dereference_nowakeup(socket_link_t * link,
                                             bool * otherEnd);

/**
 * Creates the shutdown message which should be sent after a link `link` has
 * been closed.
 * @critical
 */
socket_msg_t * anscheduler_shutdown_message(socket_link_t * link);

/**
 * Call this is a link has been shutdown.
 * @critical -> @noncritical -> @critical
 */
void anscheduler_complete_shutdown(socket_link_t * link, bool otherEnd);

/**
 * Wakeup the other end of a socket link. The link may be closed, since this
 * method may be useful to send a closed message.
 * @critical -> @noncritical -> @critical
 */
void anscheduler_link_wakeup(socket_link_t * thisEnd);
