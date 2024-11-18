/**
 * @file
 *
 * Interface for the platform-specific parts of the network sub-system.
 */

#ifndef NETWORK_PLATFORM_H
#define NETWORK_PLATFORM_H

#include "definitions.h"
#include "network/connection.h"
#include "network/common_types.h"

#ifdef MC_PLATFORM_LINUX
#incude <arpa/inet.h>
#define SOCKFD_INVALID (-1)
#elif defined MC_PLATFORM_WINDOWS
#include <winsock2.h>
#include <ws2tcpip.h>
#define SOCKFD_INVALID INVALID_SOCKET
#else
#error Not implemented for this platform yet!
#endif

#define SOCKIO_PENDING -1
#define SOCKIO_ERROR -2

typedef uintptr_t socketfd;

typedef struct {
    union {
        u16 family;
        struct sockaddr sa;
        struct sockaddr_in ip4;
        struct sockaddr_in6 ip6;
        struct sockaddr_storage storage;
    } data;
    u64 length;
} SocketAddress;

void* network_handle(void* params);

i32 network_platform_init(NetworkContext* ctx);

void close_connection(NetworkContext* ctx, Connection* conn);

i32 accept_connection(NetworkContext* ctx);
i32 create_server_socket(NetworkContext* ctx, char* host, i32 port);

enum IOCode read_bytes(Connection* conn);
enum IOCode read_varint(Connection* conn);

enum IOCode try_send(socketfd socket, void* data, u64 size, u64* out_sent);



void sockaddr_init(SocketAddress* addr, u16 family);
bool sockaddr_parse(SocketAddress* addr, char* host, i32 port);
bool sock_is_valid(socketfd socket);
i64 sock_recv(socketfd socket, void* buf, u64 buf_len);
i64 sock_recv_buf(socketfd socket, ByteBuffer* output);

i64 sock_send(socketfd socket, const void* buf, u64 buf_len);
i64 sock_send_buf(socketfd socket, ByteBuffer* input);

socketfd sock_create(int address_family, int type, int protocol);
bool sock_bind(socketfd socket, SocketAddress* address);
bool sock_listen(socketfd socket, i32 backlog);
socketfd sock_accept(socketfd socket);

#endif /* ! NETWORK_PLATFORM_H */
