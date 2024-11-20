//
// Created by bmorino on 20/11/2024.
//

#ifndef SOCKET_H
#define SOCKET_H

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

typedef uintptr_t socketfd;

typedef struct SocketAddress {
    union {
        u16 family;
        struct sockaddr sa;
        struct sockaddr_in ip4;
        struct sockaddr_in6 ip6;
        struct sockaddr_storage storage;
    } data;
    u64 length;
} SocketAddress;

void sockaddr_init(SocketAddress* addr, u16 family);
bool sockaddr_parse(SocketAddress* addr, char* host, i32 port);
string sockaddr_to_string(SocketAddress* addr, Arena* arena, u32* out_port);

bool sock_is_valid(socketfd socket);
// i64 sock_recv(socketfd socket, void* buf, u64 buf_len);
// i64 sock_recv_buf(socketfd socket, ByteBuffer* output);
//
// i64 sock_send(socketfd socket, const void* buf, u64 buf_len);
// i64 sock_send_buf(socketfd socket, ByteBuffer* input);

socketfd sock_create(int address_family, int type, int protocol);
bool sock_bind(socketfd socket, const SocketAddress* address);
bool sock_listen(socketfd socket, i32 backlog);
enum IOCode sock_accept(socketfd socket, socketfd* out_accepted, SocketAddress* out_address);
bool sock_get_peer_address(socketfd socket, SocketAddress* out_address);

void sock_close(socketfd socket);

#endif /* ! SOCKET_H */
