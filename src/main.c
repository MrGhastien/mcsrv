#include "event/event.h"
#include "network/network.h"
#include "registry/registry.h"
#include <string.h>
#include <unistd.h>

#include <signal.h>

void on_interrupt(int num) {
    switch (num) {
    case SIGINT: {
        char* str = "Requesting shutdown.\n";
        write(STDOUT_FILENO, str, strlen(str));
        break;
    }
    default:
        break;
    }
}

static void init(char* host, i32 port, u64 max_connections) {

    struct sigaction action = {0};
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

    init("0.0.0.0", 25565, 10);

    res = net_handle();

    cleanup();

    return res;
}
