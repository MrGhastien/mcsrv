#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include "connection.h"
#include "packet.h"
#include "utils.h"

static void on_packet_recv(Connection* conn) {
    Packet* packet = packet_read(conn);
    if (!packet) {
        fprintf(stderr, "Received invalid packet.\nClosing connection.\n");
        close(conn->sockfd);
        return;
    }

    pkt_acceptor handler = get_pkt_handler(packet, conn);
    if(handler)
        handler(packet, conn);

    puts("====");
    free(packet);
}

int main(int argc, char** argv) {

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("Could not create socket");
        return 1;
    }

    int option = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof option);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    if (!inet_aton("0.0.0.0", &addr.sin_addr)) {
        perror("Invalid bind address");
        return 2;
    }

    addr.sin_port = htons(25565);

    if (bind(sockfd, (struct sockaddr*)&addr, sizeof addr) == -1) {
        perror("Could not bind address");
        return 3;
    }

    if (listen(sockfd, 0) == -1) {
        perror("Socket cannot listen");
        return 4;
    }

    int epollfd = epoll_create1(0);
    if (epollfd == -1) {
        perror("Failed to create epoll instance");
        return 1;
    }

    struct epoll_event event_in = {.events = EPOLLIN | EPOLLET, .data.u64 = 0};

    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd, &event_in) == -1) {
        perror("Failed to create epoll instance");
        return 1;
    }

    struct epoll_event events[10];
    struct sockaddr_in peer_addr;
    socklen_t peer_addr_length = sizeof peer_addr;
    size_t eventCount = 0;

    while ((eventCount = epoll_wait(epollfd, events, 10, -1)) != -1) {
        for (size_t i = 0; i < eventCount; i++) {
            struct epoll_event* e = &events[i];
            if (e->data.u64 == 0) {
                int peerfd = accept(sockfd, (struct sockaddr*)&peer_addr, &peer_addr_length);
                if (peerfd == -1) {
                    perror("Invalid connection received");
                    return 7;
                }

                Connection* conn = malloc(sizeof(Connection));
                if (!conn) {
                    perror("Failed to allocate memory to register connection");
                    return 1;
                }
                conn->compression = FALSE;
                conn->sockfd = peerfd;
                conn->state = STATE_HANDSHAKE;
                event_in.data.ptr = conn;
                if (epoll_ctl(epollfd, EPOLL_CTL_ADD, peerfd, &event_in) == -1) {
                    perror("Failed to handle connection");
                    return 8;
                }
            } else
                on_packet_recv(e->data.ptr);
        }
    }

    close(sockfd);

    return 0;
}
