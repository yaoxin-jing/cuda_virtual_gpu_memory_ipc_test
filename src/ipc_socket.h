#pragma once

#include <cstddef>

class IPCSocket {
public:
    IPCSocket();
    ~IPCSocket();

    // Server side (producer)
    int create_and_listen();
    int accept_connection();

    // Client side (consumer)
    int connect_to_server();

    // File descriptor passing via SCM_RIGHTS
    int send_fd(int fd);
    int recv_fd(int& fd);

    // Metadata (size) passing
    int send_metadata(size_t size);
    int recv_metadata(size_t& size);

    // Synchronization
    int send_ack();
    int wait_ack();

    void close_connection();

private:
    int socket_fd_;
    int connection_fd_;
};
