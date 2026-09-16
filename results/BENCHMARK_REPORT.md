# Network Packet Processing Engine - Benchmark Analysis Report

**Generated Date:** `Automated Benchmark Run`

---

## 1. Executive Summary & Key Highlights
- **Worker Thread Scaling:** Achieved **1.98x speedup** scaling from 1 to 8 worker threads.
- **Peak Throughput:** **31,566 packets/second** under multi-threaded worker pool.
- **Peak Bandwidth:** **265.49 Mbps** sustained wire rate.
- **Transport Ingestion:** UDP Throughput: **22,807 pps** vs TCP Framed Stream: **0 pps**.

---

## 2. Experiment A: Worker Thread Concurrency Scaling

| Workers | Throughput (PPS) | Bandwidth (Mbps) | Avg Latency (μs) | p50 Latency (μs) | p95 Latency (μs) | p99 Latency (μs) | Speedup |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| 1 | 15,910 | 65.17 | 230569.00 | 262134.00 | 346704.00 | 348814.00 | **1.00x** |
| 2 | 22,168 | 90.80 | 138664.00 | 165851.00 | 197548.00 | 201591.00 | **1.39x** |
| 4 | 24,052 | 98.52 | 100933.00 | 112709.00 | 146049.00 | 147299.00 | **1.51x** |
| 8 | 31,566 | 129.29 | 71104.20 | 73945.10 | 103811.00 | 104775.00 | **1.98x** |

## 3. Experiment B: Bounded Queue Backpressure & Buffer Sizing

| Queue Capacity | Processed Packets | Dropped Packets | Drop Rate (%) | p95 Latency (μs) | Throughput (PPS) |
| :--- | :--- | :--- | :--- | :--- | :--- |
| 100 | 10,674 | 29,326 | 73.31% | 9353.83 | 11,892 |
| 1,000 | 11,712 | 28,288 | 70.72% | 29103.20 | 12,923 |
| 10,000 | 20,518 | 19,482 | 48.70% | 164194.00 | 22,358 |

## 4. Experiment C: Payload Size vs Memory Bandwidth

| Payload (Bytes) | Frame Size | Throughput (PPS) | Bandwidth (Mbps) | Avg Latency (μs) |
| :--- | :--- | :--- | :--- | :--- |
| 64 | 100 bytes | 45,826 | 23.46 | 22086.20 |
| 128 | 164 bytes | 46,169 | 47.28 | 38998.90 |
| 512 | 548 bytes | 22,292 | 91.31 | 110421.00 |
| 1024 | 1060 bytes | 17,716 | 145.13 | 188519.00 |
| 4096 | 4132 bytes | 8,102 | 265.49 | 645006.00 |

## 5. Experiment D: Controlled Ingestion Rate vs Latency

| Target Rate (PPS) | Measured Rate (PPS) | Avg Latency (μs) | p95 Latency (μs) | p99 Latency (μs) |
| :--- | :--- | :--- | :--- | :--- |
| 1,000 | 738 | 219.18 | 232.58 | 2546.88 |
| 5,000 | 3,685 | 244.88 | 396.58 | 3852.25 |
| 10,000 | 7,375 | 221.75 | 301.42 | 3895.33 |
| 50,000 | 35,597 | 1094.55 | 5600.58 | 9621.04 |

## 6. Experiment E: UDP vs TCP Transport Comparison

| Protocol | Throughput (PPS) | Bandwidth (Mbps) | Avg Latency (μs) | p99 Latency (μs) |
| :--- | :--- | :--- | :--- | :--- |
| **UDP** | 22,807 | 93.42 | 113103.00 | 175899.00 |
| **TCP** | 0 | 0.00 | 0.00 | 0.00 |
