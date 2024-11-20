//
// Created by bmorino on 14/11/2024.
//

#include <logger.h>
#include <network/receiver.h>
#include <network/sender.h>
#include <stdio.h>
#ifdef MC_PLATFORM_WINDOWS

#include "platform/mc_thread.h"
#include "platform/network.h"
#include "utils/bitwise.h"

#include <mswsock.h>

#define COMPL_KEY_ACCEPT 0
#define COMPL_KEY_STOP 1

i64 sock_recv_buf(socketfd socket, WSAOVERLAPPED* overlapped, ByteBuffer* output);
i64 sock_send_buf(socketfd socket, WSAOVERLAPPED* overlapped, ByteBuffer* input);

typedef struct PlatformConnection {
    Connection connection;
    WSAOVERLAPPED read_overlapped;
    WSAOVERLAPPED write_overlapped;
    u64 previous_read_size;
    u64 previous_write_size;
} PlatformConnection;

typedef struct PlatformNetworkCtx {
    HANDLE completion_port;
    WSAOVERLAPPED accept_overlapped;
    WSAOVERLAPPED stop_overlapped;
} PlatformNetworkCtx;

static PlatformNetworkCtx platform_ctx = {
    .completion_port = INVALID_HANDLE_VALUE,
};

char* get_last_error(void) {
    static char error_msg[2048];
    i32 error_code = WSAGetLastError();
    FormatMessage(FORMAT_MESSAGE_IGNORE_INSERTS, NULL, error_code, 0, error_msg, 2048, NULL);
    return error_msg;
}

i32 network_platform_init(NetworkContext* ctx, u64 max_connections) {
    WSADATA wsa_data;
    i32 res = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (res != 0) {
        log_fatalf("Failed to initialize Winsock 2.2: %s", get_last_error());
        return 1;
    }

    platform_ctx.completion_port = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0ull, 1);

    objpool_init(&ctx->connections, &ctx->arena, max_connections, sizeof(PlatformConnection));

    return 0;
}
i32 platform_socket_init(socketfd server_socket) {
    if (CreateIoCompletionPort(
            (HANDLE) server_socket, platform_ctx.completion_port, COMPL_KEY_ACCEPT, 1) ==
        INVALID_HANDLE_VALUE) {
        log_fatalf("Failed to create the server socket: %s", get_last_error());
        return 6;
    }
    return 0;
}

static void network_finish(NetworkContext* ctx) {

    encryption_cleanup(&ctx->enc_ctx);

    for(i64 i = 0; i < ctx->connections.capacity; i++) {
        PlatformConnection* pconn = objpool_get(&ctx->connections, i);
        if(pconn)
            close_connection(ctx, &pconn->connection);
    }

    sock_close(ctx->server_socket);

    CloseHandle(platform_ctx.completion_port);
    arena_destroy(&ctx->arena);
}


static i32 handle_connection_io(NetworkContext* ctx,
                                PlatformConnection* pconn,
                                WSAOVERLAPPED* overlapped,
                                u64 transferred) {

    enum IOCode code = IOC_OK;
    Connection* conn = &pconn->connection;

    // Resume processing the data
    if (overlapped == &pconn->read_overlapped) {
        // READ complete
        i64 total_written = conn->recv_buffer.size - pconn->previous_read_size;
        i64 to_unwrite = total_written - transferred;

        bytebuf_unwrite(&conn->recv_buffer, to_unwrite);

        while (code == IOC_OK) {
            code = receive_packet(ctx, conn);
            if (code == IOC_AGAIN)
                code = fill_buffer(ctx, conn);
        }
    }

    if (overlapped == &pconn->write_overlapped) {
        conn->pending_write = FALSE;
        i64 total_read = conn->send_buffer.size - pconn->previous_write_size;
        i64 to_unread = total_read - transferred;
        bytebuf_unread(&conn->send_buffer, to_unread);
        while (code == IOC_OK && conn->send_buffer.size > 0) {
            code = empty_buffer(ctx, conn);
        }
    }

    if (code == IOC_ERROR) {
        log_error("Errored connection.");
        close_connection(ctx, conn);
        return 1;
    }

    if (code == IOC_CLOSED) {
        log_warn("Peer closed connection.");
        close_connection(ctx, conn);
        return 0;
    }

    return 0;
}

static enum IOCode accept_connection(NetworkContext* ctx, socketfd* peer_socket) {
    static SocketAddress peer_address_cache;
    if (!sock_is_valid(*peer_socket)) {
        enum IOCode code = sock_accept(ctx->server_socket, peer_socket, &peer_address_cache);
        switch (code) {
        case IOC_ERROR:
            log_errorf("Failed to establish connection to peer: %s", get_last_error());
            return code;
            // no fall through allowed :(
        case IOC_PENDING:
            return code;
        default:
            break;
        }
    } else {
        if (!sock_get_peer_address(*peer_socket, &peer_address_cache)) {
            log_errorf("Could not get address of connected peer: %s", get_last_error());
            return IOC_ERROR;
        }
    }

