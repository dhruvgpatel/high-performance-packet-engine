#include "tcp_receiver.hpp"
#include "logger.hpp"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>

namespace packet_engine {

TCPReceiver::TCPReceiver(std::string bind_ip,
                         uint16_t port,
                         ThreadSafeQueue<Packet>& queue,
                         MetricsCollector& metrics,
                         int max_clients)
    : PacketReceiver(std::move(bind_ip), port, queue, metrics),
      max_clients_(max_clients) {}

TCPReceiver::~TCPReceiver() {
    stop();
}

void TCPReceiver::start() {
    if (is_running_.exchange(true)) {
        return;
    }

    stop_requested_.store(false);

    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        LOG_ERROR("TCPReceiver failed to create socket: ", strerror(errno));
        is_running_.store(false);
        return;
    }
    server_socket_.reset(fd);

    socket_utils::setReuseAddr(server_socket_.get(), true);
    socket_utils::setReusePort(server_socket_.get(), true);
    socket_utils::setRecvTimeout(server_socket_.get(), 200); // 200ms accept timeout

    struct sockaddr_in bind_addr{};
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_port = htons(port_);
    if (inet_pton(AF_INET, bind_ip_.c_str(), &bind_addr.sin_addr) <= 0) {
        bind_addr.sin_addr.s_addr = INADDR_ANY;
    }

    if (::bind(server_socket_.get(), reinterpret_cast<struct sockaddr*>(&bind_addr), sizeof(bind_addr)) < 0) {
        LOG_ERROR("TCPReceiver failed to bind to ", bind_ip_, ":", port_, " - ", strerror(errno));
        server_socket_.reset();
        is_running_.store(false);
        return;
    }

    if (::listen(server_socket_.get(), max_clients_) < 0) {
        LOG_ERROR("TCPReceiver failed to listen on port ", port_, " - ", strerror(errno));
        server_socket_.reset();
        is_running_.store(false);
        return;
    }

    LOG_INFO("TCPReceiver listening on ", bind_ip_, ":", port_);
    accept_thread_ = std::thread(&TCPReceiver::acceptLoop, this);
}

void TCPReceiver::stop() {
    if (!is_running_.load()) {
        return;
    }

    stop_requested_.store(true);
    if (server_socket_.isValid()) {
        ::shutdown(server_socket_.get(), SHUT_RDWR);
    }

    if (accept_thread_.joinable()) {
        accept_thread_.join();
    }

    {
        std::lock_guard<std::mutex> lock(client_threads_mutex_);
        for (auto& th : client_threads_) {
            if (th.joinable()) {
                th.join();
            }
        }
        client_threads_.clear();
    }

    server_socket_.reset();
    is_running_.store(false);
    LOG_INFO("TCPReceiver stopped.");
}

void TCPReceiver::acceptLoop() {
    while (!stop_requested_.load(std::memory_order_relaxed)) {
        struct sockaddr_in client_addr{};
        socklen_t addr_len = sizeof(client_addr);

        int client_fd = ::accept(server_socket_.get(),
                                 reinterpret_cast<struct sockaddr*>(&client_addr),
                                 &addr_len);

        if (client_fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                continue; // Timeout
            }
            if (stop_requested_.load()) break;
            LOG_WARN("TCP accept error: ", strerror(errno));
            continue;
        }

        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(client_addr.sin_addr), ip_str, sizeof(ip_str));
        uint16_t client_port = ntohs(client_addr.sin_port);

        LOG_DEBUG("Accepted TCP connection from ", ip_str, ":", client_port);

        std::lock_guard<std::mutex> lock(client_threads_mutex_);
        client_threads_.emplace_back(&TCPReceiver::handleClient, this, client_fd, std::string(ip_str), client_port);
    }
}

void TCPReceiver::handleClient(int client_fd, std::string client_ip, uint16_t client_port) {
    SocketHandle client_sock(client_fd);
    socket_utils::setRecvTimeout(client_sock.get(), 200);

    std::vector<uint8_t> stream_buf;
    stream_buf.reserve(65536);

    uint8_t read_chunk[8192];

    while (!stop_requested_.load(std::memory_order_relaxed)) {
        ssize_t bytes_read = ::recv(client_sock.get(), read_chunk, sizeof(read_chunk), 0);

        if (bytes_read < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                continue; // Check stop_requested
            }
            break; // Connection error or closed
        }

        if (bytes_read == 0) {
            LOG_DEBUG("TCP connection closed by client ", client_ip, ":", client_port);
            break;
        }

        metrics_.recordReceived(static_cast<size_t>(bytes_read));
        stream_buf.insert(stream_buf.end(), read_chunk, read_chunk + bytes_read);

        // Frame extraction loop
        while (stream_buf.size() >= sizeof(WireHeader)) {
            WireHeader header;
            std::memcpy(&header, stream_buf.data(), sizeof(WireHeader));

            if (ntohl(header.magic) != 0x504B5445) {
                // Out of sync: scan forward for magic byte
                stream_buf.erase(stream_buf.begin());
                continue;
            }

            uint16_t payload_len = ntohs(header.payload_size);
            size_t total_frame_len = sizeof(WireHeader) + payload_len;

            if (stream_buf.size() < total_frame_len) {
                // Incomplete frame, wait for more bytes
                break;
            }

            Packet packet;
            if (Packet::deserialize(stream_buf.data(), total_frame_len, packet)) {
                auto enq_tp = std::chrono::high_resolution_clock::now();
                uint64_t enq_ns = static_cast<uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(enq_tp.time_since_epoch()).count());
                packet.setEnqueueTimestampNs(enq_ns);

                if (!queue_.try_push(std::move(packet))) {
                    metrics_.recordDropped();
                }
            }

            stream_buf.erase(stream_buf.begin(), stream_buf.begin() + static_cast<ptrdiff_t>(total_frame_len));
        }
    }
}

} // namespace packet_engine
