#ifdef MC_PLATFORM_LINUX

#include "containers/bytebuffer.h"
#include "containers/object_pool.h"
#include "definitions.h"
#include "logger.h"
#include "network/common_types.h"
#include "network/connection.h"
#include "network/packet_codec.h"
#include "network/security.h"

#include "platform/network.h"
#include "platform/socket.h"
#include "platform/platform.h"

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <signal.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

typedef struct PlatformNetworkCtx {
    int eventfd;
    int epollfd;
} PlatformNetworkCtx;

static PlatformNetworkCtx platform_ctx = {
    .eventfd = -1,
    .epollfd = -1,
};

/* ===== Socket functions ===== */

socketfd sock_create(int address_family, int type, int protocol) {
    return socket(address_family, type, protocol);
}

bool sock_is_valid(socketfd socket) {
    return (int) socket >= 0;
}

bool sock_bind(socketfd socket, const SocketAddress* address) {
    return bind(socket, &address->data.sa, address->length) == 0;
}
bool sock_listen(socketfd socket, i32 backlog) {
    return listen(socket, backlog) == 0;
}
void sock_close(socketfd socket) {
    close(socket);
}

enum IOCode sock_accept(socketfd socket, socketfd* out_accepted, SocketAddress* out_address) {
    socklen_t addr_len = sizeof(out_address->data.storage);
    socketfd peer_socket = accept(socket, &out_address->data.sa, &addr_len);
    if (!sock_is_valid(peer_socket)) {
        return IOC_ERROR;
    }

    out_address->length = addr_len;

    int flags = fcntl(peer_socket, F_GETFL, 0);
    if (fcntl(peer_socket, F_SETFL, flags | O_NONBLOCK) == -1) {
        sock_close(peer_socket);
        log_error("Could not set connection socket to non-blocking mode.");
        return IOC_ERROR;
    }

    *out_accepted = peer_socket;

    return IOC_OK;
}

enum IOCode sock_recv_buf(socketfd socket, ByteBuffer* output, u64* out_byte_count) {
    if (output->capacity == output->size)
        return IOC_OK;
    u64 region_count = 2;
    BufferRegion regions[2];
    bytebuf_get_write_regions(output, regions, &region_count, 0);

    struct iovec iov_regions[2];

    for (u64 i = 0; i < region_count; i++) {
        iov_regions[i].iov_len = regions[i].size;
        iov_regions[i].iov_base = regions[i].start;
    }

    struct msghdr header = {
        .msg_name = NULL,
        .msg_namelen = 0,
        .msg_iov = iov_regions,
        .msg_iovlen = region_count,
        .msg_control = NULL,
        .msg_controllen = 0,
    };

    i64 res = recvmsg(socket, &header, 0);
    if (res == -1) {
        if (errno == EWOULDBLOCK || errno == EAGAIN)
            return IOC_AGAIN;
        return IOC_ERROR;
    }
    if (res == 0)
        return IOC_CLOSED;

    bytebuf_register_write(output, res);
    *out_byte_count = res;

    return IOC_OK;
}

enum IOCode sock_send_buf(socketfd socket, ByteBuffer* input, u64* out_byte_count) {
    if (input->size == 0)
        return IOC_OK;
    u64 region_count = 2;
    BufferRegion regions[2];
    bytebuf_get_read_regions(input, regions, &region_count, 0);

    struct iovec iov_regions[2];

    for (u64 i = 0; i < region_count; i++) {
        iov_regions[i].iov_len = regions[i].size;
        iov_regions[i].iov_base = regions[i].start;
    }

    struct msghdr header = {
        .msg_name = NULL,
        .msg_namelen = 0,
        .msg_iov = iov_regions,
        .msg_iovlen = region_count,
        .msg_control = NULL,
        .msg_controllen = 0,
    };

    i64 res = sendmsg(socket, &header, 0);
    if (res == -1) {
        if (errno == EWOULDBLOCK || errno == EAGAIN)
            return IOC_AGAIN;
        return IOC_ERROR;
    } else if (res == 0)
        return IOC_CLOSED;

    bytebuf_register_read(input, res);
    *out_byte_count = res;

    return IOC_OK;
}

