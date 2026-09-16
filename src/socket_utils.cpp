#include "socket_utils.hpp"

#include <sys/time.h>
#include <cstring>

namespace packet_engine {
namespace socket_utils {

bool setNonBlocking(int fd, bool non_blocking) {
    if (fd < 0) return false;
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return false;
    if (non_blocking) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }
    return fcntl(fd, F_SETFL, flags) != -1;
}

bool setReuseAddr(int fd, bool reuse) {
    if (fd < 0) return false;
    int optval = reuse ? 1 : 0;
    return setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == 0;
}

bool setReusePort(int fd, bool reuse) {
    if (fd < 0) return false;
#ifdef SO_REUSEPORT
    int optval = reuse ? 1 : 0;
    return setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval)) == 0;
#else
    (void)reuse;
    return true;
#endif
}

bool setRecvBufferSize(int fd, int size_bytes) {
    if (fd < 0) return false;
    return setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &size_bytes, sizeof(size_bytes)) == 0;
}

bool setSendBufferSize(int fd, int size_bytes) {
    if (fd < 0) return false;
    return setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &size_bytes, sizeof(size_bytes)) == 0;
}

bool setRecvTimeout(int fd, int timeout_ms) {
    if (fd < 0) return false;
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    return setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == 0;
}

} // namespace socket_utils
} // namespace packet_engine
