/**
 * @file network.c
 * @author Bastien Morino
 *
 * Main loop of the network sub-system.
 */

#include "network.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include "connection.h"
#include "logger.h"
#include "memory/arena.h"
#include "platform/mc_thread.h"
#include "receiver.h"
#include "security.h"
#include "sender.h"
#include "utils/string.h"

#include <platform/mc_mutex.h>

typedef struct net_ctx {
    Arena arena;

    Connection* connections;
    u32 max_connections;
    u32 connection_count;

    int serverfd;
    int epollfd;
    int eventfd;
    MCThread thread;

    EncryptionContext enc_ctx;
    u64 compress_threshold;

    string host;
    u32 port;

    i32 code;
    bool should_continue;
} NetworkContext;

static NetworkContext ctx;

static void* network_handle(void* params);

//X
static i32 create_server_socket(char* host, i32 port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    ctx.serverfd = sockfd;
    if (sockfd == -1) {
        log_fatalf("Failed to create the server socket: %s", strerror(errno));
        return 1;
    }

    int option = 1;
    // Let the socket reuse the address, to avoid errors when quickly rerunning the server
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof option);

    struct sockaddr_in saddr = {0};
    saddr.sin_family = AF_INET;
    if (!inet_aton(host, &saddr.sin_addr)) {
        log_fatal("The server address is invalid.");
        return 2;
    }

    saddr.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr*) &saddr, sizeof saddr) == -1) {
        log_fatal("Could not bind server socket.");
        return 3;
    }

    if (listen(sockfd, 0) == -1) {
        log_fatal("Socket cannot listen");
        return 4;
    }
    return 0;
}

//X
static i32 init_epoll(void) {
    int epollfd = epoll_create1(0);
    ctx.epollfd = epollfd;
    if (epollfd == -1) {
        log_fatalf("Failed to create the epoll instance: %s", strerror(errno));
        return 1;
    }

    struct epoll_event event_in = {.events = EPOLLIN | EPOLLET, .data.fd = -1};
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, ctx.serverfd, &event_in) == -1) {
        log_fatalf("Could not register the server socket to epoll: %s", strerror(errno));
        return 1;
    }

    ctx.eventfd = eventfd(0, 0);
    if (ctx.eventfd == -1) {
        log_fatalf("Failed to create the stopping event file descriptor: %s", strerror(errno));
        return 1;
    }

    event_in.data.fd = ctx.eventfd;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, ctx.eventfd, &event_in) == -1) {
        log_fatalf("Could not register the event file descriptor to epoll: %s", strerror(errno));
        return 1;
    }
    return 0;
}

i32 network_init(char* host, i32 port, u64 max_connections) {
    i32 res = create_server_socket(host, port);
    if (res)
        return res;

    res = init_epoll();
    if (res)
        return res;

    ctx.arena = arena_create(40960);
    ctx.connections = arena_allocate(&ctx.arena, sizeof(Connection) * max_connections);
    ctx.max_connections = max_connections;
    ctx.connection_count = 0;
    ctx.host = str_create_const(host);
    ctx.port = port;
    ctx.code = 0;
    ctx.should_continue = TRUE;

    for (u64 i = 0; i < max_connections; i++) {
        ctx.connections[i].sockfd = -1;
    }

    if (!encryption_init(&ctx.enc_ctx))
        return 3;

    log_debug("Network subsystem initialized.");

    mcthread_create(&ctx.thread, &network_handle, NULL);
    return 0;
}

