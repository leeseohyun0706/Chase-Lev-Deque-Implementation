//
// Created by 이서현 on 11/2/26.
//

#ifndef CONCAL_TASK_H
#define CONCAL_TASK_H
#include <functional>
// #include <mutex>

#include "func.h++"
#include "core/id.h++"

using SlotType = size_t;

enum class TaskState {
    Running,
    Completed,
    Ready,
    Queued,
    Building,
    Waiting,
    Fail,
};

class Task;

struct EdgeNode {
    Task* child = nullptr;
    EdgeNode* next = nullptr;
};

/*
 * Base Abstraction Task.
 */
class Task : public std::enable_shared_from_this<Task> {
protected:
    const TaskId _taskId;

    std::atomic<uint32_t> _pending{1};

    // std::vector<std::shared_ptr<Task>> _successors; // A task owns tasks depended on itself as long as it's completed.

    std::atomic<EdgeNode*> _successors{nullptr};

    std::atomic<TaskState> _state{TaskState::Ready};

    // std::mutex _dependency_mutex;

    std::shared_ptr<Task> _keepAlive; // The ownership of task is owned by itself and managed by pin() and unpin().
    static inline EdgeNode kClosed{};
    static EdgeNode* closed() noexcept { return &kClosed; }
public:

    void pin() { _keepAlive = shared_from_this(); }
    std::shared_ptr<Task> unpin() { return std::move(_keepAlive); }

    virtual ~Task() = default;
    virtual void execute() = 0;

    void complete(class SchedulerPolicy& scheduler);
    void depends_on(Task& parent, EdgeNode& node);
    explicit Task(const TaskId taskId) : _taskId(taskId) {}
    TaskId taskId() const { return _taskId; }

    bool try_make_ready(SchedulerPolicy& scheduler);

    bool seal(SchedulerPolicy& scheduler) {
        // If there is no dependency at the time (also, if the early-completed task), make ready itself to the scheduler.
        if (_pending.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            try_make_ready(scheduler);
            return true;
        }
        return false;
        // _state.store(TaskState::Waiting, std::memory_order_relaxed);
    }

};


template<typename F, typename ...Args>
class PureFuncTask final : public Task{
private:

    std::array<EdgeNode, sizeof...(Args)> _edges{};
    using R = std::invoke_result_t<F, Args...>;
    PureFunc<F> _function;

    std::tuple<Input<Args>...> _inputs;
    // std::shared_ptr<OutputSlot<R>> _outputs;

    OutputSlot<R> _outputs;

public:
    EdgeNode& edge(size_t i)  { return _edges[i]; }

    OutputSlot<R>& output() noexcept { return _outputs;}
    explicit PureFuncTask(
        TaskId taskId,
        PureFunc<F> function,
        std::tuple<Input<Args>...> inputs
        )
    : Task(taskId),  _function(std::move(function)), _inputs(std::move(inputs)) {}

    void execute() override {
        auto state = _state.load(std::memory_order_acquire);
        if (state != TaskState::Ready) throw std::logic_error("Task state not ready");

        R output = std::apply(
            [this](const auto&... input) {
                return _function.invoke(input.resolve()...);
            },
            _inputs
        );

        _outputs.publish(std::move(output));
        _state.store(TaskState::Completed, std::memory_order_release);
    }

    TaskState state() const { return _state.load(std::memory_order_acquire); }

};

//
// template<typename R, typename ...Args>
// auto make_pure_task(R(fn)(Args...)) {
//     auto slot = std::make_shared<OutputSlot<R>>();
//
//     auto task = std::make_shared<PureFuncTask<R(Args...)>>()
// }

#endif //CONCAL_TASK_H
