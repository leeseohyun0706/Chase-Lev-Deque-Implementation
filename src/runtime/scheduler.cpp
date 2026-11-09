
#include "../../include/pure/scheduler.h++"
#include "worker.h++"

IdGenerator<WorkerId> workerIdGenerator;

Scheduler::Scheduler(size_t worker_size) : _worker_size(worker_size) {
    auto origin = WorkerRecordBuffer::Clock::now();

    for (size_t i = 0; i < _worker_size; i++) {
        _workers.push_back(std::make_unique<Worker>(workerIdGenerator.next(), _policy, origin));
    }
}

void Scheduler::submit(Task *task) {
    _policy.submit(task); // Submit according to SchedulerPolicy
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

void Scheduler::shutdown() const {
    stop_request();
    join_worker();
}

void Scheduler::stop_request() const {
    for (auto& worker : _workers) {
        worker->stop_request();
    }
}

void Scheduler::join_worker() const {
    for (auto& worker : _workers) {
        worker->join();
    }
}

