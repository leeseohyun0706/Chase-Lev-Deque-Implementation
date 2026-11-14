//
// Created by 이서현 on 11/2/26.
//

#ifndef CONCAL_TASK_H
#define CONCAL_TASK_H
#include <functional>
#include "pure/input.h++"
#include "func.h++"
#include "id.h++"

using SlotType = size_t;

enum class TaskState {
    Running,
    Completed,
    Ready,
    // Queued,
    // Building,
    // Waiting,
    Fail,
};

struct ExecResult {
    enum class Status {
        Ok,
        Failed,
        Skipped
    } status{Status::Ok};
    std::exception_ptr error;
};
class Task;

// Can I utilize memoization?
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

    // Starts at 1 because of seal() guard.
    // Thus, the task cannot become ready while dependencies are still being attached.
    std::atomic<uint32_t> _pending{1};
    std::atomic<EdgeNode*> _successors{nullptr};
    std::atomic<TaskState> _state{TaskState::Ready};


    std::shared_ptr<Task> _keepAlive; // The ownership of task is owned by itself and managed by pin() and unpin().

    static inline EdgeNode kClosed{};
    static EdgeNode* closed() noexcept { return &kClosed; }

public:

    void pin() { _keepAlive = shared_from_this(); }
    std::shared_ptr<Task> unpin() { return std::move(_keepAlive); }

    virtual ~Task() = default;
    virtual ExecResult execute() = 0;

    void complete(class SchedulerPolicy& scheduler);
    void depends_on(Task& parent, EdgeNode& node);
    explicit Task(const TaskId taskId) : _taskId(taskId) {}
    TaskId taskId() const { return _taskId; }

    bool try_make_ready(SchedulerPolicy& scheduler);

    bool seal(SchedulerPolicy& scheduler) {
        // If there is no dependency at the time (also, the early-completed task), make ready itself to the scheduler.
        if (_pending.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            try_make_ready(scheduler);
            return true;
        }
        return false;
    }

};

// Expected input types are convertible with target types or the same types indicating OutputRef.
template<typename A, typename P>
concept InputFor = std::convertible_to<A, P> || std::same_as<std::remove_cvref_t<A>, OutputRef<P>>;


template<typename F, typename ...Args>
class PureFuncTask final : public Task{
private:

    std::array<EdgeNode, sizeof...(Args)> _edges{};

    using R = std::invoke_result_t<F, Args...>; // Deduct a return type from FunctionType

    PureFunc<F> _function;

    // use std::decay_t for generalization.
    std::optional<std::tuple<Input<std::decay_t<Args>>...>> _inputs;

    OutputSlot<R> _outputs;

public:
    EdgeNode& edge(size_t i)  { return _edges[i]; }

    OutputSlot<R>& output() noexcept { return _outputs;}


    // a count of template arguments and a count of inputs should be the same.
    template<typename ...CallArgs>
        requires (sizeof...(CallArgs) == sizeof...(Args)) && (InputFor<CallArgs, Args> && ...)
    explicit PureFuncTask(
        TaskId taskId,
        PureFunc<F> function,
        CallArgs... inputs
        )
    : Task(taskId),
    _function(std::move(function)),
    _inputs(std::in_place, Input<Args>(std::forward<CallArgs>(inputs))...)
    {}

    ExecResult execute() override {
        ExecResult result;

        auto &inputs = _inputs.value();
        auto state = _state.load(std::memory_order_acquire);
        if (state != TaskState::Ready) throw std::logic_error("Task state not ready");

        std::apply([&](const auto&... in) {
            ((result.error = result.error ? result.error : in.error()), ...);
        }, inputs);

        if (result.error) {
            // If arguments have any errors, the task will be skipped.
            result.status = ExecResult::Status::Skipped;
            _outputs.fail(result.error);
            _state.store(TaskState::Fail, std::memory_order_release);
        }else {
            try {
                R output = std::apply(
                        [this](const auto&... input) {
                            return _function.invoke(input.resolve()...);
                        },
                        _inputs.value()
                    );

                _outputs.publish(std::move(output));
                _state.store(TaskState::Completed, std::memory_order_release);
            }catch (...) {
                // If an exception is detected in a task, it will be failed.
                result.status = ExecResult::Status::Failed;
                result.error = std::current_exception();
                _outputs.fail(result.error);
                _state.store(TaskState::Fail, std::memory_order_release);
            }
        }

        _inputs.reset();
        return result;
    }

    TaskState state() const {
        return _state.load(std::memory_order_acquire);
    }
};


#endif //CONCAL_TASK_H
