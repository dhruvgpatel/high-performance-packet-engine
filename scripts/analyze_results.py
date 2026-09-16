#!/usr/bin/env python3
"""
Performance Analysis Engine for
High-Performance Multithreaded Network Packet Processing Engine
"""

import os
import sys
import json
import csv
import argparse
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent
RESULTS_DIR = PROJECT_ROOT / "results"
CSV_FILE = RESULTS_DIR / "benchmark_results.csv"
REPORT_FILE = RESULTS_DIR / "BENCHMARK_REPORT.md"


def load_results(csv_path):
    if not csv_path.exists():
        print(f"[ERROR] Results file {csv_path} not found. Run benchmark first!")
        sys.exit(1)
    
    rows = []
    with open(csv_path, "r") as f:
        reader = csv.DictReader(f)
        for row in reader:
            parsed = {}
            for k, v in row.items():
                try:
                    if "." in v:
                        parsed[k] = float(v)
                    else:
                        parsed[k] = int(v)
                except ValueError:
                    parsed[k] = v
            rows.append(parsed)
    return rows


def generate_markdown_report(data):
    exp_a = [r for r in data if r.get("experiment") == "ExpA_WorkerScaling"]
    exp_b = [r for r in data if r.get("experiment") == "ExpB_QueueSize"]
    exp_c = [r for r in data if r.get("experiment") == "ExpC_PacketSize"]
    exp_d = [r for r in data if r.get("experiment") == "ExpD_IngestionRate"]
    exp_e = [r for r in data if r.get("experiment") == "ExpE_ProtocolComparison"]

    report = []
    report.append("# Network Packet Processing Engine - Benchmark Analysis Report")
    report.append("\n**Generated Date:** `Automated Benchmark Run`")
    report.append("\n---\n")

    # 1. Executive Summary
    report.append("## 1. Executive Summary & Key Highlights")
    if exp_a:
        base_tp = exp_a[0]["server_throughput_pps"]
        max_tp = max(r["server_throughput_pps"] for r in exp_a)
        speedup = max_tp / base_tp if base_tp > 0 else 1.0
        report.append(f"- **Worker Thread Scaling:** Achieved **{speedup:.2f}x speedup** scaling from 1 to 8 worker threads.")
        report.append(f"- **Peak Throughput:** **{max_tp:,.0f} packets/second** under multi-threaded worker pool.")
    if exp_c:
        max_mbps = max(r["server_throughput_mbps"] for r in exp_c)
        report.append(f"- **Peak Bandwidth:** **{max_mbps:,.2f} Mbps** sustained wire rate.")
    if exp_e:
        udp_tp = next((r["server_throughput_pps"] for r in exp_e if r["protocol"] == "udp"), 0)
        tcp_tp = next((r["server_throughput_pps"] for r in exp_e if r["protocol"] == "tcp"), 0)
        report.append(f"- **Transport Ingestion:** UDP Throughput: **{udp_tp:,.0f} pps** vs TCP Framed Stream: **{tcp_tp:,.0f} pps**.")

    report.append("\n---\n")

    # 2. Experiment A: Worker Scaling
    report.append("## 2. Experiment A: Worker Thread Concurrency Scaling")
    report.append("\n| Workers | Throughput (PPS) | Bandwidth (Mbps) | Avg Latency (μs) | p50 Latency (μs) | p95 Latency (μs) | p99 Latency (μs) | Speedup |")
    report.append("| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |")
    base_tp = exp_a[0]["server_throughput_pps"] if exp_a else 1.0
    for r in exp_a:
        sp = r["server_throughput_pps"] / base_tp if base_tp > 0 else 1.0
        report.append(f"| {r['workers']} | {r['server_throughput_pps']:,.0f} | {r['server_throughput_mbps']:.2f} | {r['avg_latency_us']:.2f} | {r['p50_latency_us']:.2f} | {r['p95_latency_us']:.2f} | {r['p99_latency_us']:.2f} | **{sp:.2f}x** |")

    # 3. Experiment B: Queue Size & Backpressure
    report.append("\n## 3. Experiment B: Bounded Queue Backpressure & Buffer Sizing")
    report.append("\n| Queue Capacity | Processed Packets | Dropped Packets | Drop Rate (%) | p95 Latency (μs) | Throughput (PPS) |")
    report.append("| :--- | :--- | :--- | :--- | :--- | :--- |")
    for r in exp_b:
        tot = r["total_processed"] + r["dropped_packets"]
        drop_pct = (r["dropped_packets"] / tot * 100.0) if tot > 0 else 0.0
        report.append(f"| {r['queue_size']:,} | {r['total_processed']:,} | {r['dropped_packets']:,} | {drop_pct:.2f}% | {r['p95_latency_us']:.2f} | {r['server_throughput_pps']:,.0f} |")

    # 4. Experiment C: Packet Size
    report.append("\n## 4. Experiment C: Payload Size vs Memory Bandwidth")
    report.append("\n| Payload (Bytes) | Frame Size | Throughput (PPS) | Bandwidth (Mbps) | Avg Latency (μs) |")
    report.append("| :--- | :--- | :--- | :--- | :--- |")
    for r in exp_c:
        frame_sz = r["packet_size"] + 36
        report.append(f"| {r['packet_size']} | {frame_sz} bytes | {r['server_throughput_pps']:,.0f} | {r['server_throughput_mbps']:.2f} | {r['avg_latency_us']:.2f} |")

    # 5. Experiment D: Ingestion Rate
    report.append("\n## 5. Experiment D: Controlled Ingestion Rate vs Latency")
    report.append("\n| Target Rate (PPS) | Measured Rate (PPS) | Avg Latency (μs) | p95 Latency (μs) | p99 Latency (μs) |")
    report.append("| :--- | :--- | :--- | :--- | :--- |")
    for r in exp_d:
        report.append(f"| {r['target_rate_pps']:,} | {r['server_throughput_pps']:,.0f} | {r['avg_latency_us']:.2f} | {r['p95_latency_us']:.2f} | {r['p99_latency_us']:.2f} |")

    # 6. Experiment E: Protocol
    report.append("\n## 6. Experiment E: UDP vs TCP Transport Comparison")
    report.append("\n| Protocol | Throughput (PPS) | Bandwidth (Mbps) | Avg Latency (μs) | p99 Latency (μs) |")
    report.append("| :--- | :--- | :--- | :--- | :--- |")
    for r in exp_e:
        report.append(f"| **{r['protocol'].upper()}** | {r['server_throughput_pps']:,.0f} | {r['server_throughput_mbps']:.2f} | {r['avg_latency_us']:.2f} | {r['p99_latency_us']:.2f} |")

    report_text = "\n".join(report) + "\n"
    with open(REPORT_FILE, "w") as f:
        f.write(report_text)
    
    print(f"[SUCCESS] Markdown analysis report generated: {REPORT_FILE}")
    print(report_text)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", default=str(CSV_FILE), help="Path to benchmark_results.csv")
    args = parser.parse_args()
    data = load_results(Path(args.input))
    generate_markdown_report(data)
