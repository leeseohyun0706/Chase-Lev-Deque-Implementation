
#include "scheduler.h++"
#include "runtime/worker.h++"
#include <spdlog/stopwatch.h>
#include <pure/logging/logger.h++>


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
    spdlog::stopwatch sw;
    stop_request();
    join_worker();

    _logger.info("shutdown in {:.3}s    submitted={} complete={}",
        sw, _stats._submitted.load(std::memory_order_relaxed),
        _stats._completed.load(std::memory_order_relaxed)
    );
    for (auto& w : _workers) {
        const auto& s = w->stats();
        auto att = s._stealAttempts.load(std::memory_order_relaxed);
        auto ok = s._stealSuccesses.load(std::memory_order_relaxed);

        _logger.info("worker {} : executed={} steals={}/{} ({:.1f}%) parks={}",
            w->workerId(),
            s._taskExecuted.load(std::memory_order_relaxed),
            ok, att, att ? 100.0 * ok / att : 0.0,
            s._parkedAttempts.load(std::memory_order_relaxed)
        );
    }
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


Scheduler::Scheduler(LoggingContext &logContext, SchedulerConfiguration &config, TracingConfiguration& tracing)
    : _config(config),
    _logger(*logContext.make("rt.sc")),
    _worker_size(config.worker_size),
    _policy(_workers, _logger, _stats, _config),
    _loggingContext(logContext) {

    auto origin = TraceRecorder::Clock::now();

    for (size_t i = 0; i < _worker_size; i++) {
        const auto& w_id = _workerIdGenerator.next();
        _workers.push_back(std::make_unique<Worker>(
            w_id, _policy, origin,
            *_loggingContext.make("rt.sc.wo" + std::to_string(w_id._value)),
            _config.worker, tracing));
    }
}