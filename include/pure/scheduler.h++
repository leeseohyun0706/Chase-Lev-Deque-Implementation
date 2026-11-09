
#ifndef CONCAL_SCHEDULER_H
#define CONCAL_SCHEDULER_H

#include "concurrency/deque.h++"
#include "task.h++"
#include "tracing/tracer.h++"
#include "runtime/schedulerpolicy.h++"

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
    size_t _schedulerId{0};

    SchedulerStats _stats;
    size_t _worker_size{4};
    std::vector<std::unique_ptr<class Worker>> _workers;
    SchedulerPolicy _policy{_workers};
    void stop_request() const;
    void join_worker() const;

public:

    explicit Scheduler(size_t worker_size = 4);

    void submit(Task* task);

    void start();
    void shutdown() const;

    auto& getPolicy() { return _policy; }

    void exportTrace(std::ostream& out);

};



#endif //CONCAL_SCHEDULER_H
