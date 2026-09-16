#include "packet.hpp"
#include "socket_utils.hpp"
#include "logger.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <random>
#include <cstring>
#include <atomic>
#include <iomanip>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

using namespace packet_engine;

struct GeneratorConfig {
    std::string protocol{"udp"};
    std::string target_ip{"127.0.0.1"};
    uint16_t target_port{9000};
    uint64_t total_packets{100000};
    size_t payload_size{512};
    uint64_t rate_pps{0}; // 0 = unlimited max speed
    uint32_t duration_sec{0};
    uint32_t num_threads{1};
    bool randomize_ports{true};
    bool burst_mode{false};
    uint32_t burst_size{100};
};

void printHelp(const char* progName) {
    std::cout << "Usage: " << progName << " [options]\n"
              << "Options:\n"
              << "  --protocol <udp|tcp>      Protocol mode (default: udp)\n"
              << "  --ip <address>            Target IP address (default: 127.0.0.1)\n"
              << "  --port <port>             Target Port (default: 9000)\n"
              << "  --count <number>          Total packets to send (default: 100000)\n"
              << "  --size <bytes>            Payload size in bytes (default: 512)\n"
              << "  --rate <pps>              Target rate in packets per second (0 = max speed)\n"
              << "  --duration <seconds>      Duration to run in seconds (overrides count)\n"
              << "  --threads <count>         Number of generator threads (default: 1)\n"
              << "  --burst                   Enable burst transmission mode\n"
              << "  --burst-size <count>      Number of packets per burst (default: 100)\n"
              << "  --help                    Show this help message\n";
}

GeneratorConfig parseArgs(int argc, char* argv[]) {
    GeneratorConfig cfg;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--protocol" && i + 1 < argc) {
            cfg.protocol = argv[++i];
        } else if (arg == "--ip" && i + 1 < argc) {
            cfg.target_ip = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            cfg.target_port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--count" && i + 1 < argc) {
            cfg.total_packets = static_cast<uint64_t>(std::stoull(argv[++i]));
        } else if (arg == "--size" && i + 1 < argc) {
            cfg.payload_size = static_cast<size_t>(std::stoul(argv[++i]));
        } else if (arg == "--rate" && i + 1 < argc) {
            cfg.rate_pps = static_cast<uint64_t>(std::stoull(argv[++i]));
        } else if (arg == "--duration" && i + 1 < argc) {
            cfg.duration_sec = static_cast<uint32_t>(std::stoul(argv[++i]));
        } else if (arg == "--threads" && i + 1 < argc) {
            cfg.num_threads = static_cast<uint32_t>(std::stoul(argv[++i]));
        } else if (arg == "--burst") {
            cfg.burst_mode = true;
        } else if (arg == "--burst-size" && i + 1 < argc) {
            cfg.burst_size = static_cast<uint32_t>(std::stoul(argv[++i]));
        } else if (arg == "--help" || arg == "-h") {
            printHelp(argv[0]);
            std::exit(0);
        }
    }
    return cfg;
}

