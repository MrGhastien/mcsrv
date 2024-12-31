//
// Created by bmorino on 14/11/2024.
//
#ifdef MC_PLATFORM_WINDOWS

#include "logger.h"
#include "network/packet_codec.h"
#include "platform/mc_thread.h"
#include "platform/network.h"

#include <mswsock.h>
#include <stdio.h>

#define COMPL_KEY_ACCEPT 0
#define COMPL_KEY_STOP 1

enum IOCode
sock_recv_buf(socketfd socket, WSAOVERLAPPED* overlapped, ByteBuffer* output, u64* out_byte_count);
enum IOCode
sock_send_buf(socketfd socket, WSAOVERLAPPED* overlapped, ByteBuffer* input, u64* out_byte_count);

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
    u8 accept_addr_buffer[1024];
} PlatformNetworkCtx;
typedef struct CompletionInfo {
    WSAOVERLAPPED* overlapped;
    u64 key;
    unsigned long transfer_size;
} CompletionInfo;

static PlatformNetworkCtx platform_ctx = {
    .completion_port = INVALID_HANDLE_VALUE,
};

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
bool sockaddr_from_acceptex(void* acceptex_buffer, SocketAddress* out_address) {
    struct sockaddr* local;
    i32 local_len;
    struct sockaddr* peer;
    i32 peer_len;

    GetAcceptExSockaddrs(acceptex_buffer,
                         0,
                         sizeof(out_address->data.storage) + 16,
                         sizeof(out_address->data.storage) + 16,
                         &local,
                         &local_len,
                         &peer,
                         &peer_len);

    out_address->length = peer_len;
    memcpy(&out_address->data.storage, peer, peer_len);

    return TRUE;
}

bool sock_is_valid(socketfd socket) {
    return socket != INVALID_SOCKET;
}
enum IOCode
sock_recv_buf(socketfd socket, WSAOVERLAPPED* overlapped, ByteBuffer* output, u64* out_byte_count) {
    WSABUF wsa_regions[2];
    u64 region_count = 2;
    BufferRegion buf_regions[2];
    if(bytebuf_get_write_regions(output, buf_regions, &region_count, 0) == 0)
        return IOC_OK;

    for (u64 i = 0; i < region_count; i++) {
        wsa_regions[i].len = buf_regions[i].size;
        wsa_regions[i].buf = buf_regions[i].start;
    }

    unsigned long bytes = 0;
    unsigned long flags = 0;
    i32 res = WSARecv(socket, wsa_regions, region_count, &bytes, &flags, overlapped, NULL);
    if (res != 0) {
        i32 error = WSAGetLastError();
        if (error == WSA_IO_PENDING)
            return IOC_PENDING;
        log_errorf("Failed to fill the receive buffer: %s", get_last_error());
        return IOC_ERROR;
    }

    bytebuf_register_write(output, bytes);
    if (bytes == 0)
        return IOC_CLOSED;
    *out_byte_count = bytes;
    return IOC_OK;
}
enum IOCode
sock_send_buf(socketfd socket, WSAOVERLAPPED* overlapped, ByteBuffer* input, u64* out_byte_count) {
    WSABUF wsa_regions[2];
    u64 region_count = 2;
    BufferRegion buf_regions[2];
    if(bytebuf_get_read_regions(input, buf_regions, &region_count, 0) == 0)
        return IOC_OK;

    for (u64 i = 0; i < region_count; i++) {
        wsa_regions[i].len = buf_regions[i].size;
        wsa_regions[i].buf = buf_regions[i].start;
    }

    unsigned long bytes = 0;
    i32 res = WSASend(socket, wsa_regions, region_count, &bytes, 0, overlapped, NULL);
    if (res != 0) {
        i32 error = WSAGetLastError();
        if (error == WSA_IO_PENDING)
            return IOC_PENDING;
        return IOC_ERROR;
    }

    bytebuf_register_read(input, bytes);
    if (bytes == 0)
        return IOC_CLOSED;
    *out_byte_count = bytes;
    return IOC_OK;
}

