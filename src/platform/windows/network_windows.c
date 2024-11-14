//
// Created by bmorino on 14/11/2024.
//

#ifdef MC_PLATFORM_WINDOWS

#include "platform/network.h"

bool sock_is_valid(Socket socket) {
    return socket != INVALID_SOCKET;
}

i64 sock_recv(Socket socket, void* buf, u64 buf_len) {
    WSABUF wsabuf = {
        .len = buf_len,
        .buf = buf,
    };
    unsigned long bytes;
    WSAOVERLAPPED overlapped = {};
    i32 res = WSARecv(socket, &wsabuf, 1l, &bytes, 0, &overlapped, NULL);

    if(res != 0) {
        i32 error = WSAGetLastError();
        return error == WSA_IO_PENDING ? SOCKIO_PENDING : SOCKIO_ERROR;
    }

    return bytes;
}
i64 sock_recv_buf(Socket socket, ByteBuffer* output) {
    WSABUF regions[2];
    u64 size = 0;
    u64 total_size = 0;
    i32 region_count = 0;
    while(size < output->capacity - output->size) {
        void* cast_ptr = &regions[region_count].buf;
        size = bytebuf_contiguous_write(output, cast_ptr);
        regions[region_count].len = size;
        total_size += size;
        region_count++;
    }

    if(region_count == 0)
        return 0;

    unsigned long bytes = 0;
    WSAOVERLAPPED overlapped = {};
    i32 res = WSARecv(socket, regions, region_count, &bytes, 0, &overlapped, NULL);
    if(res != 0) {
        i32 error = WSAGetLastError();
        if(error == WSA_IO_PENDING)
            return SOCKIO_PENDING;
        bytebuf_unwrite(output, size);
        return SOCKIO_ERROR;
    }

    bytebuf_unwrite(output, size - bytes);
    return bytes;
}

i64 sock_send(Socket socket, const void* buf, u64 buf_len) {
    WSABUF wsabuf = {
        .len = buf_len,
        .buf = buf,
    };
    unsigned long bytes = 0;
    WSAOVERLAPPED overlapped = {};
    i32 res = WSASend(socket, &wsabuf, 1l, &bytes, 0, &overlapped, NULL);
    if(res != 0) {
        i32 error = WSAGetLastError();
        return error == WSA_IO_PENDING ? SOCKIO_PENDING : SOCKIO_ERROR;
    }

    return bytes;
}
i64 sock_send_buf(Socket socket, ByteBuffer* input) {
    WSABUF regions[2];
    u64 size = 0;
    u64 total_size = 0;
    i32 region_count = 0;
    while(size < input->size) {
        void* cast_ptr = &regions[region_count].buf;
        size = bytebuf_contiguous_read(input, cast_ptr);
        regions[region_count].len = size;
        total_size += size;
        region_count++;
    }

    if(region_count == 0)
        return 0;

    unsigned long bytes = 0;
    WSAOVERLAPPED overlapped = {};
    i32 res = WSARecv(socket, regions, region_count, &bytes, 0, &overlapped, NULL);
    if(res != 0) {
        i32 error = WSAGetLastError();
        if(error == WSA_IO_PENDING)
            return SOCKIO_PENDING;
        bytebuf_unread(input, total_size);
        return SOCKIO_ERROR;
    }

    bytebuf_unread(input, size - bytes);
    return bytes;
}

bool sock_bind(Socket socket, SocketAddress* address) {
    return bind(socket, &address->data.sa, address->length) == 0;
}
bool sock_listen(Socket socket, i32 backlog) {
    return listen(socket, backlog) == 0;
}
Socket sock_accept(Socket socket) {
    Socket accepted = WSASocketA(AF_INET, SOCK_STREAM, 0, 0, 0, 0);
    bool success = AcceptEx(socket, accepted, )
}


#endif