void sockaddr_init(SocketAddress* addr, u16 family) {
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
        log_errorf("Could not parse address : %s", gai_strerror(res));
        return FALSE;
    }

    memcpy(&addr->data.storage, info->ai_addr, info->ai_addrlen);
    addr->length = info->ai_addrlen;

    freeaddrinfo(info);

    return TRUE;
}
string sockaddr_to_string(SocketAddress* addr, Arena* arena, u32* out_port) {
    char host[NI_MAXHOST];
    char serv[NI_MAXSERV];
    if (getnameinfo(&addr->data.sa, addr->length, host, NI_MAXHOST, serv, NI_MAXSERV, 0) != 0) {
        log_errorf("Could not convert address to string: %s", get_last_error());
        return str_create_view(NULL);
    }

    *out_port = strtoul(serv, NULL, 0);

    return str_create(host, arena);
}

/* ===== Networking sub-system ===== */

enum IOCode fill_buffer(NetworkContext* ctx, Connection* conn) {
    UNUSED(ctx);
    enum IOCode res = IOC_AGAIN;
    enum IOCode code = IOC_OK;

    i64 starting_pos = bytebuf_current_pos(&conn->recv_buffer);

    while (conn->recv_buffer.size < conn->recv_buffer.capacity && code == IOC_OK) {
        u64 size = 0;
        code = sock_recv_buf(conn->peer_socket, &conn->recv_buffer, &size);
        if (code < res)
            res = code;
        if (code == IOC_AGAIN)
            conn->pending_recv = TRUE;
    }

    if(conn->encryption) {
        encryption_decipher(&conn->peer_enc_ctx, &conn->recv_buffer, starting_pos);
    }

    /*
      Because decompression increases the size of the data, we have to make sure we have enough room
      inside the connection's recv buffer.
      I think we can use the packet cache to get the next packet's size.

      Maybe the idea of reading as much bytes as possible in one go is not so good.
      Reading the right amount of bytes each time we decode a packet might be simpler (and probably as fast). 
     */
    /* if (conn->compression) { */
    /*     if(compression_decompress(&conn->cmprss_ctx, &conn->recv_buffer, dst_buffer) < 0) */
    /*         return IOC_ERROR; */
    /* } */
    // I think i will only decrypt here, and decompress when receiving a packet
    // This works well !

    return res;
}

enum IOCode empty_buffer(NetworkContext* ctx, Connection* conn) {
    UNUSED(ctx);
    enum IOCode code = IOC_OK;
    while (conn->send_buffer.size > 0 && code == IOC_OK) {
        u64 size;
        code = sock_send_buf(conn->peer_socket, &conn->send_buffer, &size);
        if (code == IOC_AGAIN)
            conn->pending_send = TRUE;
    }
    return code;
}

i32 platform_socket_init(socketfd server_socket) {

    struct epoll_event event_in = {.events = EPOLLIN | EPOLLET, .data.fd = -1};
    if (epoll_ctl(platform_ctx.epollfd, EPOLL_CTL_ADD, server_socket, &event_in) == -1) {
        log_fatalf("Could not register the server socket to epoll: %s", get_last_error());
        return 1;
    }

    return 0;
}

i32 network_platform_init(NetworkContext* ctx, u64 max_connections) {
    int epollfd = epoll_create1(0);
    platform_ctx.epollfd = epollfd;
    if (epollfd == -1) {
        log_fatalf("Failed to create the epoll instance: %s", get_last_error());
        return 1;
    }

    platform_ctx.eventfd = eventfd(0, 0);
    if (platform_ctx.eventfd == -1) {
        log_fatalf("Failed to create the stopping event file descriptor: %s", get_last_error());
        return 1;
    }

    struct epoll_event event_in = {.events = EPOLLIN | EPOLLET, .data.fd = -2};
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, platform_ctx.eventfd, &event_in) == -1) {
        log_fatalf("Could not register the event file descriptor to epoll: %s", get_last_error());
        return 1;
    }

    objpool_init(&ctx->connections, &ctx->arena, max_connections, sizeof(Connection));
    return 0;
}

static enum IOCode accept_connection(NetworkContext* ctx) {
    SocketAddress peer_address;
    socketfd peer_socket;
    enum IOCode code = sock_accept(ctx->server_socket, &peer_socket, &peer_address);
    switch (code) {
    case IOC_ERROR:
        log_errorf("Failed to establish connection to peer: %s", get_last_error());
        return code;
    case IOC_AGAIN:
        return code;
    default:
        break;
    }

    i64 index;
    Connection* conn = objpool_add(&ctx->connections, &index);
    if (!conn) {
        log_warn("Reached maximum connection amount, rejecting.");
        sock_close(peer_socket);
        return IOC_CLOSED;
    }

