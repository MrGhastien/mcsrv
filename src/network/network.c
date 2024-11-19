/**
 * @file network.c
 * @author Bastien Morino
 *
 * Main loop of the network sub-system.
 */

#include "network.h"

#include "logger.h"

#include "common_types.h"
#include "platform/mc_thread.h"
#include "platform/network.h"

#include "receiver.h"
#include "sender.h"

static NetworkContext ctx;

i32 create_server_socket(NetworkContext* ctx, char* host, i32 port) {
    socketfd server_socket = sock_create(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (!sock_is_valid(server_socket)) {
        log_fatalf("Failed to create the server socket: %s", get_last_error());
        return 1;
    }

    int option = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (char*) &option, sizeof option) != 0) {
        log_fatalf("Failed to create the server socket: %s", get_last_error());
        return 2;
    }

    SocketAddress address;
    if (!sockaddr_parse(&address, host, port)) {
        log_fatal("Failed to create the server socket: Invalid or malformed address.");
        return 3;
    }

    if (!sock_bind(server_socket, &address)) {
        log_fatalf("Failed to bind the server socket: %s", get_last_error());
        return 4;
    }

    if (!sock_listen(server_socket, 0)) {
        log_fatalf("Failed to create the server socket: %s", get_last_error());
        return 5;
    }

    if (platform_socket_init(server_socket) != 0)
        return 7;

    log_debugf("Created the server socket, bound to [%s:%i].", host, port);
    ctx->server_socket = server_socket;
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
    objpool_init(&ctx.connections, &ctx.arena, max_connections, sizeof(Connection));
    ctx.host = str_create_const(host);
    ctx.port = port;
    ctx.code = 0;
    ctx.should_continue = TRUE;

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

void network_stop(void) {
    platform_network_stop();
    mcthread_join(&ctx.thread, NULL);
    log_debug("Network thread exited.");
}
