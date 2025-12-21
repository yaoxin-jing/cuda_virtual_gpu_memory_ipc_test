#include "ipc_socket.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>
#include <cstdio>
#include <cerrno>

constexpr const char* SOCKET_PATH = "/tmp/cuda_vmm_test.sock";

IPCSocket::IPCSocket() : socket_fd_(-1), connection_fd_(-1) {}

IPCSocket::~IPCSocket() {
    close_connection();
    unlink(SOCKET_PATH);
}

int IPCSocket::create_and_listen() {
    // Remove old socket file if exists
    unlink(SOCKET_PATH);

    // Create Unix domain socket
    socket_fd_ = socket(AF_UNIX, SOCK_STREAM, 0);
    if (socket_fd_ < 0) {
        fprintf(stderr, "Failed to create socket: %s\n", strerror(errno));
        return -1;
    }

    // Bind to socket path
    struct sockaddr_un addr = {};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (bind(socket_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "Failed to bind socket: %s\n", strerror(errno));
        ::close(socket_fd_);
        return -1;
    }

    // Listen for connections
    if (listen(socket_fd_, 1) < 0) {
        fprintf(stderr, "Failed to listen on socket: %s\n", strerror(errno));
        ::close(socket_fd_);
        return -1;
    }

    return 0;
}

int IPCSocket::accept_connection() {
    connection_fd_ = accept(socket_fd_, NULL, NULL);
    if (connection_fd_ < 0) {
        fprintf(stderr, "Failed to accept connection: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

int IPCSocket::connect_to_server() {
    // Create socket
    socket_fd_ = socket(AF_UNIX, SOCK_STREAM, 0);
    if (socket_fd_ < 0) {
        fprintf(stderr, "Failed to create socket: %s\n", strerror(errno));
        return -1;
    }

    // Connect to server
    struct sockaddr_un addr = {};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (connect(socket_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "Failed to connect to server: %s\n", strerror(errno));
        ::close(socket_fd_);
        return -1;
    }

    connection_fd_ = socket_fd_;
    return 0;
}

int IPCSocket::send_fd(int fd) {
    struct msghdr msg = {};
    struct cmsghdr *cmsg;
    char buf[CMSG_SPACE(sizeof(int))];
    char dummy = 'X';
    struct iovec io = {.iov_base = &dummy, .iov_len = sizeof(dummy)};

    msg.msg_iov = &io;
    msg.msg_iovlen = 1;
    msg.msg_control = buf;
    msg.msg_controllen = sizeof(buf);

    cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));
    memcpy(CMSG_DATA(cmsg), &fd, sizeof(int));

    if (sendmsg(connection_fd_, &msg, 0) < 0) {
        fprintf(stderr, "Failed to send FD: %s\n", strerror(errno));
        return -1;
    }

    return 0;
}

int IPCSocket::recv_fd(int& fd) {
    struct msghdr msg = {};
    struct cmsghdr *cmsg;
    char buf[CMSG_SPACE(sizeof(int))];
    char dummy;
    struct iovec io = {.iov_base = &dummy, .iov_len = sizeof(dummy)};

    msg.msg_iov = &io;
    msg.msg_iovlen = 1;
    msg.msg_control = buf;
    msg.msg_controllen = sizeof(buf);

    if (recvmsg(connection_fd_, &msg, 0) < 0) {
        fprintf(stderr, "Failed to receive FD: %s\n", strerror(errno));
        return -1;
    }

    cmsg = CMSG_FIRSTHDR(&msg);
    if (cmsg == NULL || cmsg->cmsg_type != SCM_RIGHTS) {
        fprintf(stderr, "Invalid control message\n");
        return -1;
    }

    memcpy(&fd, CMSG_DATA(cmsg), sizeof(int));
    return 0;
}

int IPCSocket::send_metadata(size_t size) {
    if (send(connection_fd_, &size, sizeof(size), 0) != sizeof(size)) {
        fprintf(stderr, "Failed to send metadata: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

int IPCSocket::recv_metadata(size_t& size) {
    if (recv(connection_fd_, &size, sizeof(size), 0) != sizeof(size)) {
        fprintf(stderr, "Failed to receive metadata: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

int IPCSocket::send_ack() {
    char ack = 'A';
    if (send(connection_fd_, &ack, 1, 0) != 1) {
        fprintf(stderr, "Failed to send ACK: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

int IPCSocket::wait_ack() {
    char ack;
    if (recv(connection_fd_, &ack, 1, 0) != 1) {
        fprintf(stderr, "Failed to receive ACK: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

void IPCSocket::close_connection() {
    if (connection_fd_ >= 0 && connection_fd_ != socket_fd_) {
        ::close(connection_fd_);
        connection_fd_ = -1;
    }
    if (socket_fd_ >= 0) {
        ::close(socket_fd_);
        socket_fd_ = -1;
    }
}
