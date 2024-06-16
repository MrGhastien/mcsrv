#include "event/event.h"
#include "network/network.h"
#include "registry/registry.h"

static void init(void) {
    registry_system_init();

    event_system_init();
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    init();

    return net_handle("0.0.0.0", 25565, 10);
}
