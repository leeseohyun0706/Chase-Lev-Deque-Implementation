
#ifndef CONCAL_WORKER_H
#define CONCAL_WORKER_H
#include <deque>
#include <thread>

#include "concurrency/deque.h++"
#include "pure/task.h++"
// #include "runtime/schedulerpolicy.h++"
#include "tracing/tracer.h++"





enum class WorkerState {
    Running,
    Searching, // For pop from local deque or steal from other worker.
    Parked, // There is no executable task.
    Stopped
};

struct alignas(64) WorkerStats {
    // std::atomic<WorkerState> _state{WorkerState::Searching};
    std::atomic<uint64_t> _taskExecuted{0};

    std::atomic<uint64_t> _stealAttempts{0};
    std::atomic<uint64_t> _stealSuccesses{0};

    std::atomic<uint64_t> _parkedAttempts{0};
    std::atomic<uint64_t> _waitReturns{0};
    std::atomic<uint64_t> _totalExecutionNs{0};
};



/*
 * Worker executes its tasks included in chase-lev deque on its own thread.
 */
class Worker {
private:
    friend class scheduler;
    WorkerId _workerId; // Worker Identity

    WorkerStats _stats; // Statistics
    Deque<Task> _deque; // Chase-lev Deque

    std::thread _thread;

    std::atomic<bool> _is_stopped{true}; // Thread Stop

    std::atomic<WorkerState> _state{WorkerState::Parked};

    SchedulerPolicy& _policy;

    WorkerRecordBuffer _records;
    TraceRecorder _recorder;

    void run();
public:
    WorkerId workerId() const {
        return _workerId;
    }
    explicit Worker(
        WorkerId workerId,
        SchedulerPolicy& policy,
        WorkerRecordBuffer::Clock::time_point origin,
        size_t maxLogSize = 4092 * 2
    ) :
    _workerId(workerId),
    _policy(policy),
    _records(maxLogSize, workerId, origin),
    _recorder(_records, _workerId, origin)
    {}



    /*
     * Warning: Worker::join() does not mean to wait as long as local deque is not empty; it's thread-join.
     */
    void join();

    void wake();

    void submit(Task* task);

    size_t get_epoch() const;


    void start();


    // Because worker thread could be still running when stop signal is received, there are two ways in scheduler policy:
    // First : Just entirely stop the worker when stop is requested, regardless of whether local deque is empty or not (Current Version).
    // Second : Stop the worker after all tasks in local deque entirely popped.
    void stop_request();

    auto get_deque()-> Deque<Task>& {
        return _deque;
    }

    std::span<const TraceEvent> records() const;
};

Worker* current_worker();
#endif //CONCAL_WORKER_H
