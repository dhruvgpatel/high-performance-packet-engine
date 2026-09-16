#include "web_dashboard_server.hpp"
#include "logger.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>

namespace packet_engine {

WebDashboardServer::WebDashboardServer(const std::string& bind_ip,
                                       uint16_t port,
                                       const MetricsCollector& metrics,
                                       const ThreadSafeQueue<Packet>& queue,
                                       const std::string& engine_protocol,
                                       uint16_t engine_port,
                                       size_t worker_count)
    : bind_ip_(bind_ip),
      port_(port),
      metrics_(metrics),
      queue_(queue),
      engine_protocol_(engine_protocol),
      engine_port_(engine_port),
      worker_count_(worker_count),
      start_time_(std::chrono::high_resolution_clock::now()) {}

WebDashboardServer::~WebDashboardServer() {
    stop();
}

bool WebDashboardServer::start() {
    if (is_running_.load()) {
        return true;
    }

    int raw_sock = ::socket(AF_INET, SOCK_STREAM, 0);
    if (raw_sock < 0) {
        LOG_ERROR("WebDashboardServer socket creation failed: ", strerror(errno));
        return false;
    }

    server_socket_.reset(raw_sock);
    socket_utils::setReuseAddr(server_socket_.get(), true);
    socket_utils::setReusePort(server_socket_.get(), true);
    socket_utils::setNonBlocking(server_socket_.get(), true);

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_);
    if (inet_pton(AF_INET, bind_ip_.c_str(), &addr.sin_addr) <= 0) {
        addr.sin_addr.s_addr = INADDR_ANY;
    }

