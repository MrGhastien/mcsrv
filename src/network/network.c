#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <fcntl.h>
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
    arena_save(&conn->arena);
    Packet* packet = packet_read(conn);
    if (!packet) {
        fprintf(stderr, "Received invalid packet.\nClosing connection.\n");
        close(conn->sockfd);

        arena_restore(&conn->arena);
        return;
    }

    pkt_acceptor handler = get_pkt_handler(packet, conn);
    if (handler)
        handler(packet, conn);

    puts("====");
    arena_restore(&conn->arena);
}

static int net_init(char* host, int port, int* out_serverfd, int* out_epollfd) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    *out_serverfd = sockfd;
    if (sockfd == -1) {
        perror("Could not create socket");
        return 1;
    }

    int option = 1;
    // Let the socket reuse the address, to avoid errors when quickly rerunning the server
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof option);

    struct sockaddr_in saddr;
    memset(&saddr, 0, sizeof saddr);
    saddr.sin_family = AF_INET;
    if (!inet_aton(host, &saddr.sin_addr)) {
        perror("Invalid bind address");
        return 2;
    }

    saddr.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr*)&saddr, sizeof saddr) == -1) {
        perror("Could not bind address");
        return 3;
    }

    if (listen(sockfd, 0) == -1) {
        perror("Socket cannot listen");
        return 4;
    }

    int epollfd = epoll_create1(0);
    *out_epollfd = epollfd;
    if (epollfd == -1) {
        perror("Failed to create epoll instance");
        return 1;
    }

    struct epoll_event event_in = {.events = EPOLLIN | EPOLLET, .data.u64 = 0};
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd, &event_in) == -1) {
        perror("Failed to create epoll instance");
        return 1;
    }
    printf("Listening on %s:%i.\n", host, port);
    return 0;
}

static int accept_connection(Arena* conn_arena, int serverfd, int epollfd) {
    struct sockaddr_in peer_addr;
    socklen_t peer_addr_length = sizeof peer_addr;
    int peerfd = accept(serverfd, (struct sockaddr*)&peer_addr, &peer_addr_length);
    if (peerfd == -1) {
        perror("Invalid connection received");
        return 1;
    }
    int flags = fcntl(peerfd, F_GETFL, 0);
    if (fcntl(peerfd, F_SETFL, flags | O_NONBLOCK) == -1) {
            perror("Could not set socket to non-blocking mode");
            return 1;
    }

    Connection* conn = arena_allocate(conn_arena, sizeof(Connection));
    if (!conn) {
        perror("Failed to allocate memory to register connection");
        return 1;
    }
    conn->compression = FALSE;
    conn->sockfd = peerfd;
    conn->state = STATE_HANDSHAKE;
    conn->arena = arena_create(1 << 16);
    struct epoll_event event_in = {.events = EPOLLIN | EPOLLET, .data.ptr = conn};
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, peerfd, &event_in) == -1) {
        perror("Failed to handle connection");
        return 1;
    }
    char addr_str[16];
    inet_ntop(AF_INET, &peer_addr.sin_addr, addr_str, 16);
    printf("Accepted connection from %s:%i.\n", addr_str, peer_addr.sin_port);
    return 0;
}

int net_handle(char* host, int port) {

    Arena conn_arena = arena_create(4096);

    int epollfd;
    int serverfd;

    int ret = net_init(host, port, &serverfd, &epollfd);
    if (ret)
        return ret;

    struct epoll_event events[10];
    int eventCount = 0;

    while ((eventCount = epoll_wait(epollfd, events, 10, -1)) != -1) {
        for (int i = 0; i < eventCount; i++) {
            struct epoll_event* e = &events[i];
            if (e->data.u64 == 0)
                accept_connection(&conn_arena, serverfd, epollfd);
            else
                on_packet_recv(e->data.ptr);
        }
    }

    close(serverfd);
    arena_destroy(&conn_arena);

    return 0;
}
