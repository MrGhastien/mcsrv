#include "event/event.h"
#include "logger.h"
#include "network/network.h"
#include "registry/registry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <signal.h>

void on_interrupt(int num) {
    switch (num) {
    case SIGINT: {
        //TODO: Make this signal handler safe !
        log_info("Requested shutdown.");
        break;
    }
    default:
        break;
    }
}

static void init(char* host, i32 port, u64 max_connections) {

    struct sigaction action = {0};
    sigemptyset(&action.sa_mask);
    action.sa_handler       = &on_interrupt;

    sigaction(SIGINT, &action, NULL);

    event_system_init();
    registry_system_init();
    net_init(host, port, max_connections);
}

static void cleanup(void) {
    net_cleanup();
    registry_system_cleanup();
    event_system_cleanup();
}

int main(int argc, char** argv) {
    (void) argc;
    (void) argv;

    i32 res;

#ifdef DEBUG
    log_trace("Trace message test.");
    log_debug("Debug message test.");
    log_info("Info message test.");
    log_warn("Warning message test.");
    log_error("Error message test.");
    log_fatal("Fatal error message test. Don't worry, I won't crash now !");
#endif

    init("0.0.0.0", 25565, 10);

    res = net_handle();

    cleanup();

    return res;
}
