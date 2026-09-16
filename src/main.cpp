#include "packet.hpp"
#include "thread_safe_queue.hpp"
#include "udp_receiver.hpp"
#include "tcp_receiver.hpp"
#include "worker_thread_pool.hpp"
#include "packet_validator.hpp"
#include "packet_classifier.hpp"
#include "packet_processor.hpp"
#include "metrics_collector.hpp"
#include "logger.hpp"
#include "web_dashboard_server.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <csignal>
#include <fstream>
#include <sstream>
#include <chrono>
#include <thread>
#include <atomic>

using namespace packet_engine;

namespace {
std::atomic<bool> g_shutdown_requested{false};

void signalHandler(int signum) {
    (void)signum;
    g_shutdown_requested.store(true, std::memory_order_relaxed);
}

void setupSignalHandlers() {
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
#ifdef SIGPIPE
    std::signal(SIGPIPE, SIG_IGN); // Ignore broken pipe on TCP disconnects
#endif
}
} // namespace

struct ServerOptions {
    std::string protocol{"udp"};
    std::string ip{"0.0.0.0"};
    uint16_t port{9000};
    size_t worker_threads{4};
    size_t queue_capacity{10000};
    uint32_t duration_seconds{0};
    uint32_t metrics_interval_sec{2};
    std::string log_level{"INFO"};
    std::string config_file{""};
    std::string metrics_output_json{""};
    size_t compute_iterations{50};
    uint32_t simulated_delay_us{0};
    bool enable_web{true};
    uint16_t web_port{8080};
};

void printUsage(const char* prog) {
    std::cout << "Usage: " << prog << " [options]\n"
              << "Options:\n"
              << "  --protocol <udp|tcp|both>   Transport protocol to ingest (default: udp)\n"
              << "  --ip <bind_ip>              IP address to bind (default: 0.0.0.0)\n"
              << "  --port <port>               Port number (default: 9000)\n"
              << "  --workers <count>           Worker thread pool size (default: 4)\n"
              << "  --queue-size <capacity>     Bounded queue max capacity (default: 10000)\n"
              << "  --duration <seconds>        Run duration in seconds (0 = run forever)\n"
              << "  --metrics-interval <sec>    Metrics display interval (default: 2s)\n"
              << "  --metrics-output <path>     JSON output file for final metrics snapshot\n"
              << "  --compute-iters <n>         Workload CPU iterations per packet (default: 50)\n"
              << "  --delay-us <us>             Simulated processing delay in microseconds (default: 0)\n"
              << "  --web-port <port>           Web Telemetry Dashboard port (default: 8080)\n"
              << "  --no-web                    Disable embedded Web Dashboard HTTP server\n"
              << "  --log-level <DEBUG|INFO|WARN|ERROR> Log level (default: INFO)\n"
              << "  --config <path>             JSON configuration file\n"
              << "  --help                      Show this help message\n";
}

void parseJsonConfig(const std::string& filepath, ServerOptions& opts) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        LOG_WARN("Could not open configuration file: ", filepath);
        return;
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());

    auto extractString = [&content](const std::string& key) -> std::string {
        size_t pos = content.find("\"" + key + "\"");
        if (pos == std::string::npos) return "";
        size_t colon = content.find(':', pos);
        if (colon == std::string::npos) return "";
        size_t q1 = content.find('"', colon);
        if (q1 == std::string::npos) return "";
        size_t q2 = content.find('"', q1 + 1);
        if (q2 == std::string::npos) return "";
        return content.substr(q1 + 1, q2 - q1 - 1);
    };

    auto extractInt = [&content](const std::string& key) -> int64_t {
        size_t pos = content.find("\"" + key + "\"");
        if (pos == std::string::npos) return -1;
        size_t colon = content.find(':', pos);
        if (colon == std::string::npos) return -1;
        size_t start = content.find_first_of("-0123456789", colon);
        if (start == std::string::npos) return -1;
        size_t end = content.find_first_not_of("-0123456789", start);
        std::string num_str = content.substr(start, end - start);
        return std::stoll(num_str);
    };

    std::string proto = extractString("protocol");
    if (!proto.empty()) opts.protocol = proto;

    std::string ip = extractString("ip");
    if (!ip.empty()) opts.ip = ip;

    int64_t port = extractInt("port");
    if (port > 0) opts.port = static_cast<uint16_t>(port);

    int64_t workers = extractInt("worker_threads");
    if (workers > 0) opts.worker_threads = static_cast<size_t>(workers);

    int64_t qcap = extractInt("queue_capacity");
    if (qcap > 0) opts.queue_capacity = static_cast<size_t>(qcap);

    int64_t minterval = extractInt("metrics_interval_seconds");
    if (minterval > 0) opts.metrics_interval_sec = static_cast<uint32_t>(minterval);

    std::string lvl = extractString("log_level");
    if (!lvl.empty()) opts.log_level = lvl;

    int64_t iters = extractInt("simulated_workload_iterations");
    if (iters >= 0) opts.compute_iterations = static_cast<size_t>(iters);

    int64_t delay = extractInt("simulated_delay_us");
    if (delay >= 0) opts.simulated_delay_us = static_cast<uint32_t>(delay);

    int64_t wport = extractInt("web_port");
    if (wport > 0) opts.web_port = static_cast<uint16_t>(wport);

    LOG_INFO("Loaded configuration from ", filepath);
}

