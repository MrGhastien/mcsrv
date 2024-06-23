#include "event/event.h"
#include "logger.h"
#include "network/network.h"
#include "platform/signal-handler.h"
#include "registry/registry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <signal.h>

typedef struct server_ctx {
    bool running;
} ServerContext;

static ServerContext server_ctx;

static void init(char* host, i32 port, u64 max_connections) {

    sigset_t global_sigmask;
    sigfillset(&global_sigmask);

    // Block the SIGINT & SIGTERM signals for all other threads.
    // Make sure the main thread is the one handling signals.
    pthread_sigmask(SIG_BLOCK, &global_sigmask, NULL);

    signal_system_init();
    event_system_init();
    registry_system_init();
    network_init(host, port, max_connections);

    server_ctx.running = TRUE;
}

static void cleanup(void) {
    server_ctx.running = FALSE;

    network_stop();
    registry_system_cleanup();
    event_system_cleanup();
    signal_system_cleanup();
}

int main(int argc, char** argv) {
    (void) argc;
    (void) argv;
    i32 res = 0;

    init("0.0.0.0", 25565, 10);

    event_handle();

    cleanup();
    log_info("Goodbye!");

    return res;
}
