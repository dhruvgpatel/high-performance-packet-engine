#pragma once

#include <string>
#include <cstdint>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

namespace packet_engine {

/**
 * @brief RAII wrapper for POSIX socket file descriptors.
 * Ensures sockets are never leaked even if exceptions or early returns occur.
 */
class SocketHandle {
public:
    SocketHandle() noexcept : fd_(-1) {}
    explicit SocketHandle(int fd) noexcept : fd_(fd) {}
    ~SocketHandle() { reset(); }

    SocketHandle(const SocketHandle&) = delete;
    SocketHandle& operator=(const SocketHandle&) = delete;

    SocketHandle(SocketHandle&& other) noexcept : fd_(other.fd_) {
        other.fd_ = -1;
    }

    SocketHandle& operator=(SocketHandle&& other) noexcept {
        if (this != &other) {
            reset();
            fd_ = other.fd_;
            other.fd_ = -1;
        }
        return *this;
    }

    int get() const noexcept { return fd_; }
    bool isValid() const noexcept { return fd_ >= 0; }

    int release() noexcept {
        int tmp = fd_;
        fd_ = -1;
        return tmp;
    }

    void reset(int new_fd = -1) noexcept {
        if (fd_ >= 0) {
            ::close(fd_);
        }
        fd_ = new_fd;
    }

private:
    int fd_{-1};
};

namespace socket_utils {

bool setNonBlocking(int fd, bool non_blocking = true);
bool setReuseAddr(int fd, bool reuse = true);
bool setReusePort(int fd, bool reuse = true);
bool setRecvBufferSize(int fd, int size_bytes);
bool setSendBufferSize(int fd, int size_bytes);
bool setRecvTimeout(int fd, int timeout_ms);

} // namespace socket_utils

} // namespace packet_engine
