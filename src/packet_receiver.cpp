#include "packet_receiver.hpp"

namespace packet_engine {

PacketReceiver::PacketReceiver(std::string bind_ip,
                               uint16_t port,
                               ThreadSafeQueue<Packet>& queue,
                               MetricsCollector& metrics)
    : bind_ip_(std::move(bind_ip)),
      port_(port),
      queue_(queue),
      metrics_(metrics) {}

} // namespace packet_engine
