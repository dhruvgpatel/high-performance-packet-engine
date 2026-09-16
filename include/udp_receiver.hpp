#pragma once

#include "packet_receiver.hpp"
#include "socket_utils.hpp"
#include <thread>
#include <vector>

namespace packet_engine {

class UDPReceiver : public PacketReceiver {
public:
    UDPReceiver(std::string bind_ip,
                uint16_t port,
                ThreadSafeQueue<Packet>& queue,
                MetricsCollector& metrics,
                int socket_rcvbuf_bytes = 4 * 1024 * 1024);

    ~UDPReceiver() override;

    void start() override;
    void stop() override;

private:
    void receiveLoop();

    int socket_rcvbuf_bytes_;
    SocketHandle server_socket_;
    std::thread receiver_thread_;
};

} // namespace packet_engine
