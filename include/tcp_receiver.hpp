#pragma once

#include "packet_receiver.hpp"
#include "socket_utils.hpp"
#include <thread>
#include <vector>
#include <mutex>

namespace packet_engine {

class TCPReceiver : public PacketReceiver {
public:
    TCPReceiver(std::string bind_ip,
                uint16_t port,
                ThreadSafeQueue<Packet>& queue,
                MetricsCollector& metrics,
                int max_clients = 32);

    ~TCPReceiver() override;

    void start() override;
    void stop() override;

private:
    void acceptLoop();
    void handleClient(int client_fd, std::string client_ip, uint16_t client_port);

    int max_clients_;
    SocketHandle server_socket_;
    std::thread accept_thread_;
    std::vector<std::thread> client_threads_;
    std::mutex client_threads_mutex_;
};

} // namespace packet_engine
