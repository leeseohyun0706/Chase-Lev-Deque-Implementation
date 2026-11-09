
#ifndef CONCAL_OUTPUTSLOT_H
#define CONCAL_OUTPUTSLOT_H
#include <memory>
#include <atomic>

enum class ResultState {
    Empty,
    Ready,
    Failed
};

template<typename T>
class OutputSlot {
private:
    T _slot;
    std::atomic<ResultState> _state{ResultState::Empty};

public:
    void publish(T slot) {
        if (_state.load(std::memory_order_acquire) != ResultState::Empty)
            return;
        _slot = slot;
        _state.store(ResultState::Ready, std::memory_order_release);
        _state.notify_all();
    }

    void wait() const {
        _state.wait(ResultState::Empty, std::memory_order_acquire);
    }

    const T& read_ready() const {
        if (_state.load(std::memory_order_acquire) != ResultState::Ready) throw std::logic_error("Output is not ready");

        return _slot;
    }
    const T& get() const {
        while (true) {
            auto s = _state.load(std::memory_order_acquire);

            if (s == ResultState::Ready)
                return _slot;

            _state.wait(ResultState::Empty, std::memory_order_acquire);
        }
    }

};

template<typename T>
class OutputRef {
private:
    std::shared_ptr<OutputSlot<T>> _slot;
    std::shared_ptr<class Task> _task;
public:
    explicit OutputRef(std::shared_ptr<OutputSlot<T>> slot, std::shared_ptr<Task> task) : _slot(std::move(slot)), _task(std::move(task)) {}

    std::shared_ptr<const T> get() const {
        _slot->wait();

        return {
            _slot,
            std::addressof(_slot->read_ready())
        };
    }

    const OutputSlot<T>* getOutputSlot() const { return _slot.get(); }
    const T& read_ready() const {
        return _slot->read_ready();
    }

    auto getTask() const { return _task.get(); }
};



#endif //CONCAL_OUTPUTSLOT_H
