#!/usr/bin/env python3
"""
Automated Performance Benchmarking Suite for
High-Performance Multithreaded Network Packet Processing Engine

Runs experiments:
- Exp A: Worker Thread Scaling (1, 2, 4, 8 workers)
- Exp B: Bounded Queue Backpressure (100, 1000, 10000 slots)
- Exp C: Packet Size Impact (64, 128, 512, 1024, 4096 bytes)
- Exp D: Ingestion Rate Variations (1000, 5000, 10000, 50000 pps)
- Exp E: UDP vs TCP Transport Comparison
"""

import os
import sys
import time
import json
import csv
import subprocess
import signal
import argparse
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent
BUILD_DIR = PROJECT_ROOT / "build"
RESULTS_DIR = PROJECT_ROOT / "results"
ENGINE_BIN = BUILD_DIR / "packet_engine"
GENERATOR_BIN = BUILD_DIR / "packet_generator"


def ensure_binaries():
    if not ENGINE_BIN.exists() or not GENERATOR_BIN.exists():
        print(f"[ERROR] Engine binary ({ENGINE_BIN}) or Generator binary ({GENERATOR_BIN}) not found!")
        print("Please build the project first: cmake -S . -B build && cmake --build build -j")
        sys.exit(1)
    RESULTS_DIR.mkdir(parents=True, exist_ok=True)