ServerOptions parseCommandLine(int argc, char* argv[]) {
    ServerOptions opts;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--config" && i + 1 < argc) {
            opts.config_file = argv[++i];
            parseJsonConfig(opts.config_file, opts);
        }
    }

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--protocol" && i + 1 < argc) {
            opts.protocol = argv[++i];
        } else if (arg == "--ip" && i + 1 < argc) {
            opts.ip = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            opts.port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--workers" && i + 1 < argc) {
            opts.worker_threads = static_cast<size_t>(std::stoul(argv[++i]));
        } else if (arg == "--queue-size" && i + 1 < argc) {
            opts.queue_capacity = static_cast<size_t>(std::stoul(argv[++i]));
        } else if (arg == "--duration" && i + 1 < argc) {
            opts.duration_seconds = static_cast<uint32_t>(std::stoul(argv[++i]));
        } else if (arg == "--metrics-interval" && i + 1 < argc) {
            opts.metrics_interval_sec = static_cast<uint32_t>(std::stoul(argv[++i]));
        } else if (arg == "--metrics-output" && i + 1 < argc) {
            opts.metrics_output_json = argv[++i];
        } else if (arg == "--compute-iters" && i + 1 < argc) {
            opts.compute_iterations = static_cast<size_t>(std::stoul(argv[++i]));
        } else if (arg == "--delay-us" && i + 1 < argc) {
            opts.simulated_delay_us = static_cast<uint32_t>(std::stoul(argv[++i]));
        } else if (arg == "--web-port" && i + 1 < argc) {
            opts.web_port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--no-web") {
            opts.enable_web = false;
        } else if (arg == "--log-level" && i + 1 < argc) {
            opts.log_level = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            std::exit(0);
        }
    }
    return opts;
}

