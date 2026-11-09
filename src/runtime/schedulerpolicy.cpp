#include "schedulerpolicy.h++"
#include "runtime/worker.h++"

SchedulerPolicy::SchedulerPolicy(std::span<std::unique_ptr<Worker>> workers) : _workers(workers) {
}

Task * SchedulerPolicy::try_pop(Deque<Task> &_deque)  {
    if (auto* task = _deque.pop()) {
        _readyCount.fetch_sub(1);
        return task;
    }
    return nullptr;
}

Task* SchedulerPolicy::try_steal() {
    // First Steal Policy (Iterate all workers until finding stolen task).
    for (auto& worker : _workers) {
        if (auto *task = worker->get_deque().steal()) {
            _readyCount.fetch_sub(1);
            return task;
        }
    }
    return nullptr;
}

Task * SchedulerPolicy::try_steal_from_global_queue(Deque<Task> &_deque) {

    const auto first = _globalInjectionQueue.tryPop();
    // If global queue be empty.
    if (!first) return nullptr;

    size_t r = 0; // tasks local deque pushed

    constexpr size_t batch_size = 8;
    // Takes batch size amounts of tasks from globalQueue and push them to local deque.
    for (size_t i = 1; i < batch_size; i++) {
        Task* task = _globalInjectionQueue.tryPop();
        if (!task) break;

        if (!_deque.push(task)) {
            // Going back to global queue.
            _globalInjectionQueue.push(task);
            break;
        }
        ++r;
    }

    if (r > 0)
        wake();
    _readyCount.fetch_sub(1);
    return first;
}

Task* SchedulerPolicy::try_find_task(Deque<Task> &_deque) {
    // First, tries to pop from local thread deque.
    if (auto* task = try_pop(_deque)) {
        return task;
    }
    // Second, tries to steal from global queue.
    if (auto* task = try_steal_from_global_queue(_deque))
        return task;
    return nullptr;
}

void SchedulerPolicy::wake() {
    _workEpoch.fetch_add(1);
    _workEpoch.notify_one();
}

void SchedulerPolicy::wait(size_t old) const {
    _workEpoch.wait(old);
}

void SchedulerPolicy::submit(Task *task) {
    _readyCount.fetch_add(1);
    _globalInjectionQueue.push(task);
    wake();
}

void SchedulerPolicy::enqueue_ready(Task *task) {
    if (auto*w = current_worker()) w->submit(task);
    else submit(task);
}