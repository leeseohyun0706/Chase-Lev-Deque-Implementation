#include "pure/task.h++"

#include "pure/scheduler.h++"

// template<typename R, typename ... Args>
void Task::complete(SchedulerPolicy &scheduler)  {

    EdgeNode* node = _successors.exchange(closed(), std::memory_order_acquire);

    while (node) {
        EdgeNode* next = node->next;
        Task* child = node->child;
        if (child->_pending.fetch_sub(1, std::memory_order_acq_rel) == 1) { child->try_make_ready(scheduler); }
        node = next;
    }

    // std::vector<std::shared_ptr<Task>> successors;
    // {
    //     std::lock_guard<std::mutex> lock(_dependency_mutex);
    //     successors.swap(_successors); // By using swap with shared pointer, it also swaps with its ownership.
    // }
    //
    // for (auto &next : successors) {
    //     // If other tasks depended on the task have no pending, makes them ready.
    //     if (next->_pending.fetch_sub(1, std::memory_order_acq_rel) == 1) {
    //         next->try_make_ready(scheduler); // It also pins its ownership, moving the ownership into the next task themselves.
    //     }
    //
    // }
}

// template<typename R, typename ... Args>
void Task::depends_on(Task &parent, EdgeNode& node) {

    node.child = this;
    _pending.fetch_add(1, std::memory_order_relaxed);

    auto* head = parent._successors.load(std::memory_order_acquire);
    do {
        if (head == closed()) {
            _pending.fetch_sub(1, std::memory_order_relaxed);
            return;
        }
        node.next = head;
    } while (!parent._successors.compare_exchange_weak(head, &node, std::memory_order_release, std::memory_order_acquire));

    // std::lock_guard<std::mutex> lock(parent._dependency_mutex);
    //
    //
    // // Only parent task is not completed.
    // if (parent._state.load(std::memory_order_acquire) != TaskState::Completed) {
    //     _pending.fetch_add(1, std::memory_order_relaxed);
    //
    //     parent._successors.push_back(shared_from_this());
    // }
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
    // pin(); //
    scheduler.enqueue_ready(this);
    return true;

}
