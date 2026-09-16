#!/usr/bin/env python3
"""
Publication-Quality Visualization Suite for
High-Performance Multithreaded Network Packet Processing Engine
"""

import os
import sys
import csv
import argparse
from pathlib import Path

try:
    import matplotlib
    matplotlib.use("Agg") # Non-GUI backend
    import matplotlib.pyplot as plt
except ImportError:
    print("[ERROR] matplotlib is required for plotting. Install it via: pip install matplotlib")
    sys.exit(1)

PROJECT_ROOT = Path(__file__).resolve().parent.parent
RESULTS_DIR = PROJECT_ROOT / "results"
PLOTS_DIR = RESULTS_DIR / "plots"
CSV_FILE = RESULTS_DIR / "benchmark_results.csv"


def load_csv(csv_path):
    if not csv_path.exists():
        print(f"[ERROR] CSV file {csv_path} not found.")
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


def setup_style():
    plt.style.use("seaborn-v0_8-whitegrid" if "seaborn-v0_8-whitegrid" in plt.style.available else "default")
    plt.rcParams.update({
        "font.family": "sans-serif",
        "font.size": 11,
        "axes.titlesize": 13,
        "axes.titleweight": "bold",
        "axes.labelsize": 11,
        "axes.labelweight": "bold",
        "xtick.labelsize": 10,
        "ytick.labelsize": 10,
        "legend.fontsize": 10,
        "figure.titlesize": 14,
        "lines.linewidth": 2.2,
        "lines.markersize": 7,
        "grid.color": "#e0e0e0",
        "grid.linestyle": "--",
        "grid.alpha": 0.7
    })


def plot_worker_scaling(exp_data, out_dir):
    if not exp_data:
        return
    
    workers = [r["workers"] for r in exp_data]
    throughput = [r["server_throughput_pps"] for r in exp_data]
    base_tp = throughput[0] if throughput[0] > 0 else 1.0
    speedup = [tp / base_tp for tp in throughput]
    ideal_speedup = [float(w) / workers[0] for w in workers]

    # 1. Throughput & Speedup
    fig, ax1 = plt.subplots(figsize=(8, 5), dpi=300)
    color1 = "#1f77b4"
    color2 = "#2ca02c"

    ax1.set_xlabel("Worker Thread Count")
    ax1.set_ylabel("Throughput (Packets / Second)", color=color1)
    line1 = ax1.plot(workers, throughput, color=color1, marker="o", label="Measured Throughput (PPS)")
    ax1.tick_params(axis="y", labelcolor=color1)
    ax1.set_xticks(workers)

    ax2 = ax1.twinx()
    ax2.set_ylabel("Speedup Ratio (x)", color=color2)
    line2 = ax2.plot(workers, speedup, color=color2, marker="s", linestyle="-", label="Measured Speedup")
    line3 = ax2.plot(workers, ideal_speedup, color="#7f7f7f", linestyle=":", label="Linear (Ideal) Speedup")
    ax2.tick_params(axis="y", labelcolor=color2)
    ax2.grid(False)

    lines = line1 + line2 + line3
    labels = [l.get_label() for l in lines]
    ax1.legend(lines, labels, loc="lower right", frameon=True)

    plt.title("Experiment A: Worker Thread Concurrency Scaling & Throughput")
    fig.tight_layout()
    plot_path = out_dir / "worker_scaling_throughput.png"
    plt.savefig(plot_path)
    plt.close()
    print(f" -> Saved plot: {plot_path}")

    # 2. Latency percentiles vs Workers
    fig, ax = plt.subplots(figsize=(8, 5), dpi=300)
    avg_lat = [r["avg_latency_us"] for r in exp_data]
    p50_lat = [r["p50_latency_us"] for r in exp_data]
    p95_lat = [r["p95_latency_us"] for r in exp_data]
    p99_lat = [r["p99_latency_us"] for r in exp_data]

    ax.plot(workers, avg_lat, marker="o", label="Average Latency", color="#1f77b4")
    ax.plot(workers, p50_lat, marker="s", label="p50 (Median)", color="#2ca02c")
    ax.plot(workers, p95_lat, marker="^", label="p95 Tail Latency", color="#ff7f0e")
    ax.plot(workers, p99_lat, marker="d", label="p99 Tail Latency", color="#d62728")

    ax.set_xlabel("Worker Thread Count")
    ax.set_ylabel("Processing Latency (microseconds)")
    ax.set_xticks(workers)
    ax.legend(loc="upper right", frameon=True)
    plt.title("Experiment A: Latency Percentiles vs Worker Concurrency")
    fig.tight_layout()
    plot_path = out_dir / "worker_scaling_latency.png"
    plt.savefig(plot_path)
    plt.close()
    print(f" -> Saved plot: {plot_path}")


