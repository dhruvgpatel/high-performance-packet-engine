#ifndef PACKET_ENGINE_WEB_DASHBOARD_SERVER_HPP
#define PACKET_ENGINE_WEB_DASHBOARD_SERVER_HPP

#include "metrics_collector.hpp"
#include "socket_utils.hpp"
#include "thread_safe_queue.hpp"
#include "packet.hpp"

#include <string>
#include <thread>
#include <atomic>
#include <memory>
#include <functional>

namespace packet_engine {

class WebDashboardServer {
public:
    WebDashboardServer(const std::string& bind_ip,
                       uint16_t port,
                       const MetricsCollector& metrics,
                       const ThreadSafeQueue<Packet>& queue,
                       const std::string& engine_protocol = "UDP",
                       uint16_t engine_port = 9000,
                       size_t worker_count = 4);

    ~WebDashboardServer();

    // Disable copy
    WebDashboardServer(const WebDashboardServer&) = delete;
    WebDashboardServer& operator=(const WebDashboardServer&) = delete;

    bool start();
    void stop();
    bool isRunning() const { return is_running_.load(std::memory_order_relaxed); }
    uint16_t getPort() const { return port_; }

private:
    void acceptLoop();
    void handleClient(int client_fd);
    std::string buildJsonMetrics() const;
    std::string getDashboardHtml() const;

    std::string bind_ip_;
    uint16_t port_;
    const MetricsCollector& metrics_;
    const ThreadSafeQueue<Packet>& queue_;
    std::string engine_protocol_;
    uint16_t engine_port_;
    size_t worker_count_;

    SocketHandle server_socket_;
    std::thread server_thread_;
    std::atomic<bool> is_running_{false};
    std::atomic<bool> stop_requested_{false};
    std::chrono::high_resolution_clock::time_point start_time_;
};

} // namespace packet_engine

#endif // PACKET_ENGINE_WEB_DASHBOARD_SERVER_HPP
