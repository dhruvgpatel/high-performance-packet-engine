# System Architecture Specification

## 1. High-Level Data Plane Overview

The **High-Performance Multithreaded Network Packet Processing Engine** is designed following the architectural principles of modern carrier-grade network gateways and high-frequency trading data planes.

```
                              [ Incoming Packet Stream ]
                                         │
                   ┌─────────────────────┴─────────────────────┐
                   │                                           │
             (UDP Datagrams)                             (TCP Streams)
                   ▼                                           ▼
          ┌─────────────────┐                         ┌─────────────────┐
          │   UDPReceiver   │                         │   TCPReceiver   │
          │ (Zero-copy SO)  │                         │ (Stream Framing)│
          └────────┬────────┘                         └────────┬────────┘
                   │                                           │
                   └─────────────────────┬─────────────────────┘
                                         │  (Move Semantics)
                                         ▼
                             ┌───────────────────────┐
                             │ ThreadSafeQueue<T>    │
                             │ (Bounded MPMC Queue)  │
                             └───────────┬───────────┘
                                         │
                 ┌───────────────────────┼───────────────────────┐
                 │                       │                       │
                 ▼                       ▼                       ▼
          ┌──────────────┐        ┌──────────────┐        ┌──────────────┐
          │ Worker Th #0 │        │ Worker Th #1 │        │ Worker Th #N │
          └──────┬───────┘        └──────┬───────┘        └──────┬───────┘
                 │                       │                       │
                 └───────────────────────┼───────────────────────┘
                                         │
                                         ▼
                             ┌───────────────────────┐
                             │   PacketValidator     │
                             │ (Header/Port/Length)  │
                             └───────────┬───────────┘
                                         │
                                         ▼
                             ┌───────────────────────┐
                             │   PacketClassifier    │
                             │ (5-Tuple/Priority/QoS)│
                             └───────────┬───────────┘
                                         │
                                         ▼
                             ┌───────────────────────┐
                             │   PacketProcessor     │
                             │ (CRC32/Payload Hash)  │
                             └───────────┬───────────┘
                                         │
                                         ▼
                             ┌───────────────────────┐
                             │   MetricsCollector    │
                             │ (Lockless Atomix/HDR) │
                             └───────────┬───────────┘
                                         │
                                         ▼
                             ┌───────────────────────┐
                             │  Console / JSON / CSV │
                             └───────────────────────┘
```

---

## 2. In-Depth Subsystem Design

### A. Network Ingestion Layer (`UDPReceiver` & `TCPReceiver`)
- **UDP Datagram Handling:** Configures POSIX non-blocking sockets with custom socket options (`SO_REUSEADDR`, `SO_REUSEPORT`, `SO_RCVBUF` up to 4MB). Uses pre-allocated receive buffers to minimize dynamic heap allocations on the hot path.
- **TCP Stream Framing:** TCP is a continuous byte stream without inherent frame boundaries. The `TCPReceiver` enforces a 36-byte binary wire header (`WireHeader`) containing a 4-byte synchronization magic (`0x504B5445`), 64-bit packet ID, timestamps, 5-tuple, and a 16-bit payload length prefix. Incomplete stream fragments are accumulated in a persistent stream buffer and parsed incrementally.

### B. Synchronization & Queue Layer (`ThreadSafeQueue<T>`)
- Multi-Producer Multi-Consumer (MPMC) bounded FIFO ring queue.
- Protected by `std::mutex` and two `std::condition_variable` primitives (`not_empty_cv_`, `not_full_cv_`).
- Predicate loops protect against spurious wakeups.
- Move semantics ensure vector payload transfers take $\mathcal{O}(1)$ pointer swaps with zero copy overhead.

### C. Worker Thread Pool (`WorkerThreadPool`)
- Fixed pool of worker consumer threads initialized to match logical CPU cores.
- Threads execute a tight event loop pulling from `ThreadSafeQueue`, routing frames through validator, classifier, and processor before recording atomic metrics.
- Catches and isolates all exceptions inside the thread boundary to guarantee service uptime.

### D. Validation & Classification Subsystem
- **Validator:** Enforces minimum/maximum payload constraints, verifies non-zero ports, validates IPv4 address structures, and checks frame checksums.
- **Classifier:** Evaluates QoS priority flags, protocol types, and destination port mappings to assign packets into categories: `CONTROL`, `DATA`, `ACK`, `HIGH_PRIORITY`, or `INVALID`.

### E. Processing Engine (`PacketProcessor`)
- Executes deterministic compute workloads (FNV-1a / Murmur-inspired bitwise rounds + CRC32 verification) on the packet payload.
- Calculates microsecond-accurate queue dwell time and compute execution time.

### F. Metrics & Telemetry Subsystem (`MetricsCollector`)
- Utilizes cache-line padded `std::atomic<uint64_t>` (`alignas(64)`) variables to eliminate **False Sharing** across CPU cores.
- Maintains a reservoir sample buffer for exact p50 (median), p95, and p99 tail latency calculations.
- Exports structured telemetry to console, CSV, and JSON.