void runUdpGenerator(const GeneratorConfig& cfg,
                     uint32_t thread_id,
                     uint64_t packets_to_send,
                     std::atomic<uint64_t>& sent_counter,
                     std::atomic<uint64_t>& bytes_counter,
                     std::atomic<bool>& stop_flag) {
    
    int sock = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        LOG_ERROR("Thread #", thread_id, " failed to create UDP socket: ", strerror(errno));
        return;
    }
    SocketHandle sock_handle(sock);
    socket_utils::setSendBufferSize(sock_handle.get(), 4 * 1024 * 1024);

    struct sockaddr_in target_addr{};
    target_addr.sin_family = AF_INET;
    target_addr.sin_port = htons(cfg.target_port);
    inet_pton(AF_INET, cfg.target_ip.c_str(), &target_addr.sin_addr);

    std::mt19937_64 rng(1337 + thread_id);
    std::uniform_int_distribution<uint16_t> port_dist(1024, 65535);
    std::uniform_int_distribution<uint8_t> prio_dist(0, 255);
    std::uniform_int_distribution<uint8_t> byte_dist(0, 255);

    std::vector<uint8_t> base_payload(cfg.payload_size);
    for (size_t i = 0; i < cfg.payload_size; ++i) {
        base_payload[i] = byte_dist(rng);
    }

    uint64_t local_sent = 0;
    uint64_t thread_rate = (cfg.rate_pps > 0) ? (cfg.rate_pps / cfg.num_threads) : 0;
    auto interval = (thread_rate > 0) ? std::chrono::nanoseconds(1000000000ULL / thread_rate) : std::chrono::nanoseconds(0);

    auto start_time = std::chrono::high_resolution_clock::now();
    auto next_send_time = start_time;

    while (!stop_flag.load(std::memory_order_relaxed) && (cfg.duration_sec > 0 || local_sent < packets_to_send)) {
        uint64_t packet_id = (static_cast<uint64_t>(thread_id) << 48) | (local_sent + 1);
        uint16_t src_port = cfg.randomize_ports ? port_dist(rng) : static_cast<uint16_t>(50000 + thread_id);
        uint8_t priority = prio_dist(rng);

        Packet packet(packet_id,
                      "192.168.1.100",
                      cfg.target_ip,
                      src_port,
                      cfg.target_port,
                      IpProtocol::UDP,
                      priority,
                      base_payload);

        std::vector<uint8_t> wire_data = packet.serialize();

        ssize_t sent = ::sendto(sock_handle.get(),
                                wire_data.data(),
                                wire_data.size(),
                                0,
                                reinterpret_cast<struct sockaddr*>(&target_addr),
                                sizeof(target_addr));

        if (sent > 0) {
            local_sent++;
            sent_counter.fetch_add(1, std::memory_order_relaxed);
            bytes_counter.fetch_add(static_cast<uint64_t>(sent), std::memory_order_relaxed);
        }

        if (thread_rate > 0) {
            next_send_time += interval;
            while (std::chrono::high_resolution_clock::now() < next_send_time) {
                // Short spin-wait for high precision rate control
            }
        }
    }
}

void runTcpGenerator(const GeneratorConfig& cfg,
                     uint32_t thread_id,
                     uint64_t packets_to_send,
                     std::atomic<uint64_t>& sent_counter,
                     std::atomic<uint64_t>& bytes_counter,
                     std::atomic<bool>& stop_flag) {
    
    int sock = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        LOG_ERROR("Thread #", thread_id, " failed to create TCP socket: ", strerror(errno));
        return;
    }
    SocketHandle sock_handle(sock);

    struct sockaddr_in target_addr{};
    target_addr.sin_family = AF_INET;
    target_addr.sin_port = htons(cfg.target_port);
    inet_pton(AF_INET, cfg.target_ip.c_str(), &target_addr.sin_addr);

    if (::connect(sock_handle.get(), reinterpret_cast<struct sockaddr*>(&target_addr), sizeof(target_addr)) < 0) {
        LOG_ERROR("Thread #", thread_id, " failed to connect TCP socket to ", cfg.target_ip, ":", cfg.target_port, " - ", strerror(errno));
        return;
    }

    std::mt19937_64 rng(2024 + thread_id);
    std::uniform_int_distribution<uint16_t> port_dist(1024, 65535);
    std::uniform_int_distribution<uint8_t> prio_dist(0, 255);
    std::uniform_int_distribution<uint8_t> byte_dist(0, 255);

    std::vector<uint8_t> base_payload(cfg.payload_size);
    for (size_t i = 0; i < cfg.payload_size; ++i) {
        base_payload[i] = byte_dist(rng);
    }

    uint64_t local_sent = 0;
    uint64_t thread_rate = (cfg.rate_pps > 0) ? (cfg.rate_pps / cfg.num_threads) : 0;
    auto interval = (thread_rate > 0) ? std::chrono::nanoseconds(1000000000ULL / thread_rate) : std::chrono::nanoseconds(0);
    auto next_send_time = std::chrono::high_resolution_clock::now();

    while (!stop_flag.load(std::memory_order_relaxed) && (cfg.duration_sec > 0 || local_sent < packets_to_send)) {
        uint64_t packet_id = (static_cast<uint64_t>(thread_id) << 48) | (local_sent + 1);
        uint16_t src_port = cfg.randomize_ports ? port_dist(rng) : static_cast<uint16_t>(50000 + thread_id);
        uint8_t priority = prio_dist(rng);

        Packet packet(packet_id,
                      "192.168.1.100",
                      cfg.target_ip,
                      src_port,
                      cfg.target_port,
                      IpProtocol::TCP,
                      priority,
                      base_payload);

        std::vector<uint8_t> wire_data = packet.serialize();

        ssize_t sent = ::send(sock_handle.get(), wire_data.data(), wire_data.size(), 0);
        if (sent > 0) {
            local_sent++;
            sent_counter.fetch_add(1, std::memory_order_relaxed);
            bytes_counter.fetch_add(static_cast<uint64_t>(sent), std::memory_order_relaxed);
        } else if (sent < 0) {
            LOG_WARN("TCP send error: ", strerror(errno));
            break;
        }

        if (thread_rate > 0) {
            next_send_time += interval;
            while (std::chrono::high_resolution_clock::now() < next_send_time) {
            }
        }
    }
}