def plot_queue_size_impact(exp_data, out_dir):
    if not exp_data:
        return
    
    q_sizes = [str(r["queue_size"]) for r in exp_data]
    drops = [r["dropped_packets"] for r in exp_data]
    p95 = [r["p95_latency_us"] for r in exp_data]

    fig, ax1 = plt.subplots(figsize=(8, 5), dpi=300)
    color1 = "#d62728"
    color2 = "#1f77b4"

    bars = ax1.bar(q_sizes, drops, color=color1, alpha=0.7, width=0.4, label="Dropped Packets")
    ax1.set_xlabel("Bounded Queue Capacity (Slots)")
    ax1.set_ylabel("Dropped Packets Count", color=color1)
    ax1.tick_params(axis="y", labelcolor=color1)

    for bar in bars:
        yval = bar.get_height()
        ax1.text(bar.get_x() + bar.get_width()/2, yval + 5, f"{int(yval)}", ha="center", va="bottom", fontsize=9)

    ax2 = ax1.twinx()
    line = ax2.plot(q_sizes, p95, color=color2, marker="o", linewidth=2.5, label="p95 Latency (μs)")
    ax2.set_ylabel("p95 Latency (microseconds)", color=color2)
    ax2.tick_params(axis="y", labelcolor=color2)
    ax2.grid(False)

    plt.title("Experiment B: Queue Capacity vs Packet Loss & Backpressure")
    fig.tight_layout()
    plot_path = out_dir / "queue_size_impact.png"
    plt.savefig(plot_path)
    plt.close()
    print(f" -> Saved plot: {plot_path}")


def plot_packet_size_impact(exp_data, out_dir):
    if not exp_data:
        return
    
    sizes = [r["packet_size"] for r in exp_data]
    pps = [r["server_throughput_pps"] for r in exp_data]
    mbps = [r["server_throughput_mbps"] for r in exp_data]

    fig, ax1 = plt.subplots(figsize=(8, 5), dpi=300)
    color1 = "#1f77b4"
    color2 = "#ff7f0e"

    ax1.set_xlabel("Packet Payload Size (Bytes)")
    ax1.set_ylabel("Packet Rate (Packets / Sec)", color=color1)
    line1 = ax1.plot(sizes, pps, color=color1, marker="o", label="Throughput (PPS)")
    ax1.tick_params(axis="y", labelcolor=color1)
    ax1.set_xscale("log", base=2)
    ax1.set_xticks(sizes)
    ax1.get_xaxis().set_major_formatter(matplotlib.ticker.ScalarFormatter())

    ax2 = ax1.twinx()
    ax2.set_ylabel("Network Bandwidth (Mbps)", color=color2)
    line2 = ax2.plot(sizes, mbps, color=color2, marker="s", linestyle="--", label="Bandwidth (Mbps)")
    ax2.tick_params(axis="y", labelcolor=color2)
    ax2.grid(False)

    lines = line1 + line2
    labels = [l.get_label() for l in lines]
    ax1.legend(lines, labels, loc="center right", frameon=True)

    plt.title("Experiment C: Payload Size vs Packet Rate & Bandwidth")
    fig.tight_layout()
    plot_path = out_dir / "packet_size_throughput.png"
    plt.savefig(plot_path)
    plt.close()
    print(f" -> Saved plot: {plot_path}")


def plot_protocol_comparison(exp_data, out_dir):
    if not exp_data:
        return
    
    protos = [r["protocol"].upper() for r in exp_data]
    pps = [r["server_throughput_pps"] for r in exp_data]
    lat = [r["avg_latency_us"] for r in exp_data]

    x = range(len(protos))
    width = 0.35

    fig, ax1 = plt.subplots(figsize=(7, 5), dpi=300)
    rects1 = ax1.bar([p - width/2 for p in x], pps, width, label="Throughput (PPS)", color="#1f77b4")
    ax1.set_ylabel("Throughput (PPS)", color="#1f77b4")
    ax1.set_xticks(x)
    ax1.set_xticklabels(protos)

    ax2 = ax1.twinx()
    rects2 = ax2.bar([p + width/2 for p in x], lat, width, label="Avg Latency (μs)", color="#2ca02c")
    ax2.set_ylabel("Avg Latency (μs)", color="#2ca02c")
    ax2.grid(False)

    plt.title("Experiment E: UDP Datagram vs Framed TCP Ingestion")
    fig.tight_layout()
    plot_path = out_dir / "protocol_comparison.png"
    plt.savefig(plot_path)
    plt.close()
    print(f" -> Saved plot: {plot_path}")


def main():
    setup_style()
    PLOTS_DIR.mkdir(parents=True, exist_ok=True)
    
    data = load_csv(CSV_FILE)
    exp_a = [r for r in data if r.get("experiment") == "ExpA_WorkerScaling"]
    exp_b = [r for r in data if r.get("experiment") == "ExpB_QueueSize"]
    exp_c = [r for r in data if r.get("experiment") == "ExpC_PacketSize"]
    exp_e = [r for r in data if r.get("experiment") == "ExpE_ProtocolComparison"]

    plot_worker_scaling(exp_a, PLOTS_DIR)
    plot_queue_size_impact(exp_b, PLOTS_DIR)
    plot_packet_size_impact(exp_c, PLOTS_DIR)
    plot_protocol_comparison(exp_e, PLOTS_DIR)
    print(f"\n[SUCCESS] All plots generated in {PLOTS_DIR}")


if __name__ == "__main__":
    main()
