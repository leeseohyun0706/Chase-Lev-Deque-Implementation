#include "pure/task.h++"

#include "pure/scheduler.h++"

// template<typename R, typename ... Args>
void Task::complete(SchedulerPolicy &scheduler)  {
    std::vector<Task*> successors;
    {
        std::lock_guard<std::mutex> lock(_dependency_mutex);
        successors.swap(_successors);
    }
    for (auto* next : successors) {
        if (next->_pending.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            // scheduler.enqueue_ready(next);
            next->try_make_ready(scheduler);
        }

    }
}

// template<typename R, typename ... Args>
void Task::depends_on(Task &parent) {
    std::lock_guard<std::mutex> lock(parent._dependency_mutex);


    // Only parent task is not completed.
    if (parent._state.load(std::memory_order_acquire) != TaskState::Completed) {
        _pending.fetch_add(1, std::memory_order_relaxed);

        parent._successors.push_back(this);
    }
}

bool Task::try_make_ready(SchedulerPolicy &scheduler) {
    // pending > 0 means its dependency is not resolved.
    if (_pending.load(std::memory_order_acquire) != 0)
            return false;

    // TaskState expected = TaskState::Ready;
    //
    // if (!_state.compare_exchange_strong(expected, TaskState::Ready,
    //     std::memory_order_acq_rel, std::memory_order_acquire))
    //     return false;

    scheduler.enqueue_ready(this);
    return true;

}
