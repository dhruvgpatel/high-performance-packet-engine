# Performance Benchmarking Methodology & Experimental Design

## 1. Overview

The benchmark suite evaluates multi-threaded data plane scalability, memory bandwidth utilization, queue backpressure dynamics, and transport protocol trade-offs.

---

## 2. Experimental Design

### Experiment A: Worker Concurrency Scaling (1, 2, 4, 8 Workers)
- **Goal:** Quantify multi-threaded speedup and latency scaling under high CPU load.
- **Metrics:** Throughput (Packets/Sec), Speedup Factor ($S = \frac{T_N}{T_1}$), p50, p95, and p99 tail latencies.

### Experiment B: Queue Capacity & Backpressure (100, 1,000, 10,000 Slots)
- **Goal:** Analyze packet loss dynamics and queue dwell times under bursty network traffic.
- **Metrics:** Dropped Packet Count, Packet Loss %, and p95 Latency.

### Experiment C: Payload Size vs Memory Bandwidth (64B to 4096B)
- **Goal:** Measure memory bus saturation and effective network throughput (Mbps) vs packet rate (PPS).
- **Metrics:** Packets Per Second vs Megabits Per Second.

### Experiment D: Controlled Ingestion Rates (1k, 5k, 10k, 50k PPS)
- **Goal:** Observe latency percentiles under varying traffic arrival intensities.
- **Metrics:** p50/p95/p99 tail latency vs arrival rate.

### Experiment E: Transport Protocol Comparison (UDP vs TCP)
- **Goal:** Compare datagram vs stream framing overhead on packet ingestion throughput.
- **Metrics:** Sustained throughput, average latency, and p99 latency.

---

## 3. How to Run Benchmarks

### Execute Full Benchmark:
```bash
python3 scripts/run_benchmark.py
```

### Execute Quick Smoke Benchmark:
```bash
python3 scripts/run_benchmark.py --quick
```

### Generate Statistical Markdown Report:
```bash
python3 scripts/analyze_results.py
```

### Generate Publication-Quality Plots:
```bash
python3 scripts/plot_results.py
```
Generated figures will be saved in `results/plots/`.
