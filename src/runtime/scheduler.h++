
#ifndef CONCAL_SCHEDULER_H
#define CONCAL_SCHEDULER_H

#include "pure/task.h++"
#include "pure/configuration.h++"
#include "schedulerpolicy.h++"


class LoggingContext;
struct alignas(64) SchedulerStats {
    std::atomic<uint64_t> _submitted{0};
    std::atomic<uint64_t> _started{0};
    std::atomic<uint64_t> _completed{0};
    std::atomic<uint64_t> _failed{0};
};

/*
 *
 * Basic Scheduler
 *
 */
class Scheduler {
private:

    SchedulerConfiguration& _config;
    size_t _schedulerId{0};
    spdlog::logger& _logger;
    SchedulerStats _stats;
    size_t _worker_size;
    std::vector<std::unique_ptr<class Worker>> _workers;
    SchedulerPolicy _policy;
    IdGenerator<WorkerId> _workerIdGenerator;
    LoggingContext &_loggingContext;

    void stop_request() ;
    void join_worker() const;

public:

    explicit Scheduler(LoggingContext& loggingContext, SchedulerConfiguration& config, TracingConfiguration& tracing);

    void start();
    void shutdown();
    auto& getPolicy() { return _policy; }
    void exportTrace(std::ostream& out);

};



#endif //CONCAL_SCHEDULER_H
