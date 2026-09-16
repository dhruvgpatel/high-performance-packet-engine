# Debugging & Systems Troubleshooting Guide

## 1. Building in Debug Mode with Symbols

To generate unoptimized debug binaries with full DWARF symbol tables (`-g -O0`):
```bash
cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON
cmake --build build-debug -j
```

---

## 2. GDB Debugging Workflows

### Inspecting Multi-Threaded Deadlocks & Blocked Threads:
```bash
gdb ./build-debug/packet_engine
(gdb) run --protocol udp --workers 4
# If process hangs:
(gdb) Ctrl+C
(gdb) info threads
(gdb) thread apply all bt
```

### Setting Breakpoints in Worker Threads:
```bash
(gdb) break packet_engine::WorkerThreadPool::workerRoutine
(gdb) run
(gdb) print packet
(gdb) frame
```

---

## 3. Memory Safety with AddressSanitizer (ASan)

Detects heap buffer overflows, use-after-free, double-free, and memory leaks:
```bash
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DENABLE_ASAN=ON -DBUILD_TESTS=ON
cmake --build build-asan -j
./build-asan/tests/unit_tests
```

---

## 4. Concurrency Safety with ThreadSanitizer (TSan)

Detects data races, lock order inversions, and uninitialized synchronization primitives:
```bash
cmake -S . -B build-tsan -DCMAKE_BUILD_TYPE=Debug -DENABLE_TSAN=ON -DBUILD_TESTS=ON
cmake --build build-tsan -j
./build-tsan/tests/unit_tests
```

---

## 5. Valgrind Memory and Cache Profiling

```bash
valgrind --leak-check=full --show-leak-kinds=all ./build-debug/packet_engine --duration 5
```
