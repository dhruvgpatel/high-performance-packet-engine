# Comprehensive Testing Guide

## 1. Overview

The test suite is built on **GoogleTest (v1.14.0)** and verifies every individual subsystem as well as end-to-end socket data planes.

---

## 2. Test Breakdown

| Test Suite | Test Cases | Objective |
| :--- | :--- | :--- |
| `PacketTest` | `BasicCreationAndGetters`, `IpNumericConversions`, `SerializationAndDeserializationRoundtrip`, `CorruptedDeserializationRejection` | Validates data layout, binary wire serialization roundtrip, and corrupted packet rejection. |
| `ThreadSafeQueueTest` | `BasicOperations`, `BoundedCapacityTryPush`, `MultiProducerMultiConsumerIntegrity`, `ShutdownWakesBlockedConsumers` | Multi-threaded MPMC stress tests with 100,000 items, verifying zero element loss, backpressure, and shutdown unblocking. |
| `PacketValidatorTest` | `ValidPacket`, `InvalidPorts`, `InvalidProtocol`, `PayloadSizeBounds`, `ChecksumVerification` | Verifies boundary conditions, invalid port 0, checksum bitflips, and length bounds. |
| `PacketClassifierTest` | `ClassificationRules` | Tests QoS priority flags, DNS/ICMP control detection, and ACK classification. |
| `PacketProcessorTest` | `ProcessValidPacket`, `ProcessInvalidPacket` | Validates hash calculation, compute latency measurement, and error status handling. |
| `MetricsCollectorTest` | `BasicCounters`, `ConcurrentUpdates`, `JsonReportContainsRequiredFields` | Tests multi-threaded atomic counter concurrency and JSON export integrity. |
| `WorkerThreadPoolTest` | `ProcessAllPacketsWithoutLoss` | Tests worker thread pool lifecycle, job consumption, and queue draining. |
| `IntegrationTest` | `EndToEndUdpIngestAndProcess` | End-to-end socket loopback test sending 500 UDP datagrams over the network stack. |

---

## 3. Running Tests

### Standard Unit & Integration Tests:
```bash
# Configure and build
cmake -S . -B build -DBUILD_TESTS=ON
cmake --build build -j

# Run tests via CTest
ctest --test-dir build --output-on-failure
```

### Running with AddressSanitizer (ASan):
```bash
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DENABLE_ASAN=ON -DBUILD_TESTS=ON
cmake --build build-asan -j
ctest --test-dir build-asan --output-on-failure
```

### Running with ThreadSanitizer (TSan):
```bash
cmake -S . -B build-tsan -DCMAKE_BUILD_TYPE=Debug -DENABLE_TSAN=ON -DBUILD_TESTS=ON
cmake --build build-tsan -j
ctest --test-dir build-tsan --output-on-failure
```
