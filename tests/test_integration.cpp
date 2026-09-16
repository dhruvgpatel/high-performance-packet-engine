#include <gtest/gtest.h>
#include "packet.hpp"
#include "thread_safe_queue.hpp"
#include "udp_receiver.hpp"
#include "worker_thread_pool.hpp"
#include "metrics_collector.hpp"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <chrono>
#include <thread>

using namespace packet_engine;

TEST(IntegrationTest, EndToEndUdpIngestAndProcess) {
    const uint16_t test_port = 9991;
    const size_t num_packets = 500;

    ThreadSafeQueue<Packet> queue(10000);
    MetricsCollector metrics(2);

    UDPReceiver receiver("127.0.0.1", test_port, queue, metrics);
    WorkerThreadPool pool(2, queue, metrics);

    pool.start();
    receiver.start();

    std::this_thread::sleep_for(std::chrono::milliseconds(50)); // Allow receiver to bind

    // Client sender
    int client_sock = ::socket(AF_INET, SOCK_DGRAM, 0);
    ASSERT_GE(client_sock, 0);

    struct sockaddr_in target_addr{};
    target_addr.sin_family = AF_INET;
    target_addr.sin_port = htons(test_port);
    inet_pton(AF_INET, "127.0.0.1", &target_addr.sin_addr);

    for (size_t i = 1; i <= num_packets; ++i) {
        Packet pkt(i, "127.0.0.1", "127.0.0.1", 12345, test_port, IpProtocol::UDP, 0, {0x11, 0x22, 0x33});
        std::vector<uint8_t> data = pkt.serialize();
        ssize_t sent = ::sendto(client_sock, data.data(), data.size(), 0,
                                reinterpret_cast<struct sockaddr*>(&target_addr), sizeof(target_addr));
        EXPECT_GT(sent, 0);
    }

    ::close(client_sock);

    // Wait for in-flight packets
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    receiver.stop();
    pool.stop();

    MetricsSnapshot s = metrics.getSnapshot();
    EXPECT_EQ(s.total_received, num_packets);
    EXPECT_EQ(s.total_processed, num_packets);
    EXPECT_EQ(s.valid_packets, num_packets);
    EXPECT_EQ(s.dropped_packets, 0);
}