    Arena arena = ctx->arena;

    u32 peer_port;
    string peer_host = sockaddr_to_string(&peer_address_cache, &arena, &peer_port);

    i64 index;
    PlatformConnection* pconn = objpool_add(&ctx->connections, &index);
    if (!pconn) {
        sock_close(*peer_socket);
        log_warn("Reached maximum connection amount, rejecting.");
        return IOC_CLOSED;
    }

    Connection* connection = &pconn->connection;
    *pconn = (PlatformConnection){
        .connection = conn_create(*peer_socket, index, &ctx->enc_ctx, peer_host, peer_port),
        .read_overlapped = {0},
        .write_overlapped = {0},
    };

    if (CreateIoCompletionPort(
            peer_socket, platform_ctx.completion_port, (uintptr_t) connection, 1) != 0) {
        log_errorf("Could not handle the connection to [%s:%u]: %s",
                   connection->peer_addr.base,
                   connection->peer_port,
                   get_last_error());
        return IOC_ERROR;
    }

    handle_connection_io(ctx, pconn, &pconn->read_overlapped, 0);

    log_infof("Accepted connection from [%s:%i].", peer_host.base, peer_port);
    return IOC_OK;
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

static enum IOCode try_accept(NetworkContext* ctx) {
    static socketfd peer_socket_cache = SOCKFD_INVALID;
    enum IOCode accept_res = IOC_OK;
    while (accept_res == IOC_OK) {
        accept_res = accept_connection(ctx, &peer_socket_cache);
    }

    return accept_res;
}

void* network_handle(void* params) {
    NetworkContext* ctx = params;

    mcthread_set_name("network");

    try_accept(ctx);

    while (ctx->should_continue) {
        unsigned long bytes_transferred;
        u64 key;
        WSAOVERLAPPED* overlapped;
        bool res = GetQueuedCompletionStatus(
            platform_ctx.completion_port, &bytes_transferred, &key, &overlapped, INFINITE);
        if (res) {
            if (key == COMPL_KEY_ACCEPT) {
                // the server socket has received a connection
                try_accept(ctx);
            } else if (key == COMPL_KEY_STOP) {
                // Stop!
                ctx->should_continue = FALSE;
                break;
            } else {
                // all good.
                PlatformConnection* conn = (PlatformConnection*) key;
                ctx->code = handle_connection_io(ctx, conn, overlapped, bytes_transferred);
            }

        } else if (overlapped) {
            // A pending IO operation failed.
            if (key == COMPL_KEY_ACCEPT) {
                // A connection could not be accepted.
                log_errorf("Failed to accept connection: %s", get_last_error());
            } else if (key == COMPL_KEY_STOP) {
                // Stop!
                ctx->should_continue = FALSE;
                break;
            } else {
                // all good.
                PlatformConnection* conn = (PlatformConnection*) key;
                ctx->code = handle_connection_io(ctx, conn, overlapped, bytes_transferred);
            }
        } else {
            // GetQueuedCompletionStatus failed.
            ctx->should_continue = FALSE;
            break;
        }
    }

    network_finish(ctx);

    return NULL;
}

void platform_network_stop(void) {
    if (!PostQueuedCompletionStatus(
            platform_ctx.completion_port, 0, COMPL_KEY_STOP, &platform_ctx.stop_overlapped)) {
        log_errorf("Failed to stop the network thread : %s", get_last_error());
    }
}

enum IOCode fill_buffer(NetworkContext* ctx, Connection* conn) {
    if (conn->recv_buffer.size == conn->recv_buffer.capacity)
        return IOC_OK;
    PlatformConnection* pconn = objpool_get(&ctx->connections, conn->table_index);
    if (pconn == NULL) {
        log_fatal("Connection has no platform specific data!");
        abort();
        return IOC_ERROR;
    }

    if (conn->recv_buffer.size < conn->recv_buffer.capacity) {
        i64 size = sock_recv_buf(conn->peer_socket, &pconn->read_overlapped, &conn->recv_buffer);
        if (size == SOCKIO_ERROR)
            return IOC_ERROR;
        if (size == SOCKIO_PENDING) {
            conn->pending_read = TRUE;
            return IOC_PENDING;
        }
    }

    return IOC_OK;
}

enum IOCode empty_buffer(NetworkContext* ctx, Connection* conn) {
    if (conn->send_buffer.size == 0)
        return IOC_OK;

