#include "schedulerpolicy.h++"
#include "runtime/worker.h++"

SchedulerPolicy::SchedulerPolicy(std::vector<std::unique_ptr<Worker>>& workers, SchedulerConfiguration& config) : _config(config), _workers(workers), _globalInjectionQueue2(1024 * 64) {
}

Task * SchedulerPolicy::try_pop(Deque<Task> &_deque)  {
    if (auto* task = _deque.pop()) {
        // _readyCount.fetch_sub(1);
        return task;
    }
    return nullptr;
}


std::shared_ptr<Task> SchedulerPolicy::try_steal() {
    if (!_config.work_stealing) return nullptr;

    auto self = current_worker();
    size_t n = _workers.size();
    size_t start = self ? self->workerId()._value : 0; // Start with current index

    for (size_t i = 0; i < n; ++i) {
        // let start = 2, n = 4
        // when i = 0, victim = 2,
        // when i = 1, victim = 3,
        // when i = 2, victim = 0,
        // when i = 3, victim = 1.
        Worker* victim = _workers[(start + i) % n].get();
        if (victim == self) continue;
        if (auto* task = victim->try_steal()) {
            // _readyCount.fetch_sub(1);
            return task->unpin();
        }
    }

    return nullptr;
}

Task* SchedulerPolicy::try_steal_from_global_queue(Deque<Task> &_deque) {

    // const auto first = _globalInjectionQueue.tryPop();
    Task* first{nullptr};
    _globalInjectionQueue2.dequeue(first);
    // If global queue be empty.
    if (!first) return nullptr;

    size_t r = 0; // tasks local deque pushed

    constexpr size_t batch_size = 8;
    // Takes batch size amounts of tasks from globalQueue and push them to local deque.
    for (size_t i = 1; i < batch_size; i++) {
        // Task* task = _globalInjectionQueue.tryPop();
        Task* task{nullptr};
        _globalInjectionQueue2.dequeue(task);
        if (!task) break;

        if (!_deque.push(task)) {
            // Going back to global queue.
            // _globalInjectionQueue.push(task);
            _globalInjectionQueue2.enqueue(task);
            break;
        }
        ++r;
    }

    if (r > 0)
        wake_one();
    // _readyCount.fetch_sub(1);
    return first;
}

std::shared_ptr<Task> SchedulerPolicy::try_find_task(Deque<Task> &_deque) {
    // First, tries to pop from local thread deque.
    if (auto* task = try_pop(_deque)) {
        return task->unpin();
    }
    // Second, tries to steal from global queue.
    if (auto* task = try_steal_from_global_queue(_deque))
        return task->unpin();
    return nullptr;
}

void SchedulerPolicy::wake_one() {
    _workEpoch.fetch_add(1);
    // vs notify_one() ?????
    // _workEpoch.notify_all();
    _workEpoch.notify_one();
}

void SchedulerPolicy::wake_all() {
    _workEpoch.fetch_add(1);
    _workEpoch.notify_all();
}

void SchedulerPolicy::wait(size_t old) const {
    _workEpoch.wait(old);
}

void SchedulerPolicy::submit(Task *task) {
    // _readyCount.fetch_add(1);
    // _globalInjectionQueue.push(task);
    _globalInjectionQueue2.enqueue(task);
    wake_one();
}

void SchedulerPolicy::enqueue_ready(Task *task) {
    if (auto*w = current_worker()) {
        // _readyCount.fetch_add(1);
        w->submit(task);
    }
    else submit(task);
}