//X
static i32 accept_connection(void) {
    struct sockaddr_in peer_addr;
    socklen_t peer_addr_length = sizeof peer_addr;
    int peerfd = accept(ctx.serverfd, (struct sockaddr*) &peer_addr, &peer_addr_length);
    if (peerfd == -1) {
        log_error("Invalid connection received.");
        return 1;
    }
    int flags = fcntl(peerfd, F_GETFL, 0);
    if (fcntl(peerfd, F_SETFL, flags | O_NONBLOCK) == -1) {
        log_error("Could not set connection socket to non-blocking mode.");
        return 1;
    }

    i64 idx = -1;
    for (u64 i = 0; i < ctx.max_connections; i++) {
        if (ctx.connections[i].sockfd == -1) {
            idx = i;
            break;
        }
    }
    if (idx == -1) {
        fputs("Max number of connections reached! Try again later.", stderr);
        return 1;
    }

    struct epoll_event event_in = {.events = EPOLLIN | EPOLLOUT | EPOLLET, .data.u64 = idx};
    if (epoll_ctl(ctx.epollfd, EPOLL_CTL_ADD, peerfd, &event_in) == -1) {
        log_error("Could not register the connection inside the network loop.");
        return 1;
    }

    char addr_str[16];
    inet_ntop(AF_INET, &peer_addr.sin_addr, addr_str, 16);

    ctx.connections[idx] =
        conn_create(peerfd, idx, &ctx.enc_ctx, str_create_const(addr_str), peer_addr.sin_port);

    log_infof("Accepted connection from %s:%i.", addr_str, peer_addr.sin_port);
    return 0;
}

//X
void close_connection(Connection* conn) {
    struct epoll_event placeholder;
    log_infof("Closing connection %i.", conn->sockfd);

    if (conn->encryption) {
        encryption_cleanup_peer(&conn->peer_enc_ctx);
    }

    close(conn->sockfd);
    epoll_ctl(ctx.epollfd, EPOLL_CTL_DEL, conn->sockfd, &placeholder);
    arena_destroy(&conn->scratch_arena);
    arena_destroy(&conn->persistent_arena);
    mcmutex_destroy(&conn->mutex);
    conn->sockfd = -1;
    ctx.connection_count--;
}

static void network_finish(void) {

    encryption_cleanup(&ctx.enc_ctx);

    for (u32 i = 0; i < ctx.max_connections; i++) {
        Connection* c = &ctx.connections[i];
        if (c->sockfd >= 0)
            close_connection(c);
    }

    close(ctx.eventfd);
    close(ctx.epollfd);
    close(ctx.serverfd);
    arena_destroy(&ctx.arena);
}

//X
static i32 handle_connection_io(Connection* conn, u32 events) {
    enum IOCode io_code = IOC_OK;
    if (events & IOEVENT_IN) {
        while (io_code == IOC_OK) {
            io_code = receive_packet(conn);
        }
        switch (io_code) {
        case IOC_CLOSED:
            log_warn("Peer closed connection.");
            close_connection(conn);
            return 0;
        case IOC_ERROR:
            log_error("Errored connection.");
            close_connection(conn);
            return 1;
        default:
            break;
        }
    }

    if (events & IOEVENT_OUT) {
        if (!conn->can_send) {
            io_code = sender_send(conn);
            switch (io_code) {
            case IOC_CLOSED:
            case IOC_ERROR:
                close_connection(conn);
                break;
            case IOC_AGAIN:
                conn->can_send = FALSE;
                break;
            default:
                break;
            }
        }
        conn->can_send = TRUE;
    }

    return 0;
}

//X
static void* network_handle(void* params) {
    (void) params;
    struct epoll_event events[10];
    i32 eventCount = 0;

    mcthread_set_name("network");

    sigset_t sigmask;
    sigfillset(&sigmask);
    sigdelset(&sigmask, SIGTERM);

    log_infof("Listening for connections on %s:%u...", ctx.host.base, ctx.port);
    while (ctx.should_continue) {
        eventCount = epoll_wait(ctx.epollfd, events, 10, -1);
        for (i32 i = 0; i < eventCount; i++) {
            struct epoll_event* e = &events[i];
            if (e->data.fd == -1)
                accept_connection();
            else if (e->data.fd == ctx.eventfd)
                ctx.should_continue = FALSE;
            else {
                Connection* conn = &ctx.connections[e->data.u64];
                ctx.code = handle_connection_io(conn, e->events);
            }
        }
    }

    if (eventCount == -1) {
        switch (errno) {
        case EINTR:
            break;
        default:
            log_fatalf("Network error: %s", strerror(errno));
            break;
        }
    }

    network_finish();
    return NULL;
}

/**
 *
 * Tells the network loop to exit, then joins the calling thread with the network thread.
 */
void network_stop(void) {
    u64 count = 1;
    write(ctx.eventfd, &count, sizeof(count));
    mcthread_join(&ctx.thread, NULL);
    log_debug("Network thread exited.");
}
