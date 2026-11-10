//
// Created by 이서현 on 11/2/26.
//

#ifndef CONCAL_TASK_H
#define CONCAL_TASK_H
#include <functional>
#include <mutex>

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

/*
 * Base Abstraction Task.
 * It poses to get pure function.
 */
class Task {
protected:
    const TaskId _taskId;

    std::atomic<uint32_t> _pending{1};
    std::vector<Task*> _successors;
    std::atomic<TaskState> _state{TaskState::Ready};

    std::mutex _dependency_mutex;
public:
    virtual ~Task() = default;
    virtual void execute() = 0;

    void complete(class SchedulerPolicy& scheduler);
    void depends_on(Task& parent);

    explicit Task(const TaskId taskId) : _taskId(taskId) {}
    TaskId taskId() const { return _taskId; }

    bool try_make_ready(SchedulerPolicy& scheduler);

    bool seal(SchedulerPolicy& scheduler) {
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
    using R = std::invoke_result_t<F, Args...>;
    PureFunc<F> _function;

    std::tuple<Input<Args>...> _inputs;
    std::shared_ptr<OutputSlot<R>> _outputs;



public:
    explicit PureFuncTask(
        TaskId taskId,
        const PureFunc<F>& function,
        const std::tuple<Input<Args>...>& inputs,
        std::shared_ptr<OutputSlot<R>> outputs)
    : Task(taskId), _function(std::move(function)), _inputs(std::move(inputs)), _outputs(outputs) {}

    void execute() override {
        auto state = _state.load(std::memory_order_acquire);
        if (state != TaskState::Ready) throw std::logic_error("Task state not ready");

        R output = std::apply(
            [this](const auto&... input) {
                return _function.invoke(input.resolve()...);
            },
            _inputs
        );

        _outputs->publish(std::move(output));
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
