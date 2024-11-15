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
// TODO add linux-specific includes
#elif defined MC_PLATFORM_WINDOWS
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#error Not implemented for this platform yet!
#endif

#define SOCKIO_PENDING -1
#define SOCKIO_ERROR -2

typedef uintptr_t Socket;

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

void* network_handle(NetworkContext* ctx, void* params);

i32 network_platform_init(NetworkContext* ctx);

void close_connection(NetworkContext* ctx, Connection* conn);

i32 accept_connection(NetworkContext* ctx);
i32 create_server_socket(NetworkContext* ctx, char* host, i32 port);

enum IOCode read_bytes(Connection* conn);
enum IOCode read_varint(Connection* conn);

enum IOCode try_send(Socket socket, void* data, u64 size, u64* out_sent);



void sockaddr_init(SocketAddress* addr, u16 family);
bool sock_is_valid(Socket socket);
i64 sock_recv(Socket socket, void* buf, u64 buf_len);
i64 sock_recv_buf(Socket socket, ByteBuffer* output);

i64 sock_send(Socket socket, const void* buf, u64 buf_len);
i64 sock_send_buf(Socket socket, ByteBuffer* input);

bool sock_bind(Socket socket, SocketAddress* address);
bool sock_listen(Socket socket, i32 backlog);
Socket sock_accept(Socket socket);

#endif /* ! NETWORK_PLATFORM_H */
