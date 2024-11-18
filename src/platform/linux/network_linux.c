//
// Created by bmorino on 14/11/2024.
//

#ifdef MC_PLATFORM_LINUX

void sockaddr_init(SocketAddress* addr, u16 family) {
    addr->data.family = family;
    switch (family) {
           case AF_INET:
               addr->length = sizeof addr->data.ip4;
               break;
           case AF_INET6:
               addr->length = sizeof addr->data.ip6;
               break;
           default:
               addr->length = sizeof addr->data.sa;
               break;
    }
}

enum IOCode try_send(Socket socket, void* data, u64 size, u64* out_sent) {
    u64 sent = 0;
    enum IOCode code = IOC_OK;
    while (sent < size) {
        void* begin = offset(data, sent);
        i64 res = send(socket, begin, size - sent, 0);

        if (res == -1) {
            code = errno == EAGAIN || errno == EWOULDBLOCK ? IOC_AGAIN : IOC_ERROR;
            break;
        } else if (res == 0) {
            code = IOC_CLOSED;
            break;
        }

        sent += res;
    }
    *out_sent = sent;
    return code;
}

i32 create_server_socket(NetworkContext* ctx, char* host, i32 port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    ctx.serverfd = sockfd;
    if (sockfd == -1) {
        log_fatalf("Failed to create the server socket: %s", strerror(errno));
        return 1;
    }

    int option = 1;
    // Let the socket reuse the address, to avoid errors when quickly rerunning the server
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof option);

    struct sockaddr_in saddr = {0};
    saddr.sin_family = AF_INET;
    if (!inet_aton(host, &saddr.sin_addr)) {
        log_fatal("The server address is invalid.");
        return 2;
    }

    saddr.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr*) &saddr, sizeof saddr) == -1) {
        log_fatal("Could not bind server socket.");
        return 3;
    }

    if (listen(sockfd, 0) == -1) {
        log_fatal("Socket cannot listen");
        return 4;
    }
    return 0;
}

i32 network_platform_init(NetworkContext* ctx) {
    int epollfd = epoll_create1(0);
    ctx.epollfd = epollfd;
    if (epollfd == -1) {
        log_fatalf("Failed to create the epoll instance: %s", strerror(errno));
        return 1;
    }

    struct epoll_event event_in = {.events = EPOLLIN | EPOLLET, .data.fd = -1};
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, ctx.serverfd, &event_in) == -1) {
        log_fatalf("Could not register the server socket to epoll: %s", strerror(errno));
        return 1;
    }

    ctx.eventfd = eventfd(0, 0);
    if (ctx.eventfd == -1) {
        log_fatalf("Failed to create the stopping event file descriptor: %s", strerror(errno));
        return 1;
    }

    event_in.data.fd = ctx.eventfd;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, ctx.eventfd, &event_in) == -1) {
        log_fatalf("Could not register the event file descriptor to epoll: %s", strerror(errno));
        return 1;
    }
    return 0;
}

i32 accept_connection(void) {
    struct sockaddr_in peer_addr;
    socklen_t peer_addr_length = sizeof peer_addr;
    int peerfd = accept(ctx.serverfd, (struct sockaddr*) &peer_addr, &peer_addr_length);
    if (peerfd == -1) {
        log_error("Invalid connection received.");
        return 1;
    }
    int flags = fcntl(peerfd, F_GETFL, 0);
    if (fcntl(peerfd, F_SETFL, flags | O_NONBLOCK) == -1) {
        log_error("Could not set connection socket to non-blocking mode.");
        return 1;
    }

    i64 idx = -1;
    for (u64 i = 0; i < ctx.max_connections; i++) {
        if (ctx.connections[i].sockfd == -1) {
            idx = i;
            break;
        }
    }
    if (idx == -1) {
        fputs("Max number of connections reached! Try again later.", stderr);
        return 1;
    }

    struct epoll_event event_in = {.events = EPOLLIN | EPOLLOUT | EPOLLET, .data.u64 = idx};
    if (epoll_ctl(ctx.epollfd, EPOLL_CTL_ADD, peerfd, &event_in) == -1) {
        log_error("Could not register the connection inside the network loop.");
        return 1;
    }

    char addr_str[16];
    inet_ntop(AF_INET, &peer_addr.sin_addr, addr_str, 16);

    ctx.connections[idx] =
        conn_create(peerfd, idx, &ctx.enc_ctx, str_create_const(addr_str), peer_addr.sin_port);

    log_infof("Accepted connection from %s:%i.", addr_str, peer_addr.sin_port);
    return 0;
}

void close_connection(Connection* conn) {
    struct epoll_event placeholder;
    log_infof("Closing connection %i.", conn->sockfd);

    if (conn->encryption) {
        encryption_cleanup_peer(&conn->peer_enc_ctx);
    }

    close(conn->sockfd);
    epoll_ctl(ctx.epollfd, EPOLL_CTL_DEL, conn->sockfd, &placeholder);
    arena_destroy(&conn->scratch_arena);
    arena_destroy(&conn->persistent_arena);
    mcmutex_destroy(&conn->mutex);
    conn->sockfd = -1;
    ctx.connection_count--;
}

static void* network_handle(void* params) {
    (void) params;
    struct epoll_event events[10];
    i32 eventCount = 0;

    mcthread_set_name("network");

    sigset_t sigmask;
    sigfillset(&sigmask);
    sigdelset(&sigmask, SIGTERM);

    log_infof("Listening for connections on %s:%u...", ctx.host.base, ctx.port);
    while (ctx.should_continue) {
        eventCount = epoll_wait(ctx.epollfd, events, 10, -1);
        for (i32 i = 0; i < eventCount; i++) {
            struct epoll_event* e = &events[i];
            if (e->data.fd == -1)
                accept_connection();
            else if (e->data.fd == ctx.eventfd)
                ctx.should_continue = FALSE;
            else {
                Connection* conn = &ctx.connections[e->data.u64];
                ctx.code = handle_connection_io(conn, e->events);
            }
        }
    }

    if (eventCount == -1) {
        switch (errno) {
        case EINTR:
            break;
        default:
            log_fatalf("Network error: %s", strerror(errno));
            break;
        }
    }

    network_finish();
    return NULL;
}

#endif