#include "udp_receiver.hpp"
#include "logger.hpp"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>

namespace packet_engine {

UDPReceiver::UDPReceiver(std::string bind_ip,
                         uint16_t port,
                         ThreadSafeQueue<Packet>& queue,
                         MetricsCollector& metrics,
                         int socket_rcvbuf_bytes)
    : PacketReceiver(std::move(bind_ip), port, queue, metrics),
      socket_rcvbuf_bytes_(socket_rcvbuf_bytes) {}

UDPReceiver::~UDPReceiver() {
    stop();
}

void UDPReceiver::start() {
    if (is_running_.exchange(true)) {
        return;
    }

    stop_requested_.store(false);

    int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        LOG_ERROR("UDPReceiver failed to create socket: ", strerror(errno));
        is_running_.store(false);
        return;
    }
    server_socket_.reset(fd);

    socket_utils::setReuseAddr(server_socket_.get(), true);
    socket_utils::setReusePort(server_socket_.get(), true);
    socket_utils::setRecvBufferSize(server_socket_.get(), socket_rcvbuf_bytes_);
    socket_utils::setRecvTimeout(server_socket_.get(), 100); // 100ms timeout for responsive stop check

    struct sockaddr_in bind_addr{};
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_port = htons(port_);
    if (inet_pton(AF_INET, bind_ip_.c_str(), &bind_addr.sin_addr) <= 0) {
        bind_addr.sin_addr.s_addr = INADDR_ANY;
    }

    if (::bind(server_socket_.get(), reinterpret_cast<struct sockaddr*>(&bind_addr), sizeof(bind_addr)) < 0) {
        LOG_ERROR("UDPReceiver failed to bind to ", bind_ip_, ":", port_, " - ", strerror(errno));
        server_socket_.reset();
        is_running_.store(false);
        return;
    }

    LOG_INFO("UDPReceiver listening on ", bind_ip_, ":", port_,
             " (SO_RCVBUF: ", socket_rcvbuf_bytes_ / 1024, " KB)");

    receiver_thread_ = std::thread(&UDPReceiver::receiveLoop, this);
}

void UDPReceiver::stop() {
    if (!is_running_.load()) {
        return;
    }

    stop_requested_.store(true);
    if (receiver_thread_.joinable()) {
        receiver_thread_.join();
    }
    server_socket_.reset();
    is_running_.store(false);
    LOG_INFO("UDPReceiver stopped.");
}

void UDPReceiver::receiveLoop() {
    std::vector<uint8_t> buffer(65536);
    struct sockaddr_in client_addr{};
    socklen_t addr_len = sizeof(client_addr);

    uint64_t fallback_id = 1;

    while (!stop_requested_.load(std::memory_order_relaxed)) {
        ssize_t bytes_read = ::recvfrom(server_socket_.get(),
                                        buffer.data(),
                                        buffer.size(),
                                        0,
                                        reinterpret_cast<struct sockaddr*>(&client_addr),
                                        &addr_len);

        if (bytes_read < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                continue; // Timeout expired, loop and check stop_requested_
            }
            LOG_WARN("UDP recvfrom error: ", strerror(errno));
            continue;
        }

        if (bytes_read == 0) {
            continue;
        }

        metrics_.recordReceived(static_cast<size_t>(bytes_read));

        Packet packet;
        bool deserialized = Packet::deserialize(buffer.data(), static_cast<size_t>(bytes_read), packet);

        if (!deserialized) {
            // Raw frame fallback
            char src_ip_str[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(client_addr.sin_addr), src_ip_str, INET_ADDRSTRLEN);

            std::vector<uint8_t> payload(buffer.begin(), buffer.begin() + bytes_read);
            packet = Packet(fallback_id++,
                            std::string(src_ip_str),
                            bind_ip_,
                            ntohs(client_addr.sin_port),
                            port_,
                            IpProtocol::UDP,
                            0,
                            std::move(payload));
        }

        auto enq_tp = std::chrono::high_resolution_clock::now();
        uint64_t enq_ns = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(enq_tp.time_since_epoch()).count());
        packet.setEnqueueTimestampNs(enq_ns);

        // Try push to bounded queue; if full, record as dropped (Backpressure / Drop policy)
        if (!queue_.try_push(std::move(packet))) {
            metrics_.recordDropped();
        }
    }
}

} // namespace packet_engine
