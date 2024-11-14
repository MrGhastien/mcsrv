//
// Created by bmorino on 14/11/2024.
//

#ifndef NETWORK_PLATFORM_H
#define NETWORK_PLATFORM_H

#include "definitions.h"
#include "network/connection.h"


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
        struct sockaddr sa;
        struct sockaddr_in ip4;
        struct sockaddr_in6 ip6;
        struct sockaddr_storage storage;
    } data;
    u64 length;
} SocketAddress;

typedef struct NetworkContext {
} NetworkContext;

void* network_handle(NetworkContext* ctx, void* params);

void close_connection(NetworkContext* ctx, Connection* conn);

i32 accept_connection(NetworkContext* ctx);
i32 create_server_socket(NetworkContext* ctx, char* host, i32 port);



bool sock_is_valid(Socket socket);
i64 sock_recv(Socket socket, void* buf, u64 buf_len);
i64 sock_recv_buf(Socket socket, ByteBuffer* output);

i64 sock_send(Socket socket, const void* buf, u64 buf_len);
i64 sock_send_buf(Socket socket, ByteBuffer* input);

bool sock_bind(Socket socket, SocketAddress* address);
bool sock_listen(Socket socket, i32 backlog);
Socket sock_accept(Socket socket);

#endif /* ! NETWORK_PLATFORM_H */