    if (::bind(server_socket_.get(), reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        LOG_ERROR("WebDashboardServer failed to bind to ", bind_ip_, ":", port_, " - ", strerror(errno));
        server_socket_.reset();
        return false;
    }

    if (::listen(server_socket_.get(), 32) < 0) {
        LOG_ERROR("WebDashboardServer failed to listen on port ", port_, " - ", strerror(errno));
        server_socket_.reset();
        return false;
    }

    is_running_.store(true);
    stop_requested_.store(false);
    start_time_ = std::chrono::high_resolution_clock::now();

    server_thread_ = std::thread(&WebDashboardServer::acceptLoop, this);
    LOG_INFO("WebDashboardServer running at http://", (bind_ip_ == "0.0.0.0" ? "localhost" : bind_ip_), ":", port_);
    return true;
}

void WebDashboardServer::stop() {
    if (!is_running_.load()) {
        return;
    }

    stop_requested_.store(true);
    is_running_.store(false);

    if (server_socket_.isValid()) {
        ::shutdown(server_socket_.get(), SHUT_RDWR);
    }

    if (server_thread_.joinable()) {
        server_thread_.join();
    }

    server_socket_.reset();
    LOG_INFO("WebDashboardServer stopped.");
}

void WebDashboardServer::acceptLoop() {
    while (!stop_requested_.load(std::memory_order_relaxed)) {
        struct pollfd pfd{};
        pfd.fd = server_socket_.get();
        pfd.events = POLLIN;

        int ret = ::poll(&pfd, 1, 100); // 100ms timeout
        if (ret < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (ret == 0) {
            continue;
        }

        if (pfd.revents & (POLLIN | POLLPRI)) {
            struct sockaddr_in client_addr{};
            socklen_t addr_len = sizeof(client_addr);
            int client_fd = ::accept(server_socket_.get(),
                                     reinterpret_cast<struct sockaddr*>(&client_addr),
                                     &addr_len);
            if (client_fd >= 0) {
                handleClient(client_fd);
            }
        }
    }
}

void WebDashboardServer::handleClient(int client_fd) {
    SocketHandle client_sock(client_fd);
    socket_utils::setNonBlocking(client_sock.get(), false);
    socket_utils::setRecvTimeout(client_sock.get(), 2000); // 2 sec timeout

    char buffer[4096];
    ssize_t bytes_read = ::recv(client_sock.get(), buffer, sizeof(buffer) - 1, 0);
    if (bytes_read <= 0) {
        return;
    }
    buffer[bytes_read] = '\0';

    std::string req(buffer);
    std::string method, path;
    std::istringstream iss(req);
    iss >> method >> path;

    std::string response_body;
    std::string content_type = "text/plain";
    int status_code = 200;
    std::string status_text = "OK";

    if (path == "/" || path == "/index.html") {
        response_body = getDashboardHtml();
        content_type = "text/html; charset=utf-8";
    } else if (path == "/api/metrics" || path == "/api/stats") {
        response_body = buildJsonMetrics();
        content_type = "application/json";
    } else if (path == "/api/health") {
        response_body = "{\"status\":\"healthy\",\"uptime\":\"running\"}";
        content_type = "application/json";
    } else {
        status_code = 404;
        status_text = "Not Found";
        response_body = "404 Not Found";
    }

    std::ostringstream oss;
    oss << "HTTP/1.1 " << status_code << " " << status_text << "\r\n"
        << "Content-Type: " << content_type << "\r\n"
        << "Content-Length: " << response_body.size() << "\r\n"
        << "Access-Control-Allow-Origin: *\r\n"
        << "Access-Control-Allow-Methods: GET, OPTIONS\r\n"
        << "Access-Control-Allow-Headers: Content-Type\r\n"
        << "Connection: close\r\n"
        << "\r\n"
        << response_body;

    std::string http_resp = oss.str();
    ::send(client_sock.get(), http_resp.data(), http_resp.size(), 0);
}

std::string WebDashboardServer::buildJsonMetrics() const {
    auto snapshot = metrics_.getSnapshot();
    
    auto now = std::chrono::high_resolution_clock::now();
    double uptime_sec = std::chrono::duration<double>(now - start_time_).count();
    if (uptime_sec <= 0.0001) uptime_sec = 0.0001;

    double pps = static_cast<double>(snapshot.total_processed) / uptime_sec;
    double mbps = (static_cast<double>(snapshot.total_bytes_received) * 8.0) / (uptime_sec * 1000000.0);

    size_t cur_queue_size = queue_.size();
    size_t q_capacity = queue_.capacity();

    std::ostringstream oss;
    oss << "{\n"
        << "  \"elapsed_seconds\": " << uptime_sec << ",\n"
        << "  \"protocol\": \"" << engine_protocol_ << "\",\n"
        << "  \"port\": " << engine_port_ << ",\n"
        << "  \"worker_count\": " << worker_count_ << ",\n"
        << "  \"current_queue_size\": " << cur_queue_size << ",\n"
        << "  \"queue_capacity\": " << q_capacity << ",\n"
        << "  \"total_received\": " << snapshot.total_received << ",\n"
        << "  \"total_processed\": " << snapshot.total_processed << ",\n"
        << "  \"valid_packets\": " << snapshot.valid_packets << ",\n"
        << "  \"invalid_packets\": " << snapshot.invalid_packets << ",\n"
        << "  \"dropped_packets\": " << snapshot.dropped_packets << ",\n"
        << "  \"total_bytes_received\": " << snapshot.total_bytes_received << ",\n"
        << "  \"total_bytes_processed\": " << snapshot.total_bytes_processed << ",\n"
        << "  \"throughput_pps\": " << pps << ",\n"
        << "  \"throughput_mbps\": " << mbps << ",\n"
        << "  \"avg_latency_us\": " << snapshot.avg_latency_us << ",\n"
        << "  \"p50_latency_us\": " << snapshot.p50_latency_us << ",\n"
        << "  \"p95_latency_us\": " << snapshot.p95_latency_us << ",\n"
        << "  \"p99_latency_us\": " << snapshot.p99_latency_us << ",\n"
        << "  \"min_latency_us\": " << snapshot.min_latency_us << ",\n"
        << "  \"max_latency_us\": " << snapshot.max_latency_us << ",\n"
        << "  \"avg_queue_dwell_us\": " << snapshot.avg_queue_dwell_us << ",\n"
        << "  \"avg_processing_time_us\": " << snapshot.avg_processing_time_us << ",\n"
        << "  \"categories\": {\n"
        << "    \"data\": " << snapshot.data_packets << ",\n"
        << "    \"high_priority\": " << snapshot.high_priority_packets << ",\n"
        << "    \"control\": " << snapshot.control_packets << ",\n"
        << "    \"ack\": " << snapshot.ack_packets << "\n"
        << "  },\n"
        << "  \"worker_distribution\": [";

    for (size_t i = 0; i < snapshot.per_worker_processed.size(); ++i) {
        oss << snapshot.per_worker_processed[i];
        if (i + 1 < snapshot.per_worker_processed.size()) {
            oss << ", ";
        }
    }
    oss << "]\n}";

    return oss.str();
}

std::string WebDashboardServer::getDashboardHtml() const {
    // Attempt to load from web/index.html
    const std::vector<std::string> search_paths = {
        "web/index.html",
        "../web/index.html",
        "/Users/dhruv/Projects/High-Performance Multithreaded Network Packet Processing Engine/web/index.html"
    };

    for (const auto& path : search_paths) {
        std::ifstream file(path);
        if (file.is_open()) {
            return std::string((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());
        }
    }

    // Embedded fallback UI if file is not found
    return R"HTML(<!DOCTYPE html>
<html>
<head><title>Packet Engine Web Dashboard</title></head>
<body style="background:#0b0f19;color:#fff;font-family:sans-serif;padding:30px;">
  <h1>Packet Processing Engine Telemetry</h1>
  <p>Dashboard HTML loaded from embedded fallback. Please ensure <code>web/index.html</code> is present.</p>
  <div id="out"></div>
  <script>
    fetch('/api/metrics').then(r=>r.json()).then(d=>{
      document.getElementById('out').innerHTML = '<pre>' + JSON.stringify(d, null, 2) + '</pre>';
    });
  </script>
</body>
</html>)HTML";
}

} // namespace packet_engine