socketfd sock_create(int address_family, int type, int protocol) {
    return WSASocketA(address_family, type, protocol, 0, 0, WSA_FLAG_OVERLAPPED);
}
bool sock_bind(socketfd socket, const SocketAddress* address) {
    return bind(socket, &address->data.sa, address->length) == 0;
}
bool sock_listen(socketfd socket, i32 backlog) {
    return listen(socket, backlog) == 0;
}
enum IOCode sock_accept(socketfd socket, socketfd* out_accepted, void* out_buffer) {
    unsigned long received;
    socketfd accepted = WSASocketA(AF_INET, SOCK_STREAM, 0, 0, 0, WSA_FLAG_OVERLAPPED);
    bool success = AcceptEx(socket,
                            accepted,
                            out_buffer,
                            0,
                            sizeof(struct sockaddr_storage) + 16,
                            sizeof(struct sockaddr_storage) + 16,
                            &received,
                            &platform_ctx.accept_overlapped);

    *out_accepted = accepted;
    if (!success) {
        i32 last_error = WSAGetLastError();
        if (last_error == ERROR_IO_PENDING)
            return IOC_PENDING;

        *out_accepted = SOCKFD_INVALID;
        return IOC_ERROR;
    }

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

/* ===== Networking subsystem ===== */

const char* get_last_error(void) {
    static char error_msg[2048];
    i32 error_code = WSAGetLastError();
    FormatMessage(FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_SYSTEM,
                  NULL,
                  error_code,
                  MAKELANGID(LANG_SYSTEM_DEFAULT, SUBLANG_DEFAULT),
                  error_msg,
                  2048,
                  NULL);
    return error_msg;
}

i32 network_platform_init(NetworkContext* ctx, u64 max_connections) {
    WSADATA wsa_data;
    i32 res = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (res != 0) {
        log_fatalf("Failed to initialize Winsock 2.2: %s", get_last_error());
        return 1;
    }

    platform_ctx.completion_port = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0ull, 0);

    objpool_init(&ctx->connections, &ctx->arena, max_connections, sizeof(PlatformConnection));

    return 0;
}
i32 platform_socket_init(socketfd server_socket) {
    if (CreateIoCompletionPort(
            (HANDLE) server_socket, platform_ctx.completion_port, COMPL_KEY_ACCEPT, 0) ==
        INVALID_HANDLE_VALUE) {
        log_fatalf("Failed to create the server socket: %s", get_last_error());
        return 6;
    }
    return 0;
}

static void network_finish(NetworkContext* ctx) {

    encryption_cleanup(&ctx->enc_ctx);

    for (i64 i = 0; i < ctx->connections.capacity; i++) {
        PlatformConnection* pconn = objpool_get(&ctx->connections, i);
        if (pconn)
            close_connection(ctx, &pconn->connection);
    }

    sock_close(ctx->server_socket);

    CloseHandle(platform_ctx.completion_port);
    arena_destroy(&ctx->arena);
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

    enum IOCode res = IOC_PENDING;
    enum IOCode code = IOC_OK;
    while (conn->recv_buffer.size < conn->recv_buffer.capacity && code == IOC_OK) {
        u64 size;
        code = sock_recv_buf(conn->peer_socket, &pconn->read_overlapped, &conn->recv_buffer, &size);
        if (code < res)
            res = code;
        if (code == IOC_PENDING)
            conn->pending_recv = TRUE;
    }

    return res;
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

    enum IOCode res = IOC_PENDING;
    enum IOCode code = IOC_OK;
    while (conn->send_buffer.size > 0 && code == IOC_OK) {
        u64 size;
        code =
            sock_send_buf(conn->peer_socket, &pconn->write_overlapped, &conn->send_buffer, &size);
        if (code < res)
            res = code;
        if (code == IOC_PENDING)
            conn->pending_send = TRUE;
    }
    return res;
}
static enum IOCode handle_connection_io(NetworkContext* ctx,
                                        PlatformConnection* pconn,
                                        WSAOVERLAPPED* overlapped,
                                        u64 transferred) {

    enum IOCode code = IOC_OK;
    Connection* conn = &pconn->connection;

    // Resume processing the data
    if (overlapped == &pconn->read_overlapped && conn->pending_recv) {
        // READ complete
        conn->pending_recv = FALSE;
        bytebuf_register_write(&conn->recv_buffer, transferred);

        while (code == IOC_OK) {
            code = receive_packet(ctx, conn);
            if (code == IOC_AGAIN && !conn->pending_recv)
                code = fill_buffer(ctx, conn);
        }
    } else if (overlapped == &pconn->write_overlapped && conn->pending_send) {
        // WRITE complete
        conn->pending_send = FALSE;
        bytebuf_register_read(&conn->send_buffer, transferred);

        code = empty_buffer(ctx, conn);
    }

    if (code == IOC_ERROR) {
        log_error("Errored connection.");
    }

    if (code == IOC_CLOSED) {
        log_warn("Peer closed connection.");
    }

    return code;
}

