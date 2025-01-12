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

#include "memory/mem_tags.h"
#include "platform/mc_thread.h"
#include "platform/network.h"
#include "platform/platform.h"
#include "platform/time.h"

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

    if (network_platform_init_socket(server_socket) != 0)
        return 7;

    log_debugf("Created the server socket, bound to [%s:%i].", host, port);
    ctx->server_socket = server_socket;
    return 0;
}
i32 network_init(char* host, i32 port, u64 max_connections) {

    ctx.arena = arena_create(40960, BLK_TAG_NETWORK);
    ctx.host = str_view(host);
    ctx.port = port;
    ctx.code = 0;
    if(!timestamp(&ctx.last_connection_clean))
        log_warnf("Failed to set last keep-alive cleaning timestamp to now: %s", get_last_error());

    i32 res = network_platform_init(&ctx, max_connections);
    if (res)
        return res;

    res = create_server_socket(&ctx, host, port);
    if (res)
        return res;

    if (!encryption_init(&ctx.enc_ctx))
        return 3;

    ctx.should_continue = TRUE;
    log_debug("Network subsystem initialized.");

    mcthread_create(&ctx.thread, &network_handle, &ctx);
    return 0;
}

struct check_keep_alive_data {
    NetworkContext* ctx;
    struct timespec now;
};
static void check_keep_alive(void* ptr, i64 idx, void* user_data) {
    UNUSED(idx);
    Connection* conn = ptr;
    struct check_keep_alive_data* data = user_data;

    if(data->now.tv_sec - conn->last_keep_alive.tv_sec >= 15)
        close_connection(data->ctx, conn);
}
void network_clean_connections(NetworkContext* ctx) {
    struct check_keep_alive_data data = {.ctx = ctx};
    timestamp(&data.now);

    objpool_foreach(&ctx->connections, &check_keep_alive, &data);
}

void network_finish(NetworkContext* ctx) {

    encryption_cleanup(&ctx->enc_ctx);

    for (i64 i = 0; i < ctx->connections.capacity; i++) {
        Connection* conn = objpool_get(&ctx->connections, i);
        if (conn)
            close_connection(ctx, conn);
    }

    sock_close(ctx->server_socket);
    platform_network_finish();
    arena_destroy(&ctx->arena);
}
void network_stop(void) {
    platform_network_stop();
    mcthread_join(&ctx.thread, NULL);
    log_debug("Network thread exited.");
}
