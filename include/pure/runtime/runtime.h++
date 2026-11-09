

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


        template<typename R, typename... Args1, typename... Args2>
        auto invoke(const PureFunc<R(Args1...)>& fn, Args2&&... args) {
            if constexpr ((std::is_same_v<Args2, OutputRef<Args1>&> || ...)) {
                return _runtime.invoke(fn, args...);
            }else {
                auto r = _runtime.invoke(fn, args...);
                _tasks.push_back(r.getTask());
                return r;
            }
        }
        ~InvokeScope() { for (auto& task : _tasks) _runtime.submit(task); }

    };
    explicit Runtime(const size_t &worker_size = 4) : _scheduler(worker_size) {}

    void start() {
        _scheduler.start();
    }
    void shutdown() {
        _scheduler.shutdown();
    }

    template<typename R, typename... Args>
    auto func_register(R(&&fn)(Args...)) {
        return PureFunc<R(Args...)>(_funcIdGenerator.next(), fn);
    }

    template<typename R, typename... Args1, typename... Args2>
    auto invoke(const PureFunc<R(Args1...)>& fn, Args2&&... args) {
        auto inputs = std::tuple<Input<Args1>...>(std::forward<Args2>(args)...);
        auto slot = std::make_shared<OutputSlot<R>>();

        auto task = std::make_shared<PureFuncTask<R, Args1...>>(_taskIdGenerator.next(), fn, inputs, slot);

        (attach_dependency(*task, args), ...);
        {
            _tasks.push_back(task);
        }

        auto res = OutputRef<R>{slot, task};

        return res;
    }

    template<typename T>
    void attach_dependency(Task&, const T&) {}

    template<typename T>
    void attach_dependency(Task& child, const OutputRef<T>& ref) {
        // ref.get();
        child.depends_on(*(ref.getTask()));
    }

    void exportTrace(std::ostream& os) {
        _scheduler.exportTrace(os);
    }

    /*
     * This submit only work when it receives no any inputted OutputRef Object (No Dependency).
     */
    template <typename T, typename ...Args>
    void submit(const OutputRef<T>& first, const Args&... tasks) {
        _scheduler.submit(first.getTask());
        submit(tasks...);
    }

    void submit(Task* task) {
        _scheduler.submit(task);
    }
};
#endif //CONCAL_RUNTIME_H
