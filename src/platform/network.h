/**
 * @file
 *
 * Interface for the platform-specific parts of the network sub-system.
 */

#ifndef NETWORK_PLATFORM_H
#define NETWORK_PLATFORM_H

#include "definitions.h"
#include "network/common_types.h"

#define SOCKIO_PENDING -1
#define SOCKIO_ERROR -2

void* network_handle(void* params);

i32 network_platform_init(NetworkContext* ctx, u64 max_connections);
i32 network_platform_init_socket(socketfd server_socket);
void platform_network_finish(void);
void platform_network_stop(void);

void close_connection(NetworkContext* ctx, Connection* conn);

i32 create_server_socket(NetworkContext* ctx, char* host, i32 port);

enum IOCode fill_buffer(Connection* conn);
enum IOCode empty_buffer(Connection* conn);

/**
 * Tries sending a buffer of data.
 *
 * @param socket The file descriptor of the socket to send data through.
 * @param[in] data The buffer containing the data to send.
 * @param size The number of bytes in the input buffer.
 * @param[out] out_sent A pointer to a @ref u64, that will contain the number
 *             of bytes sent.
 * @return
 *         - @ref IOC_OK if all input data has been sent,
 *         - @ref IOC_AGAIN if not all bytes could be sent without waiting,
 *         - @ref IOC_ERROR if a socket error occurred,
 *         - @ref IOC_CLOSED if the connection was closed while sending data.
 */
enum IOCode try_send(socketfd socket, void* data, u64 size, u64* out_sent);

#endif /* ! NETWORK_PLATFORM_H */