int main(int argc, char* argv[]) {
    GeneratorConfig cfg = parseArgs(argc, argv);

    std::cout << "=======================================================\n"
              << "         PACKET GENERATOR TRAFFIC INJECTOR              \n"
              << "=======================================================\n"
              << " Target           : " << cfg.target_ip << ":" << cfg.target_port << " (" << cfg.protocol << ")\n"
              << " Threads          : " << cfg.num_threads << "\n"
              << " Packet Payload   : " << cfg.payload_size << " bytes (Frame: " << cfg.payload_size + sizeof(WireHeader) << " bytes)\n"
              << " Target Rate      : " << (cfg.rate_pps > 0 ? std::to_string(cfg.rate_pps) + " pps" : "Max Speed (Unlimited)") << "\n";
    if (cfg.duration_sec > 0) {
        std::cout << " Duration         : " << cfg.duration_sec << " seconds\n";
    } else {
        std::cout << " Total Count      : " << cfg.total_packets << " packets\n";
    }
    std::cout << "=======================================================\n";

    std::atomic<uint64_t> sent_counter{0};
    std::atomic<uint64_t> bytes_counter{0};
    std::atomic<bool> stop_flag{false};

    std::vector<std::thread> workers;
    workers.reserve(cfg.num_threads);

    uint64_t per_thread_packets = cfg.total_packets / cfg.num_threads;

    auto start_tp = std::chrono::high_resolution_clock::now();

    for (uint32_t i = 0; i < cfg.num_threads; ++i) {
        if (cfg.protocol == "tcp" || cfg.protocol == "TCP") {
            workers.emplace_back(runTcpGenerator, cfg, i, per_thread_packets,
                                 std::ref(sent_counter), std::ref(bytes_counter), std::ref(stop_flag));
        } else {
            workers.emplace_back(runUdpGenerator, cfg, i, per_thread_packets,
                                 std::ref(sent_counter), std::ref(bytes_counter), std::ref(stop_flag));
        }
    }

    if (cfg.duration_sec > 0) {
        std::this_thread::sleep_for(std::chrono::seconds(cfg.duration_sec));
        stop_flag.store(true);
    }

    for (auto& w : workers) {
        if (w.joinable()) {
            w.join();
        }
    }

    auto end_tp = std::chrono::high_resolution_clock::now();
    double total_sec = std::chrono::duration<double>(end_tp - start_tp).count();
    if (total_sec <= 0.0001) total_sec = 0.0001;

    uint64_t total_sent = sent_counter.load();
    uint64_t total_bytes = bytes_counter.load();
    double pps = static_cast<double>(total_sent) / total_sec;
    double mbps = (static_cast<double>(total_bytes) * 8.0) / (total_sec * 1000000.0);

    std::cout << "\n=======================================================\n"
              << "               GENERATION SUMMARY                       \n"
              << "=======================================================\n"
              << " Total Sent       : " << total_sent << " packets (" << (static_cast<double>(total_bytes) / 1024.0 / 1024.0) << " MB)\n"
              << " Elapsed Time     : " << std::fixed << std::setprecision(3) << total_sec << " s\n"
              << " Actual Rate      : " << std::fixed << std::setprecision(0) << pps << " pps ("
              << std::setprecision(2) << mbps << " Mbps)\n"
              << "=======================================================\n";

    return 0;
}
