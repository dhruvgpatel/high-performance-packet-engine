#pragma once

#include "packet.hpp"
#include "thread_safe_queue.hpp"
#include "metrics_collector.hpp"

#include <string>
#include <memory>
#include <atomic>

namespace packet_engine {

class PacketReceiver {
public:
    PacketReceiver(std::string bind_ip,
                   uint16_t port,
                   ThreadSafeQueue<Packet>& queue,
                   MetricsCollector& metrics);

    virtual ~PacketReceiver() = default;

    PacketReceiver(const PacketReceiver&) = delete;
    PacketReceiver& operator=(const PacketReceiver&) = delete;

    virtual void start() = 0;
    virtual void stop() = 0;

    virtual bool isRunning() const noexcept {
        return is_running_.load(std::memory_order_relaxed);
    }

    const std::string& getBindIp() const noexcept { return bind_ip_; }
    uint16_t getPort() const noexcept { return port_; }

protected:
    std::string bind_ip_;
    uint16_t port_;
    ThreadSafeQueue<Packet>& queue_;
    MetricsCollector& metrics_;
    std::atomic<bool> is_running_{false};
    std::atomic<bool> stop_requested_{false};
};

} // namespace packet_engine