    PlatformConnection* pconn = objpool_get(&ctx->connections, conn->table_index);
    if (pconn == NULL) {
        log_fatal("Connection has no platform specific data!");
        abort();
        return IOC_ERROR;
    }

    while (conn->send_buffer.size > 0) {
        i64 size = sock_send_buf(conn->peer_socket, &pconn->write_overlapped, &conn->send_buffer);
        if (size == SOCKIO_ERROR)
            return IOC_ERROR;
        if (size == SOCKIO_PENDING) {
            conn->pending_write = TRUE;
            return IOC_PENDING;
        }
    }
    return IOC_OK;
}


/* ===== Socket functions ===== */

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
    if (res != 0) {
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
    if (getnameinfo(&addr->data.sa, addr->length, host, NI_MAXHOST, serv, NI_MAXSERV, 0) != 0) {
        log_errorf("Could not convert address to string: %s", get_last_error());
        return str_create_const(NULL);
    }

    *out_port = strtoul(serv, NULL, 0);

    return str_create(host, arena);
}

bool sock_is_valid(socketfd socket) {
    return socket != INVALID_SOCKET;
}
i64 sock_recv_buf(socketfd socket, WSAOVERLAPPED* overlapped, ByteBuffer* output) {
    WSABUF regions[2];
    u64 size = 0;
    u64 total_size = 0;
    i32 region_count = 0;
    while (size < output->capacity - output->size) {
        void* ptr;
        size = bytebuf_contiguous_write(output, &ptr);
        regions[region_count].len = size;
        total_size += size;
        regions[region_count].buf = ptr;
        region_count++;
    }

    if (region_count == 0)
        return 0;

    unsigned long bytes = 0;
    unsigned long flags = 0;
    i32 res = WSARecv(socket, regions, region_count, &bytes, &flags, overlapped, NULL);
    if (res != 0) {
        i32 error = WSAGetLastError();
        if (error == WSA_IO_PENDING)
            return SOCKIO_PENDING;
        bytebuf_unwrite(output, size);
        log_errorf("Failed to fill the receive buffer: %s", get_last_error());
        return SOCKIO_ERROR;
    }

    bytebuf_unwrite(output, size - bytes);
    return bytes;
}
i64 sock_send_buf(socketfd socket, WSAOVERLAPPED* overlapped, ByteBuffer* input) {
    WSABUF regions[2];
    u64 size = 0;
    u64 total_size = 0;
    i32 region_count = 0;
    while (size < input->size) {
        void* cast_ptr = &regions[region_count].buf;
        size = bytebuf_contiguous_read(input, cast_ptr);
        regions[region_count].len = size;
        total_size += size;
        region_count++;
    }

    if (region_count == 0)
        return 0;

    unsigned long bytes = 0;
    i32 res = WSASend(socket, regions, region_count, &bytes, 0, overlapped, NULL);
    if (res != 0) {
        i32 error = WSAGetLastError();
        if (error == WSA_IO_PENDING)
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
enum IOCode sock_accept(socketfd socket, socketfd* out_accepted, SocketAddress* out_address) {
    u8 out_buf[1024];
    unsigned long received;

    socketfd accepted = WSASocketA(AF_INET, SOCK_STREAM, 0, 0, 0, 0);
    bool success = AcceptEx(socket,
                            accepted,
                            out_buf,
                            0,
                            sizeof(out_address->data.storage) + 16,
                            sizeof(out_address->data.storage) + 16,
                            &received,
                            &platform_ctx.accept_overlapped);

    if (!success) {
        i32 last_error = WSAGetLastError();
        if (last_error == ERROR_IO_PENDING)
            return IOC_PENDING;

        *out_accepted = SOCKFD_INVALID;
        closesocket(accepted);
        return IOC_ERROR;
    }

    struct sockaddr* local;
    i32 local_len;
    struct sockaddr* peer;
    i32 peer_len;

    GetAcceptExSockaddrs(out_buf,
                         0,
                         sizeof(out_address->data.storage) + 16,
                         sizeof(out_address->data.storage) + 16,
                         &local,
                         &local_len,
                         &peer,
                         &peer_len);

    out_address->length = peer_len;
    memcpy(&out_address->data.storage, peer, peer_len);
    *out_accepted = accepted;

    return IOC_OK;
}

bool sock_get_peer_address(socketfd socket, SocketAddress* out_address) {
    STATIC_ASSERT(sizeof(out_address->data.family) == sizeof(u16),
                  "Address family member in sockaddr is not a short int.");

    int buf_size = sizeof(out_address->data.storage);
    if (getpeername(socket, &out_address->data.sa, &buf_size) != 0)
        return FALSE;

    out_address->length = buf_size;
    return TRUE;
}
void sock_close(socketfd socket) {
    closesocket(socket);
}

#endif