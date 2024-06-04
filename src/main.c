#include "network/network.h"
#include <stdio.h>

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    return net_handle("0.0.0.0", 25565);
}