    struct epoll_event event_in = {.events = EPOLLIN | EPOLLOUT | EPOLLET, .data.u64 = index};
    if (epoll_ctl(platform_ctx.epollfd, EPOLL_CTL_ADD, peer_socket, &event_in) == -1) {
        log_errorf("Could not register the connection inside the network loop : %s",
                   get_last_error());
        return 1;
    }

    Arena arena = ctx->arena;

    u32 peer_port;
    string peer_host = sockaddr_to_string(&peer_address, &arena, &peer_port);

    *conn = conn_create(peer_socket, index, &ctx->enc_ctx, peer_host, peer_port);
    conn->pending_recv = TRUE;
    conn->pending_send = TRUE;

    log_infof("Accepted connection from [%s:%i].", peer_host.base, peer_port);
    return IOC_OK;
}

void close_connection(NetworkContext* ctx, Connection* conn) {
    struct epoll_event placeholder;
    log_infof("Closing connection to [%s:%i].", conn->peer_addr.base, conn->peer_port);

    //TODO: Send a `DISCONNECT` packet when closing a connection.

    if (conn->encryption) {
        encryption_cleanup_peer(&conn->peer_enc_ctx);
    }

    sock_close(conn->peer_socket);
    epoll_ctl(platform_ctx.epollfd, EPOLL_CTL_DEL, conn->peer_socket, &placeholder);
    arena_destroy(&conn->scratch_arena);
    arena_destroy(&conn->persistent_arena);
    mcmutex_destroy(&conn->mutex);
    conn->peer_socket = SOCKFD_INVALID;
    objpool_remove(&ctx->connections, conn->table_index);
}

static void network_finish(NetworkContext* ctx) {

    encryption_cleanup(&ctx->enc_ctx);

    for (i64 i = 0; i < ctx->connections.capacity; i++) {
        Connection* conn = objpool_get(&ctx->connections, i);
        if (conn)
            close_connection(ctx, conn);
    }

    sock_close(ctx->server_socket);

    close(platform_ctx.epollfd);
    close(platform_ctx.eventfd);
    arena_destroy(&ctx->arena);
}

static enum IOCode handle_connection_io(NetworkContext* ctx, Connection* conn, i32 events) {
    enum IOCode io_code = IOC_OK;
    if (events & EPOLLIN && conn->pending_recv) {
        conn->pending_recv = FALSE;
        io_code = fill_buffer(ctx, conn);
        while (io_code == IOC_OK) {
            io_code = receive_packet(ctx, conn);
            if (io_code == IOC_AGAIN && !conn->pending_recv)
                io_code = fill_buffer(ctx, conn);
        }
        switch (io_code) {
        case IOC_CLOSED:
            log_warn("Peer closed connection.");
            close_connection(ctx, conn);
            break;
        case IOC_ERROR:
            log_error("An error occurred while processing a connection.");
            close_connection(ctx, conn);
            break;
        default:
            break;
        }
    }

    if (events & EPOLLOUT && conn->pending_send) {
        conn->pending_send = FALSE;
        io_code = empty_buffer(ctx, conn);
        switch (io_code) {
        case IOC_CLOSED:
        case IOC_ERROR:
            close_connection(ctx, conn);
            break;
        default:
            break;
        }
    }

    return io_code;
}

void* network_handle(void* params) {
    NetworkContext* ctx = params;
    struct epoll_event events[10];
    i32 eventCount = 0;

    mcthread_set_name("network");

    sigset_t sigmask;
    sigfillset(&sigmask);
    sigdelset(&sigmask, SIGTERM);

    log_infof("Listening for connections on %s:%u...", ctx->host.base, ctx->port);
    while (ctx->should_continue) {
        log_trace("Waiting for EPoll notifications...");
        eventCount = epoll_wait(platform_ctx.epollfd, events, 10, -1);
        for (i32 i = 0; i < eventCount; i++) {
            struct epoll_event* e = &events[i];
            if (e->data.fd == -1) // server socket
                accept_connection(ctx);
            else if (e->data.fd == -2) // eventfd
                ctx->should_continue = FALSE;
            else {
                Connection* conn = objpool_get(&ctx->connections, e->data.u64);
                memory_dump_stats();
                handle_connection_io(ctx, conn, e->events);
            }
        }
    }

    if (eventCount == -1) {
        switch (errno) {
        case EINTR:
            break;
        default:
            log_fatalf("Network error: %s", get_last_error());
            break;
        }
    }

    network_finish(ctx);
    return NULL;
}

void platform_network_stop(void) {
    i64 count = 1;
    i64 res = 0;
    while (res <= 0) {
        res = write(platform_ctx.eventfd, &count, sizeof(count));
    }
}

#endif
