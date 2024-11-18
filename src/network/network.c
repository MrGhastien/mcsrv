/**
 * @file network.c
 * @author Bastien Morino
 *
 * Main loop of the network sub-system.
 */

#include "network.h"

#include "common_types.h"

#include "connection.h"
#include "logger.h"
#include "memory/arena.h"
#include "platform/mc_thread.h"
#include "receiver.h"
#include "security.h"
#include "sender.h"

#include <platform/network.h>

static NetworkContext ctx;

i32 create_server_socket(NetworkContext* ctx, char* host, i32 port) {
    socketfd server_socket = sock_create(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (!sock_is_valid(server_socket)) {
        log_fatalf("Failed to create the server socket: %s", strerror(errno));
        return 1;
    }
    ctx->server_socket = server_socket;

    int option = 1;
    // Let the socket reuse the address, to avoid errors when quickly rerunning the server
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof option);

    struct sockaddr_in saddr = {0};
    saddr.sin_family = AF_INET;
    if (!inet_aton(host, &saddr.sin_addr)) {
        log_fatal("The server address is invalid.");
        return 2;
    }

    SocketAddress address;

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

i32 network_init(char* host, i32 port, u64 max_connections) {
    i32 res = network_platform_init(&ctx);
    if (res)
        return res;

    res = create_server_socket(&ctx, host, port);
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

i32 handle_connection_io(Connection* conn, u32 events) {
    enum IOCode io_code = IOC_OK;
    if (events & IOEVENT_IN) {
        while (io_code == IOC_OK) {
            io_code = receive_packet(conn);
        }
        switch (io_code) {
        case IOC_CLOSED:
            log_warn("Peer closed connection.");
            close_connection(&ctx, conn);
            return 0;
        case IOC_ERROR:
            log_error("Errored connection.");
            close_connection(&ctx, conn);
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
                close_connection(&ctx, conn);
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

void network_finish(void) {

    encryption_cleanup(&ctx.enc_ctx);

    for (u32 i = 0; i < ctx.max_connections; i++) {
        Connection* c = &ctx.connections[i];
        if (c->sockfd >= 0)
            close_connection(&ctx, c);
    }

    close(ctx.eventfd);
    close(ctx.epollfd);
    close(ctx.server_socket);
    arena_destroy(&ctx.arena);
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
