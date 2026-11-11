

#ifndef CONCAL_RUNTIME_H
#define CONCAL_RUNTIME_H
#include "pure/scheduler.h++"
#include "pure/core/configuration.h++"

class Runtime {
private:

    RuntimeConfiguration _runtimeConfig;

    Scheduler _scheduler;
    IdGenerator<FuncId> _funcIdGenerator;
    IdGenerator<TaskId> _taskIdGenerator;

    std::mutex _mutex;
    // std::vector<std::shared_ptr<Task>> _tasks;

    static void submit() {}
public:

    class InvokeScope {
    private:
        Runtime& _runtime;
    public:
        explicit InvokeScope(Runtime& runtime) : _runtime(runtime) {}
#if false
        template<typename R, typename... Args1, typename... Args2>
        auto invoke(R(&&fn)(Args1...), Args2&&... args) {
            auto rfn = _runtime.func_register(fn);
            return invoke(rfn, args...);
        }
#endif
        template<typename R, typename... Args1, typename... Args2>
        auto invoke(const PureFunc<R(Args1...)>& fn, Args2&&... args) {
            return _runtime.invoke(fn, std::forward<Args2>(args)...);
        }

        InvokeScope(const InvokeScope&) = delete;
        InvokeScope(InvokeScope&&) = delete;
        InvokeScope& operator=(const InvokeScope&) = delete;
        InvokeScope& operator=(InvokeScope&&) = delete;

        ~InvokeScope() = default;

    };

    explicit Runtime(const RuntimeConfiguration& config = RuntimeConfiguration()) : _runtimeConfig(config), _scheduler(_runtimeConfig.scheduler, _runtimeConfig.tracing) {}
    // explicit Runtime(const size_t &worker_size = 4) : _scheduler(worker_size) {}

    void start() {
        _scheduler.start();
    }
    void shutdown() {
        _scheduler.shutdown();
    }

    // template<typename R, typename... Args>
    // auto func_register(const R(&fn)(Args...)) {
    //     return PureFunc(_funcIdGenerator.next(), fn);
    // }

    template<typename F>
    auto func_register(F fn) {
        return PureFunc(_funcIdGenerator.next(), fn);
    }

    template<typename R, typename... Args>
    auto func_register(R(fn)(Args...)) {
        return PureFunc<R(Args...)>(_funcIdGenerator.next(), fn);
    }

    template<typename F, typename ...Args>
    auto invoke(const PureFunc<F>& fn, Args&&... args) {
        using R  = typename std::invoke_result_t<F, decltype(args)...>;

        auto inputs = std::tuple((Input(std::forward<Args>(args)))...);
        // auto slot = std::make_shared<OutputSlot<R>>();

        auto task = std::make_shared<PureFuncTask<F, Args...>>(_taskIdGenerator.next(), fn, inputs);
        task->pin();

        size_t edge = 0;
        // Add dependency for Child Task indicating reference input.
        (attach_dependency(*task, args, edge), ...);

        std::shared_ptr<OutputSlot<R>> slot(task, &task->output());
        task->seal(_scheduler.getPolicy());
        return OutputRef<R>{slot, task};;
    }

    template<typename R, typename... Args1, typename... Args2>
    auto invoke(const PureFunc<R(Args1...)>& fn, Args2&&... args) {
        // auto inputs = std::tuple<Input<Args1>...>(std::forward<Args2>(args)...);
        // auto slot = std::make_shared<OutputSlot<R>>();

        auto task = std::make_shared<PureFuncTask<R(Args1...), Args1...>>(_taskIdGenerator.next(), fn, std::tuple<Input<Args1>...>(args...));
        task->pin();

        size_t edge = 0;
        // Add dependency for Child Task indicating reference input.
        (attach_dependency(*task, args, edge), ...);

        std::shared_ptr<OutputSlot<R>> slot(task, &task->output());

        task->seal(_scheduler.getPolicy());


        return OutputRef<R>{slot, task};
    }

    template<typename TaskT, typename T>
    void attach_dependency(TaskT&, const T&, size_t&) {}

    template<typename TaskT, typename T>
    void attach_dependency(TaskT& child, const OutputRef<T>& ref, size_t& i) {
        // ref.get();
        child.depends_on(ref.getTask(), child.edge(i++));
    }

    void exportTrace(std::ostream& os) {
        _scheduler.exportTrace(os);
    }

};
#endif //CONCAL_RUNTIME_H
