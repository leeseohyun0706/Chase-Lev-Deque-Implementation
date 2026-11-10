

#ifndef CONCAL_RUNTIME_H
#define CONCAL_RUNTIME_H
#include "pure/scheduler.h++"

class Runtime {
private:
    Scheduler _scheduler;
    IdGenerator<FuncId> _funcIdGenerator;
    IdGenerator<TaskId> _taskIdGenerator;

    std::mutex _mutex;
    std::vector<std::shared_ptr<Task>> _tasks;

    static void submit() {}
public:

    class InvokeScope {
    private:
        std::vector<Task*> _tasks;
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
        // template<typename F, typename... Args>
        // auto invoke(const PureFunc<F>& fn, Args&&... args) {
        //     if constexpr ((std::is_class_v<Args> || ...)){
        //         return _runtime.invoke(fn, args...);
        //     }else {
        //         auto r = _runtime.invoke(fn, args...);
        //         _tasks.push_back(&r.getTask());
        //         return r;
        //     }
        // }
        template<typename R, typename... Args1, typename... Args2>
        auto invoke(const PureFunc<R(Args1...)>& fn, Args2&&... args) {
            return _runtime.invoke(fn, std::forward<Args2>(args)...);
        }
        // Submit non-dependence tasks
        void submit() {
            for (auto& task : _tasks) _runtime.submit(task);
        }

        InvokeScope(const InvokeScope&) = delete;
        InvokeScope(InvokeScope&&) = delete;
        InvokeScope& operator=(const InvokeScope&) = delete;
        InvokeScope& operator=(InvokeScope&&) = delete;

        ~InvokeScope() = default;

    };
    explicit Runtime(const size_t &worker_size = 4) : _scheduler(worker_size) {}

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
    // template<typename F>
    // auto func_register(const F& fn) {
    //     return PureFunc<F>(_funcIdGenerator.next(), fn);
    // }

    template<typename R, typename... Args>
    auto func_register(R(fn)(Args...)) {
        return PureFunc<R(Args...)>(_funcIdGenerator.next(), fn);
    }
    // template<typename F>
    // auto func_register(F&& fn) {
    //     return PureFunc<F>(_funcIdGenerator.next(), fn);
    // }

    template<typename F, typename ...Args>
    auto invoke(const PureFunc<F>& fn, Args&&... args) {
        using R  = typename std::invoke_result_t<F, decltype(args)...>;

        auto inputs = std::tuple((Input(std::forward<Args>(args)))...);
        auto slot = std::make_shared<OutputSlot<R>>();

        auto task = std::make_shared<PureFuncTask<F, Args...>>(_taskIdGenerator.next(), fn, inputs, slot);
        // Add dependency for Child Task indicating reference input.
        (attach_dependency(*task, args), ...);

        {
            std::lock_guard lock(_mutex);
            _tasks.push_back(task);
        }
        task->seal(_scheduler.getPolicy());
        return OutputRef<R>{slot, *task};;
    }

    template<typename R, typename... Args1, typename... Args2>
    auto invoke(const PureFunc<R(Args1...)>& fn, Args2&&... args) {
        auto inputs = std::tuple<Input<Args1>...>(std::forward<Args2>(args)...);
        auto slot = std::make_shared<OutputSlot<R>>();

        auto task = std::make_shared<PureFuncTask<R(Args1...), Args1...>>(_taskIdGenerator.next(), fn, inputs, slot);
        // Add dependency for Child Task indicating reference input.
        (attach_dependency(*task, args), ...);

        {
            std::lock_guard lock(_mutex);
            _tasks.push_back(task);
        }

        task->seal(_scheduler.getPolicy());
        auto res = OutputRef<R>{slot, *task};

        return res;
    }

    template<typename T>
    void attach_dependency(Task&, const T&) {}

    template<typename T>
    void attach_dependency(Task& child, const OutputRef<T>& ref) {
        // ref.get();
        child.depends_on(ref.getTask());
    }

    void exportTrace(std::ostream& os) {
        _scheduler.exportTrace(os);
    }

    /*
     * This submit only work when it receives no any inputted OutputRef Object (No Dependency).
     */
    template <typename T, typename ...Args>
    [[deprecated("")]]
    void submit(const OutputRef<T>& first, const Args&... tasks) {
        _scheduler.submit(&first.getTask());
        submit(tasks...);
    }

    void submit(Task* task) {
        _scheduler.submit(task);
    }
};
#endif //CONCAL_RUNTIME_H
