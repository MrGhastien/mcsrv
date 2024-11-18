//
// Created by bmorino on 14/11/2024.
//

#include <logger.h>
#include <stdio.h>
#ifdef MC_PLATFORM_WINDOWS

#include "platform/network.h"
#include "utils/bitwise.h"
#include "platform/mc_thread.h"

#include "network/common_functions.h"

struct IOEvent {
    i32 events;
    Connection* connection;
};
enum IOCode try_send(socketfd socket, void* data, u64 size, u64* out_sent) {
    u64 sent = 0;
    enum IOCode code = IOC_OK;
    while (sent < size) {
        void* begin = offset(data, sent);
        i64 res =  sock_send(socket, begin, size - sent);

        if(res == SOCKIO_PENDING)
            return IOC_AGAIN;

        if (res == SOCKIO_ERROR)
            return IOC_ERROR;

        if(res == 0)
            return IOC_CLOSED;

        sent += res;
    }
    *out_sent = sent;
    return code;
}

i32 network_platform_init(NetworkContext* ctx);

void close_connection(NetworkContext* ctx, Connection* conn);

i32 accept_connection(NetworkContext* ctx);
i32 create_server_socket(NetworkContext* ctx, char* host, i32 port) {
    socketfd server_socket = WSASocketA(AF_INET, SOCK_STREAM, IPPROTO_TCP, 0, 0, 0);
    if(!sock_is_valid(server_socket))

    ctx->server_socket = server_socket;
}

void* network_handle(void* params) {
    NetworkContext* ctx = params;

    mcthread_set_name("network");

    HANDLE completion_port = CreateIoCompletionPort(ctx->server_socket, NULL, 0ull, 1);

    while(ctx->should_continue) {
        unsigned long bytes_transferred;
        u64 key;
        WSAOVERLAPPED* overlapped;
        if(!GetQueuedCompletionStatus(completion_port, &bytes_transferred, &key, &overlapped, INFINITE)) {
            ctx->should_continue = FALSE;
            break;
        }

        struct IOEvent* event = (struct IOEvent*) key;
        ctx->code = handle_connection_io(event->connection, event->events);
    }

    return NULL;
}



static WSAOVERLAPPED overlapped = {};

void sockaddr_init(SocketAddress* addr, u16 family) {
    memset(&addr->data, 0, sizeof(addr->data));
    addr->data.family = family;
    switch (family) {
           case AF_INET:
               addr->length = sizeof addr->data.ip4;
               break;
           case AF_INET6:
               addr->length = sizeof addr->data.ip6;
               break;
           default:
               addr->length = sizeof addr->data.sa;
               break;
    }
}
bool sockaddr_parse(SocketAddress* addr, char* host, i32 port) {
    addr->length = sizeof(addr->data.ip4);
    char port_str[32];
    snprintf(port_str, 32, "%i", port);
    struct addrinfo* info;
    i32 res = getaddrinfo(host, port_str, NULL, &info);
    if(res != 0) {
        log_error("Could not parse server address.");
        return FALSE;
    }
    return TRUE;
}

bool sock_is_valid(socketfd socket) {
    return socket != INVALID_SOCKET;
}

i64 sock_recv(socketfd socket, void* buf, u64 buf_len) {
    WSABUF wsabuf = {
        .len = buf_len,
        .buf = buf,
    };
    unsigned long bytes;
    i32 res = WSARecv(socket, &wsabuf, 1l, &bytes, 0, &overlapped, NULL);

    if(res != 0) {
        i32 error = WSAGetLastError();
        return error == WSA_IO_PENDING ? SOCKIO_PENDING : SOCKIO_ERROR;
    }

    return bytes;
}
i64 sock_recv_buf(socketfd socket, ByteBuffer* output) {
    WSABUF regions[2];
    u64 size = 0;
    u64 total_size = 0;
    i32 region_count = 0;
    while(size < output->capacity - output->size) {
        void* cast_ptr = &regions[region_count].buf;
        size = bytebuf_contiguous_write(output, cast_ptr);
        regions[region_count].len = size;
        total_size += size;
        region_count++;
    }

    if(region_count == 0)
        return 0;

    unsigned long bytes = 0;
    i32 res = WSARecv(socket, regions, region_count, &bytes, 0, &overlapped, NULL);
    if(res != 0) {
        i32 error = WSAGetLastError();
        if(error == WSA_IO_PENDING)
            return SOCKIO_PENDING;
        bytebuf_unwrite(output, size);
        return SOCKIO_ERROR;
    }

    bytebuf_unwrite(output, size - bytes);
    return bytes;
}

i64 sock_send(socketfd socket, const void* buf, u64 buf_len) {
    WSABUF wsabuf = {
        .len = buf_len,
        .buf = buf,
    };
    unsigned long bytes = 0;
    i32 res = WSASend(socket, &wsabuf, 1l, &bytes, 0, &overlapped, NULL);
    if(res != 0) {
        i32 error = WSAGetLastError();
        return error == WSA_IO_PENDING ? SOCKIO_PENDING : SOCKIO_ERROR;
    }

    return bytes;
}
i64 sock_send_buf(socketfd socket, ByteBuffer* input) {
    WSABUF regions[2];
    u64 size = 0;
    u64 total_size = 0;
    i32 region_count = 0;
    while(size < input->size) {
        void* cast_ptr = &regions[region_count].buf;
        size = bytebuf_contiguous_read(input, cast_ptr);
        regions[region_count].len = size;
        total_size += size;
        region_count++;
    }

    if(region_count == 0)
        return 0;

    unsigned long bytes = 0;
    i32 res = WSASend(socket, regions, region_count, &bytes, 0, &overlapped, NULL);
    if(res != 0) {
        i32 error = WSAGetLastError();
        if(error == WSA_IO_PENDING)
            return SOCKIO_PENDING;
        bytebuf_unread(input, total_size);
        return SOCKIO_ERROR;
    }

    bytebuf_unread(input, size - bytes);
    return bytes;
}

socketfd sock_create(int address_family, int type, int protocol) {
    return WSASocketA(address_family, type, protocol, 0, 0, 0);
}
bool sock_bind(socketfd socket, SocketAddress* address) {
    return bind(socket, &address->data.sa, address->length) == 0;
}
bool sock_listen(socketfd socket, i32 backlog) {
    return listen(socket, backlog) == 0;
}
socketfd sock_accept(socketfd socket, SocketAddress* out_address) {
    STATIC_ASSERT(sizeof(out_address->data.family) == sizeof(u16), "Address family member in sockaddr is not a short int.");
    u8 out_buf[1024];
    unsigned long received;
    SocketAddress local_addr;
    sockaddr_init(&local_addr, out_address->data.family);

    socketfd accepted = WSASocketA(AF_INET, SOCK_STREAM, 0, 0, 0, 0);
    bool success = AcceptEx(socket,
        accepted,
        out_buf,
        0,
        local_addr.length + 16,
        out_address->length + 16,
        &received,
        &overlapped
        );

    if(!success) {
        closesocket(accepted);
        return INVALID_SOCKET;
    }

    struct sockaddr* local;
    i32 local_len;
    struct sockaddr* peer;
    i32 peer_len;

    GetAcceptExSockaddrs(out_buf,
        0,
        local_addr.length + 16,
        out_address->length + 16,
        &local,
        &local_len,
        &peer,
        &peer_len
        );

    out_address->length = peer_len;
    memcpy(&out_address->data.storage, peer, peer_len);

    return accepted;
}


#endif