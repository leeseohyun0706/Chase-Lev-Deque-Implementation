
#ifndef CONCAL_SCHEDULERPOLICY_H
#define CONCAL_SCHEDULERPOLICY_H

#include <span>
#include "runtime/worker.h++"
#include "pure/task.h++"
#include "concurrency/deque.h++"
#include "concurrency/mpmc_queue.h++"

// class Task;
// class Worker;

class SchedulerPolicy {
private:

    SchedulerConfiguration& _config;

    std::vector<std::unique_ptr<class Worker>>& _workers;
    std::atomic<size_t> _workEpoch{0};
    // std::atomic<size_t> _readyCount{0};

    // InjectionQueue<Task> _globalInjectionQueue;
    MPMCQueue<Task*> _globalInjectionQueue2;

    Task* try_pop(Deque<Task>& _deque);
    Task* try_steal_from_global_queue(Deque<Task>& _deque);

public:
    explicit SchedulerPolicy(std::vector<std::unique_ptr<Worker>>& _workers, SchedulerConfiguration& config);
    /*
     *
     * Note that try_pop and try_steal will depend on scheduler policy separated from Scheduler itself (in order to compare and benchmark in the same scheduler).
     *
     */
    std::shared_ptr<Task> try_steal();

    std::shared_ptr<Task> try_find_task(Deque<Task> &_deque);

    void wake_one();
    void wake_all();
    void wait(size_t old) const;

    size_t workEpoch() const {
        return _workEpoch.load();
    }

    void submit(Task* task);
    void enqueue_ready(Task* task);

};
#endif //CONCAL_SCHEDULERPOLICY_H
