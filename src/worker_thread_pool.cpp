#include "worker_thread_pool.hpp"
#include "logger.hpp"

namespace packet_engine {

WorkerThreadPool::WorkerThreadPool(size_t worker_count,
                                   ThreadSafeQueue<Packet>& queue,
                                   MetricsCollector& metrics,
                                   std::shared_ptr<PacketProcessor> processor,
                                   std::shared_ptr<PacketValidator> validator,
                                   std::shared_ptr<PacketClassifier> classifier)
    : worker_count_(worker_count > 0 ? worker_count : 1),
      queue_(queue),
      metrics_(metrics),
      processor_(processor ? processor : std::make_shared<PacketProcessor>()),
      validator_(validator ? validator : std::make_shared<PacketValidator>()),
      classifier_(classifier ? classifier : std::make_shared<PacketClassifier>()) {}

WorkerThreadPool::~WorkerThreadPool() {
    stop();
}

void WorkerThreadPool::start() {
    if (is_running_.exchange(true)) {
        return; // Already started
    }

    stop_requested_.store(false);
    workers_.reserve(worker_count_);
    LOG_INFO("Starting WorkerThreadPool with ", worker_count_, " worker threads...");

    for (size_t i = 0; i < worker_count_; ++i) {
        workers_.emplace_back(&WorkerThreadPool::workerRoutine, this, i);
    }
}

void WorkerThreadPool::stop() {
    if (!is_running_.load()) {
        return;
    }

    stop_requested_.store(true);
    // Queue shutdown wakes all blocked workers on condition variables
    queue_.shutdown();
    join();
    is_running_.store(false);
    LOG_INFO("WorkerThreadPool stopped successfully.");
}

void WorkerThreadPool::join() {
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    workers_.clear();
}

void WorkerThreadPool::workerRoutine(size_t worker_id) {
    LOG_DEBUG("Worker #", worker_id, " thread started.");

    while (true) {
        Packet packet;
        // Blocking pop returns false when queue is shut down AND empty
        if (!queue_.wait_and_pop(packet)) {
            break; // Queue drained and shut down
        }

        auto dequeue_tp = std::chrono::high_resolution_clock::now();
        uint64_t dequeue_ns = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(dequeue_tp.time_since_epoch()).count());
        packet.setDequeueTimestampNs(dequeue_ns);

        size_t payload_bytes = packet.getPayloadSize();

        try {
            // Pipeline execution: Validate -> Classify -> Workload Process
            ProcessingResult result = processor_->process(packet, *validator_, *classifier_);
            // Record lockless metrics
            metrics_.recordProcessed(worker_id, result, payload_bytes);
        } catch (const std::exception& ex) {
            LOG_ERROR("Worker #", worker_id, " caught exception processing packet ID ",
                      packet.getId(), ": ", ex.what());
            metrics_.recordDropped();
        } catch (...) {
            LOG_ERROR("Worker #", worker_id, " caught unknown exception processing packet ID ",
                      packet.getId());
            metrics_.recordDropped();
        }
    }

    LOG_DEBUG("Worker #", worker_id, " thread exiting cleanly.");
}

} // namespace packet_engine
