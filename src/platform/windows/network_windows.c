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

#define COMPL_KEY_ACCEPT 0
#define COMPL_KEY_STOP 1

struct IOEvent {
    i32 events;
    Connection* connection;
};

typedef struct PlatformNetworkCtx {
    HANDLE completion_port;
} PlatformNetworkCtx;



static PlatformNetworkCtx platform_ctx;
static WSAOVERLAPPED overlapped = {};



char* get_last_error(void) {
    static char error_msg[256];
    i32 error_code = WSAGetLastError();
    FormatMessage(FORMAT_MESSAGE_IGNORE_INSERTS, NULL, error_code, 0, error_msg, 256, NULL);
    return error_msg;
}

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

i32 network_platform_init(NetworkContext* ctx) {
    WSADATA wsa_data;
    i32 res = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if(res != 0) {
        log_fatalf("Failed to initialize Winsock 2.2: %s", get_last_error());
        return 1;
    }

    platform_ctx.completion_port = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0ull, 1);

    return 0;
}
i32 platform_socket_init(socketfd server_socket) {
    if(CreateIoCompletionPort((HANDLE)server_socket, platform_ctx.completion_port, COMPL_KEY_ACCEPT, 1) == INVALID_HANDLE_VALUE) {
        log_fatalf("Failed to create the server socket: %s", get_last_error());
        return 6;
    }
    return 0;
}
i32 platform_accept_connection(socketfd peer_socket, Connection* connection) {

    if(CreateIoCompletionPort((HANDLE)peer_socket, platform_ctx.completion_port, (uintptr_t)connection, 1) != 0) {
        log_errorf("Could not handle the connection to [%s:%u]: %s", connection->peer_addr.base, connection->peer_port, get_last_error());
        return 3;
    }

    sock_recv_buf(peer_socket, &connection->recv_buffer);
    return 0;
}

i32 accept_connection(NetworkContext* ctx) {

    SocketAddress peer_address;

    socketfd peer_socket = sock_accept(ctx->server_socket, &peer_address);
    if(!sock_is_valid(peer_socket)) {
        log_errorf("Failed to establish connection to peer: %s", get_last_error());
        return 1;
    }

    Arena arena = ctx->arena;

    u32 peer_port;
    string peer_host = sockaddr_to_string(&peer_address, &arena, &peer_port);

    i64 index;
    Connection* conn = objpool_add(&ctx->connections, &index);
    if(!conn) {
        sock_close(peer_socket);
        log_warn("Reached maximum connection amount, rejecting.");
        return 2;
    }

    *conn = conn_create(peer_socket, index, &ctx->enc_ctx, peer_host, peer_port);

    if(platform_accept_connection(peer_socket, conn) != 0) {
        sock_close(peer_socket);
        return 3;
    }

    log_infof("Accepted connection from [%s:%i].", peer_host.base, peer_port);
    return 0;
}
void close_connection(NetworkContext* ctx, Connection* conn) {
    log_infof("Closing connection to [%s:%i].", conn->peer_addr.base, conn->peer_port);
    sock_close(conn->peer_socket);

    arena_destroy(&conn->scratch_arena);
    arena_destroy(&conn->persistent_arena);
    mcmutex_destroy(&conn->mutex);
    conn->peer_socket = SOCKFD_INVALID;
    objpool_remove(&ctx->connections, conn->table_index);
}

i32 handle_connection_io(Connection* conn) {

    // Resume processing the data

    return 0;
}

void* network_handle(void* params) {
    NetworkContext* ctx = params;

    mcthread_set_name("network");

    while(ctx->should_continue) {
        unsigned long bytes_transferred;
        u64 key;
        WSAOVERLAPPED* overlapped;
        bool res = GetQueuedCompletionStatus(platform_ctx.completion_port, &bytes_transferred, &key, &overlapped, INFINITE);
        if(res) {
            if(key == COMPL_KEY_ACCEPT) {
                // the server socket has received a connection
                accept_connection(ctx);
            } else if(key == COMPL_KEY_STOP) {
                // Stop!
                ctx->should_continue = FALSE;
                break;
            } else {
                // all good.
                Connection* conn = (Connection*) key;
                ctx->code = handle_connection_io(conn);
            }



        } else if(overlapped) {
            // A pending IO operation failed.
        } else {
            // GetQueuedCompletionStatus failed.
            ctx->should_continue = FALSE;
            break;
        }

    }

    return NULL;
}

void platform_network_stop(void) {
    if(!PostQueuedCompletionStatus(platform_ctx.completion_port, 0, COMPL_KEY_STOP, &overlapped)) {
        log_errorf("Failed to stop the network thread : %s", get_last_error());
    }
}




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
    char port_str[32];
    snprintf(port_str, 32, "%i", port);
    struct addrinfo* info;
    i32 res = getaddrinfo(host, port_str, NULL, &info);
    if(res != 0) {
        log_error("Could not parse server address.");
        return FALSE;
    }

    memcpy(&addr->data.storage, info->ai_addr, info->ai_addrlen);
    addr->length = info->ai_addrlen;

    return TRUE;
}
string sockaddr_to_string(SocketAddress* addr, Arena* arena, u32* out_port) {
    char host[NI_MAXHOST];
    char serv[NI_MAXSERV];
    if(getnameinfo(&addr->data.sa, addr->length, host, NI_MAXHOST, serv, NI_MAXSERV, 0) != 0) {
        log_errorf("Could not convert address to string: %s", get_last_error());
        return str_create_const(NULL);
    }

    *out_port = strtoul(serv, NULL, 0);

    return str_create(host, arena);
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
bool sock_bind(socketfd socket, const SocketAddress* address) {
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
void sock_close(socketfd socket) {
    closesocket(socket);
}

#endif