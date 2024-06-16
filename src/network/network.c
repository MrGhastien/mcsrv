#include "network.h"
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include "connection.h"
#include "receiver.h"
#include "sender.h"

typedef struct net_ctx {
    Arena* arena;
    Connection* connections;
    u64 max_connections;
    u64 connection_count;
    int serverfd;
    int epollfd;
} NetworkContext;

static NetworkContext ctx;

static int net_init(char* host, int port, Arena* arena, u64 max_connections) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    ctx.serverfd = sockfd;
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
    ctx.epollfd = epollfd;
    if (epollfd == -1) {
        perror("Failed to create epoll instance");
        return 1;
    }

    struct epoll_event event_in = {.events = EPOLLIN | EPOLLET, .data.fd = -1};
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd, &event_in) == -1) {
        perror("Failed to create epoll instance");
        return 1;
    }

    ctx.arena = arena;
    ctx.connections = arena_allocate(ctx.arena, sizeof(Connection) * max_connections);
    ctx.max_connections = max_connections;
    ctx.connection_count = 0;

    for(u64 i = 0; i < max_connections; i++) {
        ctx.connections[i].sockfd = -1;
    }
    
    puts("Network sub-system initialized.");
    return 0;
}

static int accept_connection(void) {
    struct sockaddr_in peer_addr;
    socklen_t peer_addr_length = sizeof peer_addr;
    int peerfd = accept(ctx.serverfd, (struct sockaddr*)&peer_addr, &peer_addr_length);
    if (peerfd == -1) {
        perror("Invalid connection received");
        return 1;
    }
    int flags = fcntl(peerfd, F_GETFL, 0);
    if (fcntl(peerfd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("Could not set socket to non-blocking mode");
        return 1;
    }

    i64 idx = -1;
    for(u64 i = 0; i < ctx.max_connections; i++) {
        if(ctx.connections[i].sockfd == -1) {
            idx = i;
            break;
        }
    }
    if(idx == -1) {
        fputs("Max number of connections reached! Try again later.", stderr);
        return 1;
    }


    struct epoll_event event_in = {.events = EPOLLIN | EPOLLOUT | EPOLLET, .data.u64 = idx};
    if (epoll_ctl(ctx.epollfd, EPOLL_CTL_ADD, peerfd, &event_in) == -1) {
        perror("Failed to handle connection");
        return 1;
    }

    ctx.connections[idx] = conn_create(peerfd, idx);

    char addr_str[16];
    inet_ntop(AF_INET, &peer_addr.sin_addr, addr_str, 16);
    printf("Accepted connection from %s:%i.\n", addr_str, peer_addr.sin_port);
    return 0;
}

void close_connection(Connection* conn) {
    struct epoll_event placeholder;
    close(conn->sockfd);
    epoll_ctl(ctx.epollfd, EPOLL_CTL_DEL, conn->sockfd, &placeholder);
    arena_destroy(&conn->arena);
    conn->sockfd = -1;
    ctx.connection_count--;
}

int net_handle(char* host, int port, u64 max_connections) {

    Arena conn_arena = arena_create(40960);

    int ret = net_init(host, port, &conn_arena, max_connections);
    if (ret)
        return ret;

    struct epoll_event events[10];
    int eventCount = 0;

    printf("Listening on %s:%i.\n", host, port);
    while ((eventCount = epoll_wait(ctx.epollfd, events, 10, -1)) != -1) {
        for (int i = 0; i < eventCount; i++) {
            struct epoll_event* e = &events[i];
            if (e->data.fd == -1)
                accept_connection();
            else {
                enum IOCode code = IOC_OK;
                Connection* conn = &ctx.connections[e->data.u64];
                if (e->events & EPOLLIN)
                    code = receive_packet(conn);
                switch (code) {
                case IOC_CLOSED:
                    puts("Peer closed connection.");
                    close_connection(conn);
                    break;
                case IOC_ERROR:
                    puts("Errored connection.");
                    close_connection(conn);
                    break;
                default:
                    break;
                }

                if (e->events & EPOLLOUT)
                    sender_send(conn);

                switch (code) {
                case IOC_CLOSED:
                case IOC_ERROR:
                    close_connection(conn);
                    break;
                default:
                    break;
                }
            }
        }
    }

    close(ctx.serverfd);
    close(ctx.epollfd);
    arena_destroy(&conn_arena);

    return 0;
}