def run_single_experiment(exp_name, protocol="udp", port=9000, workers=4,
                           queue_size=10000, packet_size=512, rate_pps=0,
                           packet_count=50000, duration=0, compute_iters=50):
    
    metrics_file = RESULTS_DIR / f"temp_metrics_{int(time.time()*1000)}.json"
    
    engine_cmd = [
        str(ENGINE_BIN),
        "--protocol", protocol,
        "--port", str(port),
        "--workers", str(workers),
        "--queue-size", str(queue_size),
        "--compute-iters", str(compute_iters),
        "--metrics-output", str(metrics_file),
        "--log-level", "ERROR"
    ]
    
    if duration > 0:
        engine_cmd.extend(["--duration", str(duration + 2)])

    # Start server
    server_proc = subprocess.Popen(engine_cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    time.sleep(0.3)  # Allow server to bind

    # Generator command
    gen_cmd = [
        str(GENERATOR_BIN),
        "--protocol", protocol,
        "--port", str(port),
        "--size", str(packet_size),
        "--threads", str(min(workers, 4))
    ]
    
    if duration > 0:
        gen_cmd.extend(["--duration", str(duration)])
    else:
        gen_cmd.extend(["--count", str(packet_count)])
        
    if rate_pps > 0:
        gen_cmd.extend(["--rate", str(rate_pps)])

    gen_start = time.perf_counter()
    gen_proc = subprocess.run(gen_cmd, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
    gen_elapsed = time.perf_counter() - gen_start

    # Grace period for server to drain remaining packets
    time.sleep(0.3)
    
    # Send SIGINT to server for graceful shutdown
    try:
        server_proc.send_signal(signal.SIGINT)
        server_proc.wait(timeout=5)
    except Exception as e:
        server_proc.kill()

    data = {
        "experiment": exp_name,
        "protocol": protocol,
        "workers": workers,
        "queue_size": queue_size,
        "packet_size": packet_size,
        "target_rate_pps": rate_pps,
        "packet_count": packet_count,
        "compute_iters": compute_iters,
        "generator_time_s": round(gen_elapsed, 4),
        "server_throughput_pps": 0.0,
        "server_throughput_mbps": 0.0,
        "avg_latency_us": 0.0,
        "p50_latency_us": 0.0,
        "p95_latency_us": 0.0,
        "p99_latency_us": 0.0,
        "dropped_packets": 0,
        "valid_packets": 0,
        "total_processed": 0
    }

    if metrics_file.exists():
        try:
            with open(metrics_file, "r") as f:
                metrics_json = json.load(f)
                data["server_throughput_pps"] = round(metrics_json.get("throughput_pps", 0.0), 2)
                data["server_throughput_mbps"] = round(metrics_json.get("throughput_mbps", 0.0), 2)
                data["avg_latency_us"] = round(metrics_json.get("avg_latency_us", 0.0), 2)
                data["p50_latency_us"] = round(metrics_json.get("p50_latency_us", 0.0), 2)
                data["p95_latency_us"] = round(metrics_json.get("p95_latency_us", 0.0), 2)
                data["p99_latency_us"] = round(metrics_json.get("p99_latency_us", 0.0), 2)
                data["dropped_packets"] = metrics_json.get("dropped_packets", 0)
                data["valid_packets"] = metrics_json.get("valid_packets", 0)
                data["total_processed"] = metrics_json.get("total_processed", 0)
        except Exception as e:
            print(f"[WARN] Failed to parse metrics file: {e}")
        finally:
            try:
                metrics_file.unlink()
            except OSError:
                pass

    return data


def run_all_benchmarks(quick=False):
    ensure_binaries()
    print("===============================================================")
    print(" STARTING HIGH-PERFORMANCE PACKET ENGINE BENCHMARK SUITE       ")
    print("===============================================================")
    
    count = 15000 if quick else 40000
    results = []
    base_port = 9200

    # -------------------------------------------------------------
    # Experiment A: Worker Scaling (1, 2, 4, 8)
    # -------------------------------------------------------------
    print("\n--- Running Experiment A: Worker Scaling (1, 2, 4, 8) ---")
    workers_list = [1, 2, 4, 8]
    for w in workers_list:
        port = base_port
        base_port += 1
        print(f" -> Testing {w} Worker Threads (Port: {port}, Count: {count})...", end="", flush=True)
        res = run_single_experiment("ExpA_WorkerScaling", port=port, workers=w, packet_count=count, compute_iters=40)
        results.append(res)
        print(f" Done. Throughput: {res['server_throughput_pps']:,.0f} pps | Avg Latency: {res['avg_latency_us']:.2f} us")

    # -------------------------------------------------------------
    # Experiment B: Queue Size Variations (100, 1000, 10000)
    # -------------------------------------------------------------
    print("\n--- Running Experiment B: Queue Size / Backpressure (100, 1000, 10000) ---")
    queue_sizes = [100, 1000, 10000]
    for q in queue_sizes:
        port = base_port
        base_port += 1
        print(f" -> Testing Queue Size {q} (Port: {port}, Workers: 4)...", end="", flush=True)
        res = run_single_experiment("ExpB_QueueSize", port=port, queue_size=q, workers=4, packet_count=count)
        results.append(res)
        print(f" Done. Processed: {res['total_processed']} | Dropped: {res['dropped_packets']} | p95: {res['p95_latency_us']:.2f} us")

    # -------------------------------------------------------------
    # Experiment C: Packet Size Variations (64, 128, 512, 1024, 4096)
    # -------------------------------------------------------------
    print("\n--- Running Experiment C: Packet Payload Size (64, 128, 512, 1024, 4096 bytes) ---")
    packet_sizes = [64, 128, 512, 1024, 4096]
    for sz in packet_sizes:
        port = base_port
        base_port += 1
        print(f" -> Testing Payload {sz} bytes (Port: {port})...", end="", flush=True)
        res = run_single_experiment("ExpC_PacketSize", port=port, packet_size=sz, workers=4, packet_count=count)
        results.append(res)
        print(f" Done. Throughput: {res['server_throughput_pps']:,.0f} pps ({res['server_throughput_mbps']:.2f} Mbps)")

    # -------------------------------------------------------------
    # Experiment D: Ingestion Rate Variations (1000, 5000, 10000, 50000 pps)
    # -------------------------------------------------------------
    print("\n--- Running Experiment D: Ingestion Rate Variations (1k, 5k, 10k, 50k pps) ---")
    rates = [1000, 5000, 10000, 50000]
    for r in rates:
        port = base_port
        base_port += 1
        print(f" -> Testing Target Rate {r:,} pps (Port: {port}, Duration: 2s)...", end="", flush=True)
        res = run_single_experiment("ExpD_IngestionRate", port=port, rate_pps=r, duration=2, workers=4)
        results.append(res)
        print(f" Done. Actual Rate: {res['server_throughput_pps']:,.0f} pps | Avg Latency: {res['avg_latency_us']:.2f} us")

    # -------------------------------------------------------------
    # Experiment E: UDP vs TCP Comparison
    # -------------------------------------------------------------
    print("\n--- Running Experiment E: UDP vs TCP Transport Comparison ---")
    for proto in ["udp", "tcp"]:
        port = base_port
        base_port += 1
        print(f" -> Testing Protocol {proto.upper()} (Port: {port}, Count: {count})...", end="", flush=True)
        res = run_single_experiment("ExpE_ProtocolComparison", protocol=proto, port=port, workers=4, packet_count=count)
        results.append(res)
        print(f" Done. Throughput: {res['server_throughput_pps']:,.0f} pps | p99: {res['p99_latency_us']:.2f} us")

    # Save to CSV and JSON
    csv_file = RESULTS_DIR / "benchmark_results.csv"
    json_file = RESULTS_DIR / "benchmark_summary.json"

    if results:
        keys = results[0].keys()
        with open(csv_file, "w", newline="") as f:
            dict_writer = csv.DictWriter(f, fieldnames=keys)
            dict_writer.writeheader()
            dict_writer.writerows(results)

        with open(json_file, "w") as f:
            json.dump(results, f, indent=2)

    print("\n===============================================================")
    print(f" [SUCCESS] Benchmark suite completed successfully!")
    print(f" CSV Results Saved : {csv_file}")
    print(f" JSON Summary Saved: {json_file}")
    print("===============================================================")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Packet Engine Benchmark Runner")
    parser.add_argument("--quick", action="store_true", help="Run quick benchmark with lower packet count")
    args = parser.parse_args()
    run_all_benchmarks(quick=args.quick)
