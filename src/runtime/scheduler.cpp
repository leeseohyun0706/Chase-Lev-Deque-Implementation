
#include "../../include/pure/scheduler.h++"
#include "worker.h++"

// IdGenerator<WorkerId> workerIdGenerator;


void Scheduler::submit(Task *task) {
    _policy.enqueue_ready(task); // Submit according to SchedulerPolicy
    _stats._submitted.fetch_add(1, std::memory_order_relaxed);
}


void Scheduler::exportTrace(std::ostream &out) {
    TraceExporter exporter;
    std::vector<TraceEvent> events;
    for (auto& worker : _workers) {
        for (auto& event: worker->records())
            events.push_back(event);
    }
        exporter.exportTrace(std::span<const TraceEvent>(events), out);
}


void Scheduler::start() {
    for (auto& worker : _workers) {
        worker->start();
    }
}

void Scheduler::shutdown() {
    stop_request();
    join_worker();
}

void Scheduler::stop_request()  {
    for (auto& worker : _workers) {
        worker->stop_request();
    }
    _policy.wake_all();
}

void Scheduler::join_worker() const {
    for (auto& worker : _workers) {
        worker->join();
    }
}


Scheduler::Scheduler(SchedulerConfiguration &config, TracingConfiguration& tracing) : _config(config), _worker_size(config.worker_size), _policy(_workers, _config) {
    auto origin = TraceRecorder::Clock::now();

    for (size_t i = 0; i < _worker_size; i++) {
        _workers.push_back(std::make_unique<Worker>(_workerIdGenerator.next(), _policy, origin, _config.worker, tracing));
    }
}