int main(int argc, char* argv[]) {
    setupSignalHandlers();

    ServerOptions opts = parseCommandLine(argc, argv);
    Logger::getInstance().setLevel(stringToLogLevel(opts.log_level));

    std::cout << "\n=======================================================\n"
              << "   HIGH-PERFORMANCE NETWORK PACKET PROCESSING ENGINE   \n"
              << "=======================================================\n"
              << " Ingest Protocol    : " << opts.protocol << "\n"
              << " Bind Address       : " << opts.ip << ":" << opts.port << "\n"
              << " Worker Threads     : " << opts.worker_threads << "\n"
              << " Queue Capacity     : " << opts.queue_capacity << " packets\n"
              << " Workload Iterations: " << opts.compute_iterations << "\n"
              << " Simulated Delay    : " << opts.simulated_delay_us << " us\n"
              << " Web Dashboard      : " << (opts.enable_web ? ("Enabled (http://" + (opts.ip == "0.0.0.0" ? "localhost" : opts.ip) + ":" + std::to_string(opts.web_port) + ")") : "Disabled") << "\n"
              << " Log Level          : " << opts.log_level << "\n";
    if (opts.duration_seconds > 0) {
        std::cout << " Run Duration       : " << opts.duration_seconds << " seconds\n";
    }
    std::cout << "=======================================================\n\n";

    // 1. Initialize Core Components
    ThreadSafeQueue<Packet> queue(opts.queue_capacity);
    MetricsCollector metrics(opts.worker_threads);

    auto validator = std::make_shared<PacketValidator>(0, 65507, true);
    auto classifier = std::make_shared<PacketClassifier>();
    auto processor = std::make_shared<PacketProcessor>(true, opts.compute_iterations, opts.simulated_delay_us);

    // 2. Start Worker Pool
    WorkerThreadPool worker_pool(opts.worker_threads, queue, metrics, processor, validator, classifier);
    worker_pool.start();

    // 3. Start Web Dashboard Server (if enabled)
    std::unique_ptr<WebDashboardServer> web_server;
    if (opts.enable_web) {
        web_server = std::make_unique<WebDashboardServer>(
            opts.ip, opts.web_port, metrics, queue, opts.protocol, opts.port, opts.worker_threads
        );
        web_server->start();
    }

    // 4. Start Receivers
    std::vector<std::unique_ptr<PacketReceiver>> receivers;
    if (opts.protocol == "udp" || opts.protocol == "both") {
        receivers.push_back(std::make_unique<UDPReceiver>(opts.ip, opts.port, queue, metrics));
    }
    if (opts.protocol == "tcp" || opts.protocol == "both") {
        uint16_t tcp_port = (opts.protocol == "both") ? static_cast<uint16_t>(opts.port + 1) : opts.port;
        receivers.push_back(std::make_unique<TCPReceiver>(opts.ip, tcp_port, queue, metrics));
    }

    for (auto& r : receivers) {
        r->start();
    }

    LOG_INFO("Packet Engine is running and ready for traffic. Press Ctrl+C to terminate.");

    // 5. Main Event Loop / Periodic Reporting
    auto start_time = std::chrono::steady_clock::now();
    auto next_report_time = start_time + std::chrono::seconds(opts.metrics_interval_sec);

    while (!g_shutdown_requested.load(std::memory_order_relaxed)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        auto now = std::chrono::steady_clock::now();
        if (opts.duration_seconds > 0 &&
            std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count() >= opts.duration_seconds) {
            LOG_INFO("Duration limit (", opts.duration_seconds, "s) reached. Initiating shutdown...");
            break;
        }

        if (now >= next_report_time) {
            MetricsSnapshot s = metrics.getSnapshot();
            LOG_INFO("[Live Stats] Ingested: ", s.total_received,
                     " | Processed: ", s.total_processed,
                     " | Dropped: ", s.dropped_packets,
                     " | Rate: ", static_cast<uint64_t>(s.throughput_pps), " pps",
                     " | Avg Latency: ", static_cast<uint64_t>(s.avg_latency_us), " us",
                     " | Queue Size: ", queue.size());
            next_report_time = now + std::chrono::seconds(opts.metrics_interval_sec);
        }
    }

    // 6. Graceful Shutdown Sequence
    LOG_INFO("\n--- Initiating 9-Step Graceful Shutdown Protocol ---");

    LOG_INFO("[Step 1/9] Stop accepting new packets: Halting receivers...");
    for (auto& r : receivers) {
        r->stop();
    }

    LOG_INFO("[Step 2/9] Receivers stopped. Queued packets remaining: ", queue.size());

    LOG_INFO("[Step 3/9] Signalling ThreadSafeQueue shutdown...");
    queue.shutdown();

    LOG_INFO("[Step 4/9] Waiting for worker pool to drain and join...");
    worker_pool.stop();

    LOG_INFO("[Step 5/9] All worker threads joined cleanly.");

    LOG_INFO("[Step 6/9] Stopping Web Dashboard server...");
    if (web_server) {
        web_server->stop();
    }

    LOG_INFO("[Step 7/9] Collecting final metrics snapshot...");
    std::string report = metrics.generateSummaryString();
    std::cout << report << std::endl;

    if (!opts.metrics_output_json.empty()) {
        LOG_INFO("Writing JSON metrics report to: ", opts.metrics_output_json);
        std::ofstream jfile(opts.metrics_output_json);
        if (jfile.is_open()) {
            jfile << metrics.generateJsonReport() << "\n";
            jfile.close();
        } else {
            LOG_ERROR("Failed to write metrics output file: ", opts.metrics_output_json);
        }
    }

    LOG_INFO("[Step 8/9] Releasing socket resources and memory pools.");
    receivers.clear();

    LOG_INFO("[Step 9/9] Shutdown complete. Exiting with code 0.");
    return 0;
}
