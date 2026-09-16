# Key Design Decisions & Architectural Trade-Offs

## 1. Thread Pool vs. One Thread Per Packet
- **Decision:** Use a pre-allocated fixed `WorkerThreadPool` matched to CPU core count.
- **Rationale:** Creating an OS thread costs 1–10 microseconds and allocates an 8MB virtual memory stack (`task_struct` in Linux). At 100,000 packets/sec, spawning a thread per packet would cause extreme context switching thrashing, cache invalidation, and kernel OOM. A fixed worker pool keeps CPUs saturated with maximum L1/L2 cache locality.

---

## 2. Condition Variable vs. Busy-Polling in Queue
- **Decision:** Use `std::condition_variable` with predicate-guarded loops (`wait(lock, predicate)`).
- **Rationale:** Busy-waiting (`while(q.empty()) {}`) consumes 100% of a core, burns CPU wattage, and starves receiver threads on CPU-constrained machines. Condition variables put threads into a dormant kernel wait state (via Linux `futex`), yielding cycles until work arrives. Predicate loops guard against spurious wakeups.

---

## 3. Atomic Counters vs. Mutexes in Metrics
- **Decision:** Use lock-free `std::atomic<uint64_t>` aligned to 64-byte boundaries (`alignas(64)`).
- **Rationale:** Acquiring a mutex on every processed packet introduces lock contention and serializes worker threads. Atomic fetch-and-add (`fetch_add`) with `std::memory_order_relaxed` executes in a single hardware instruction (e.g. `LOCK XADD` on x86, `LDADD` on ARM). Aligning each atomic variable to 64 bytes prevents **False Sharing**, where independent threads invalidate each other's CPU cache lines.

---

## 4. Bounded Queue & Backpressure Strategy
- **Decision:** Implement a bounded capacity ring with `try_push()` / drop counters.
- **Rationale:** An unbounded queue will grow indefinitely during traffic surges until the operating system triggers the OOM killer. A bounded queue bounds latency and memory footprint, exerting explicit backpressure and tracking packet drops.

---

## 5. Move Semantics & Zero-Copy Architecture
- **Decision:** Pass `Packet` and `std::vector<uint8_t>` buffers by rvalue reference (`std::move`).
- **Rationale:** Eliminates deep copy memory allocations in the packet ingestion pipeline, transforming queue pushes and pops into simple pointer swaps ($\mathcal{O}(1)$).

---

## 6. Atomic 9-Step Graceful Shutdown Protocol
- **Decision:** Implement a deterministic multi-phase teardown sequence.
- **Protocol:**
  1. Catch `SIGINT`/`SIGTERM` via atomic flag.
  2. Stop Receiver threads and close incoming sockets.
  3. Signal queue shutdown (`queue.shutdown()`), setting `is_shutdown_ = true`.
  4. Broadcast `notify_all()` on condition variables to awaken any dormant consumers.
  5. Allow worker threads to drain all remaining enqueued packets.
  6. Join worker threads cleanly.
  7. Finalize and export metrics snapshots (JSON/CSV).
  8. Clean up socket file descriptors via RAII (`SocketHandle`).
  9. Exit process with code 0 without leaks or deadlocks.
