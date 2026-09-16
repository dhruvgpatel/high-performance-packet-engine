# High-Performance Multithreaded Network Packet Processing Engine

[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![Build Status](https://img.shields.io/badge/build-passing-brightgreen.svg)]()
[![Tests](https://img.shields.io/badge/GoogleTest-21%2F21%20Passed-success.svg)]()
[![Sanitizers](https://img.shields.io/badge/Sanitizers-ASan%20%7C%20TSan%20Clean-brightgreen.svg)]()
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

A carrier-grade, low-latency, multithreaded network packet ingestion and processing engine built in **Modern C++17** and **POSIX Sockets**. 

Designed to model real-world edge router data planes, high-frequency trading (HFT) ingestion pipelines, and deep packet inspection (DPI) firewalls.

---

## 🌟 Key Features

- **High-Throughput Ingestion:** Dual-mode network ingestion supporting non-blocking UDP datagrams (`SO_RCVBUF` up to 4MB) and framed TCP stream connections.
- **Embedded Real-Time Web Dashboard:** Built-in HTTP telemetry server (`http://localhost:8080`) streaming live rolling throughput charts, p50/p95/p99 tail latency meters, per-worker core load balancing, and queue backpressure gauges.
- **Cache-Conscious Thread-Safe Queue:** Bounded MPMC queue with `std::mutex` and dual `std::condition_variable` synchronization, backpressure handling, and $\mathcal{O}(1)$ move semantics.
- **Worker Thread Pool:** Configurable pool of consumer worker threads eliminating runtime thread-creation overhead and maximizing CPU cache locality.
- **Zero False Sharing:** Telemetry counters padded to 64-byte CPU cache-line boundaries (`alignas(64)`) using lock-free `std::atomic<uint64_t>` with `std::memory_order_relaxed`.
- **Nanosecond Tail Latency Telemetry:** Lockless reservoir sub-sampling calculating exact p50 (median), p95, and p99 tail latencies without serializing the hot path.
- **Deterministic Compute Workload:** Integrated frame validation, 5-tuple QoS classification, CRC32 checksums, and synthetic compute hashing without artificial thread sleeps.
- **Atomic 9-Step Graceful Shutdown:** Deterministic teardown protocol ensuring zero in-flight packet loss, clean socket reclamation, and zero deadlocks.
- **Comprehensive Quality Assurance:** 21 automated unit and integration tests using **GoogleTest**, validated under **AddressSanitizer (ASan)** and **ThreadSanitizer (TSan)**.
- **Automated Benchmarking & Visualization:** Python automation suite (Pandas + Matplotlib) generating publication-ready charts and performance reports.
- **CI/CD & Containerization:** Multi-stage **Dockerfile**, `docker-compose.yml`, and **GitHub Actions** multi-compiler matrix pipeline.

---

## 🏗️ System Architecture

```
                    +------------------------------------+
                    |        packet_generator (CLI)      |
                    |   (Burst / Constant / Multi-Rate)  |
                    +-----------------+------------------+
                                      |
                         [UDP / TCP Transport Wire]
                                      |
                                      v
+===================================================================================+
| packet_engine (Server Process)                                                    |
|                                                                                   |
|   +-----------------------+                    +-----------------------+          |
|   |      UDPReceiver      |                    |      TCPReceiver      |          |
|   |  (Non-blocking POSIX) |                    |   (Framed Streams)    |          |
|   +-----------+-----------+                    +-----------+-----------+          |
|               |                                            |                      |
|               +--------------------+-----------------------+                      |
|                                    | (Zero-Copy Move Semantics)                   |
|                                    v                                              |
|                   +----------------------------------+                            |
|                   |      ThreadSafeQueue<Packet>     |                            |
|                   |   (Bounded Mutex + CV Predicate) |                            |
|                   +----------------+-----------------+                            |
|                                    |                                              |
|                 +------------------+------------------+                           |
|                 |                  |                  |                           |
|                 v                  v                  v                           |
|          +--------------+   +--------------+   +--------------+                   |
|          | Worker Th #0 |   | Worker Th #1 |   | Worker Th #N | (WorkerThreadPool)|
|          +------+-------+   +------+-------+   +------+-------+                   |
|                 |                  |                  |                           |
|                 +------------------+------------------+                           |
|                                    |                                              |
|                                    v                                              |
|                   +----------------------------------+                            |
|                   |         PacketValidator          |                            |
|                   |   (Length, Header, Port Check)   |                            |
|                   +----------------+-----------------+                            |
|                                    |                                              |
|                                    v                                              |
|                   +----------------------------------+                            |
|                   |         PacketClassifier         |                            |
|                   |  (5-Tuple, Priority, QoS Tiers)  |                            |
|                   +----------------+-----------------+                            |
|                                    |                                              |
|                                    v                                              |
|                   +----------------------------------+                            |
|                   |         PacketProcessor          |                            |
|                   |  (CRC32 / Payload Bitwise Hash)  |                            |
|                   +----------------+-----------------+                            |
|                                    |                                              |
|                                    v                                              |
|                   +----------------------------------+                            |
|                   |         MetricsCollector         |                            |
|                   | (Lockless Atomix + p50/p95/p99)  |                            |
|                   +----------------+-----------------+                            |
|                                    |                                              |
+====================================|==============================================+
                                     v
                  +--------------------------------------+
                  |   Console Logger / JSON Reports      |
                  +------------------+-------------------+
                                     |
                                     v
                  +--------------------------------------+
                  |     Python Matplotlib Visualizer     |
                  +--------------------------------------+
```

---

## ⚡ Quick Start

### 1. Build the Engine and Tests
```bash
# Configure and build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON
cmake --build build -j

# Run the GoogleTest suite (21/21 tests)
ctest --test-dir build --output-on-failure
```

### 2. Start the Packet Processing Engine
```bash
# Run server with 4 worker threads on UDP port 9000 (Web Dashboard enabled on port 8080)
./build/packet_engine --protocol udp --ip 127.0.0.1 --port 9000 --workers 4 --queue-size 10000
```
> 🌐 **Live Web Dashboard:** Open **`http://localhost:8080`** in your browser to monitor live ingestion graphs, nanosecond latency percentiles, and worker thread load in real time!

### 3. Inject Traffic using the Packet Generator
```bash
# Transmit 100,000 UDP packets at 25,000 packets/sec with 512-byte payloads
./build/packet_generator --protocol udp --ip 127.0.0.1 --port 9000 --count 100000 --rate 25000 --size 512
```

---

## 🧪 Running Unit & Integration Tests

```bash
# Run all GoogleTest test suites
./build/tests/unit_tests
```

### Memory & Thread Safety Sanitizers:
```bash
# AddressSanitizer (ASan)
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DENABLE_ASAN=ON -DBUILD_TESTS=ON
cmake --build build-asan -j && ctest --test-dir build-asan --output-on-failure

# ThreadSanitizer (TSan)
cmake -S . -B build-tsan -DCMAKE_BUILD_TYPE=Debug -DENABLE_TSAN=ON -DBUILD_TESTS=ON
cmake --build build-tsan -j && ctest --test-dir build-tsan --output-on-failure
```

---

## 📊 Automated Benchmarks & Visualizations

Run the complete 5-phase benchmark suite:
```bash
# Execute automated benchmark experiments (Exp A through E)
python3 scripts/run_benchmark.py

# Generate statistical markdown analysis
python3 scripts/analyze_results.py

# Generate publication-quality plots in results/plots/
python3 scripts/plot_results.py
```

Generated plots:
1. `worker_scaling_throughput.png`: Throughput & Speedup vs Worker Thread Count
2. `worker_scaling_latency.png`: Latency percentiles (Average, p50, p95, p99) vs Concurrency
3. `queue_size_impact.png`: Bounded Queue Capacity vs Dropped Packets & Backpressure
4. `packet_size_throughput.png`: Payload Size (64B–4096B) vs PPS and Bandwidth (Mbps)
5. `protocol_comparison.png`: UDP Datagram vs Framed TCP Ingestion

---

## 🐳 Docker Deployment

```bash
# Build Docker image
docker build -t packet_engine .

# Run container with Docker Compose
docker-compose up
```

---

## 📂 Project Structure

```
.
├── CMakeLists.txt              # Root CMake configuration with sanitizer support
├── Dockerfile                  # Multi-stage Ubuntu build & runtime container
├── docker-compose.yml          # Container orchestration
├── config/
│   └── config.json             # Server configuration file
├── include/                    # Public C++ Header Files
│   ├── packet.hpp              # Packet data model & wire protocol header
│   ├── thread_safe_queue.hpp   # Bounded MPMC thread-safe queue template
│   ├── socket_utils.hpp        # RAII SocketHandle & socket option helpers
│   ├── packet_receiver.hpp     # Abstract receiver interface
│   ├── udp_receiver.hpp        # High-throughput UDP datagram receiver
│   ├── tcp_receiver.hpp        # Framed TCP stream receiver
│   ├── worker_thread_pool.hpp  # Worker thread pool consumer
│   ├── packet_validator.hpp    # Packet integrity validation engine
│   ├── packet_classifier.hpp   # 5-tuple and QoS classifier
│   ├── packet_processor.hpp    # Compute workload and CRC32 engine
│   ├── metrics_collector.hpp   # Cacheline-padded atomic metrics & percentiles
│   └── logger.hpp              # Thread-safe level-filtered logging engine
├── src/                        # C++ Implementation Files
│   ├── main.cpp                # Server entrypoint & 9-step graceful shutdown
│   ├── packet.cpp              # Packet serialization / CRC32
│   ├── socket_utils.cpp        # POSIX socket tuning
│   ├── packet_receiver.cpp     # Base receiver implementation
│   ├── udp_receiver.cpp        # UDP recvfrom loop & queue dispatcher
│   ├── tcp_receiver.cpp        # TCP stream accept & framing parser
│   ├── worker_thread_pool.cpp  # Worker consumer loop & exception isolation
│   ├── packet_validator.cpp    # Validation rules
│   ├── packet_classifier.cpp   # QoS classification rules
│   ├── packet_processor.cpp    # CPU compute workload
│   ├── metrics_collector.cpp   # Lock-free counters & percentile calculations
│   └── logger.cpp              # Logger formatting & output streams
├── tools/
│   └── packet_generator.cpp    # Multi-threaded traffic generator CLI
├── tests/                      # GoogleTest Test Suites (21 Tests)
│   ├── CMakeLists.txt          # GTest FetchContent build configuration
│   ├── test_packet.cpp
│   ├── test_thread_safe_queue.cpp
│   ├── test_packet_validator.cpp
│   ├── test_packet_classifier.cpp
│   ├── test_packet_processor.cpp
│   ├── test_metrics.cpp
│   ├── test_worker_pool.cpp
│   └── test_integration.cpp
├── scripts/                    # Automation & Benchmarking
│   ├── run_benchmark.py        # Automated benchmark runner (Exp A-E)
│   ├── analyze_results.py      # Statistical analyzer & markdown generator
│   └── plot_results.py         # Matplotlib publication chart generator
├── docs/                       # Technical Documentation
│   ├── ARCHITECTURE.md         # Deep-dive architecture design document
│   ├── DESIGN_DECISIONS.md     # Concurrency rationale & trade-offs
│   ├── BENCHMARKS.md           # Benchmark methodology & metrics guide
│   ├── TESTING.md              # Testing guide and coverage analysis
│   ├── DEBUGGING.md            # GDB, ASan, TSan, Valgrind debugging workflows
│   └── INTERVIEW_QA.md         # 25 in-depth technical interview Q&A
└── .github/
    └── workflows/
        └── ci.yml              # GitHub Actions CI matrix workflow
```

---

## 💼 Technical Interview Preparation

A full interview master guide is available in [`docs/INTERVIEW_QA.md`](docs/INTERVIEW_QA.md), including:
- **60-Second Elevator Pitch**
- **2-Minute Technical Deep Dive**
- **25 Comprehensive Systems Engineering Q&As** (covering thread pools, condition variables, atomic ordering, cache false-sharing, graceful shutdown, backpressure, and kernel bypass architectures).

---

## 📜 License

Distributed under the MIT License. See `LICENSE` for more information.
