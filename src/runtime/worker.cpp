#include "worker.h++"
#include "runtime/schedulerpolicy.h++"


thread_local Worker* tls_worker = nullptr;

Worker* current_worker() {
    return tls_worker;
}

/*
 * Current Worker State Changes
 *
 * Worker::Run() -> Set Searching -> Try Find Task -> Try Steal -> Set Parked
 *
 */
void Worker::run() {
        tls_worker = this;
        // As long as atomic variable _is_stopped is false (Use acquire to read released value).
        while (true) {
            // Store wakeEpoch because it could change (new wakeup) during trying pop and steal.
            const auto observed = _policy.workEpoch();

            _state.store(WorkerState::Searching); // IDK whether I can use release and acquire memory ordering to store state.

            // Try to pop from local worker deque.
            if (auto* task = _policy.try_find_task(_deque)) {
                _state.store(WorkerState::Running);

                // Run Task
                {
                    TraceScope scope(_recorder, TraceEventKind::TaskExecution, task->taskId());
                    task->execute();
                    task->complete(_policy);
                }
                _recorder.instant(TraceEventKind::TaskFinished, task->taskId(), 0);
                _stats._taskExecuted.fetch_add(1, std::memory_order_relaxed);
                continue;
            }

            // If the worker supports capturing stats during runtime, it would be needed to change for lock-free stat structure or special something.
            _stats._stealAttempts.fetch_add(1, std::memory_order_relaxed);  // Use Relaxed for statistics because we don't need to load this during worker available.

            // Try to steal from another deque by SchedulerPolicy
            _recorder.instant(TraceEventKind::StealAttempted, {0}, 0);
            if (auto* task = _policy.try_steal()) {
                _state.store(WorkerState::Running);
                _recorder.instant(TraceEventKind::StealSucceed, task->taskId(), 0);
                _recorder.instant(TraceEventKind::TaskStarted, task->taskId(), 0);
                // Run Task
                {
                    TraceScope scope(_recorder, TraceEventKind::TaskExecution, task->taskId());
                    task->execute();
                    task->complete(_policy);
                }
                _recorder.instant(TraceEventKind::TaskFinished, task->taskId(), 0);

                _stats._stealSuccesses.fetch_add(1, std::memory_order_relaxed);
                continue;
            }
            _recorder.instant(TraceEventKind::StealFailed, {0}, 0);


            if (_is_stopped.load(std::memory_order_acquire))
                break;

            _stats._parkedAttempts.fetch_add(1, std::memory_order_relaxed);
            _state.store(WorkerState::Parked);

            _recorder.instant(TraceEventKind::WorkerWaitEntered, {0}, observed);

            // Run Stolen Task
            {
                TraceScope scope(_recorder, TraceEventKind::WorkerParking, {0}, observed);
                // wait(observed) makes a current thread block as long as _wakeEpoch is the same as observed.
                _policy.wait(observed);
            }
            _recorder.instant(TraceEventKind::WorkerWaitExited, {0}, _policy.workEpoch());


        }

        tls_worker = nullptr;
}

void Worker::join() {
    if (_thread.joinable()) {
        _thread.join();
    }
}

void Worker::wake() {
    _policy.wake_one();
    _recorder.instant(TraceEventKind::WakeSignal, {0}, 0);
}

void Worker::submit(Task *task) {
    if (!task) return;
    _recorder.instant(TraceEventKind::TaskSubmitted, task->taskId(), 0);
    _deque.push(task);
    wake();
}


void Worker::start() {
    // _state.store(WorkerState::Running);
    // Store false in variable _is_stopped with release.
    _is_stopped.store(false, std::memory_order_release);
    // When start() executes, creates new thread executing Worker::run.
    _thread = std::thread(&Worker::run, this);
}

void Worker::stop_request() {
    _state.store(WorkerState::Stopped, std::memory_order_release);
    _is_stopped.store(true, std::memory_order_release);
    wake();
}

Task * Worker::try_steal() {
    return _deque.steal();
}

std::span<const TraceEvent> Worker::records() const {
    // Because WorkerRecordBuffer uses std::vector, it doesn't ensure lock-free or mutex something.
    if (!_is_stopped.load(std::memory_order_acquire)) {
        throw std::runtime_error("Worker::records() called on running state");
    }
    return _records.events();
}

