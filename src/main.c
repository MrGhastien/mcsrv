#include "definitions.h"
#include "event/event.h"
#include "logger.h"
#include "network/network.h"
#include "platform/platform.h"
#include "registry/registry.h"
#include "memory/mem_tags.h"

typedef struct server_ctx {
    bool running;
} ServerContext;

static ServerContext server_ctx;

static i32 init(char* host, i32 port, u64 max_connections) {
    i32 code = 0;

    memory_stats_init();
    platform_init();

    logger_system_init();
    event_system_init();
    registry_system_init();
    code = network_init(host, port, max_connections);

    if (code != 0) {
        log_fatal("Failed to initialize the server.");
        server_ctx.running = FALSE;
    } else {
        server_ctx.running = TRUE;
    }
    return code;
}

static void cleanup(void) {
    server_ctx.running = FALSE;

    network_stop();
    registry_system_cleanup();
    event_system_cleanup();
    logger_system_cleanup();

    platform_cleanup();
}

int main(int argc, char** argv) {
    (void) argc;
    (void) argv;
    i32 res = 0;

    res = init("0.0.0.0", 25565, 10);

    if (res != 0) {
        return res;
    }


    event_handle();

    cleanup();
    log_info("Goodbye!");

    return res;
}