static enum IOCode accept_connection(NetworkContext* ctx, socketfd* peer_socket) {
    SocketAddress peer_address_cache;
    if (!sock_is_valid(*peer_socket)) {
        enum IOCode code =
            sock_accept(ctx->server_socket, peer_socket, platform_ctx.accept_addr_buffer);
        switch (code) {
        case IOC_ERROR:
            log_errorf("Failed to establish connection to peer: %s", get_last_error());
            return code;
            // no fall through allowed :(
        case IOC_PENDING:
            return code;
        default:
            sockaddr_from_acceptex(platform_ctx.accept_addr_buffer, &peer_address_cache);
            break;
        }
    } else
        sockaddr_from_acceptex(platform_ctx.accept_addr_buffer, &peer_address_cache);

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
            (HANDLE)*peer_socket, platform_ctx.completion_port, (uintptr_t) pconn, 0) != platform_ctx.completion_port) {
        log_errorf("Could not handle the connection to [%s:%u]: %s",
                   connection->peer_addr.base,
                   connection->peer_port,
                   get_last_error());
        close_connection(ctx, &pconn->connection);
        return IOC_ERROR;
    }

    *peer_socket = SOCKFD_INVALID;
    log_infof("Accepted connection from [%s:%i].", peer_host.base, peer_port);

    enum IOCode code = handle_connection_io(ctx, pconn, &pconn->read_overlapped, 0);
    if (code < IOC_OK)
        close_connection(ctx, &pconn->connection);
    return code;
}
static enum IOCode try_accept(NetworkContext* ctx) {
    static socketfd peer_socket_cache = SOCKFD_INVALID;
    enum IOCode accept_res = IOC_OK;
    while (accept_res == IOC_OK) {
        accept_res = accept_connection(ctx, &peer_socket_cache);
    }

    return accept_res;
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

static void handle_completion(NetworkContext* ctx, CompletionInfo* info, bool success) {
    switch (info->key) {
    case COMPL_KEY_ACCEPT:
        if (success)
            try_accept(ctx);
        else
            log_errorf("Failed to accept connection: %s", get_last_error());
        break;
    case COMPL_KEY_STOP:
        ctx->should_continue = FALSE;
        break;
    default:
        PlatformConnection* pconn = (PlatformConnection*) info->key;
        if (!success ||
            handle_connection_io(ctx, pconn, info->overlapped, info->transfer_size) < IOC_OK)
            close_connection(ctx, &pconn->connection);
        break;
    }
}

void* network_handle(void* params) {
    NetworkContext* ctx = params;

    mcthread_set_name("network");

    try_accept(ctx);

    while (ctx->should_continue) {
        CompletionInfo info;
        log_debug("Waiting for completion...");
        bool res = GetQueuedCompletionStatus(platform_ctx.completion_port,
                                             &info.transfer_size,
                                             &info.key,
                                             &info.overlapped,
                                             INFINITE);
        if (res && info.overlapped != NULL) {
            handle_completion(ctx, &info, res);
        } else {
            // GetQueuedCompletionStatus failed.
            ctx->should_continue = FALSE;
            break;
        }
    }

    network_finish(ctx);

    return NULL;
}

/* ===== Socket functions ===== */

#endif
