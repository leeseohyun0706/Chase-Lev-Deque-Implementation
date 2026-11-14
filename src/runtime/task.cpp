#include "pure/task.h++"

#include "scheduler.h++"

// template<typename R, typename ... Args>
void Task::complete(SchedulerPolicy &scheduler)  {
    scheduler.stats()._completed.fetch_add(1, std::memory_order_relaxed);
    const auto* node = _successors.exchange(closed(), std::memory_order_acquire);
    // Making all successors that depend on the task to be ready.
    while (node) {
        EdgeNode* next = node->next;
        Task* child = node->child;
        if (child->_pending.fetch_sub(1, std::memory_order_acq_rel) == 1) { child->try_make_ready(scheduler); }
        node = next;
    }

}

void Task::depends_on(Task &parent, EdgeNode& node) {

    node.child = this;
    _pending.fetch_add(1, std::memory_order_relaxed);

    auto* head = parent._successors.load(std::memory_order_acquire);
    do {
        if (head == closed()) { // untile head == kClosed;
            _pending.fetch_sub(1, std::memory_order_relaxed);
            return;
        }
        node.next = head;
    } while (!parent._successors.compare_exchange_weak(head, &node, std::memory_order_release, std::memory_order_acquire));
}

bool Task::try_make_ready(SchedulerPolicy &scheduler) {
    // pending > 0 means its dependency is not resolved.
    if (_pending.load(std::memory_order_acquire) != 0)
            return false;

    scheduler.enqueue_ready(this);
    return true;

}
