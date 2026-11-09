
#ifndef CONCAL_ID_H
#define CONCAL_ID_H
#include <atomic>

template<typename Tag>
struct Id {
    uint64_t _value = 0;

    bool operator==(const Id& x) const = default;
};

struct FuncTag {};
struct TaskTag {};
struct WorkerTag{};
struct RuntimeTag{};

using FuncId = Id<FuncTag>;
using TaskId = Id<TaskTag>;
using WorkerId = Id<WorkerTag>;
using RuntimeId = Id<RuntimeTag>;


template<typename ID>
class IdGenerator {
private:
    std::atomic<uint64_t> _counter{1};
public:
    ID next() noexcept {
        return ID{
            _counter.fetch_add(1, std::memory_order_relaxed)
        };
    }
};
#endif //CONCAL_ID_H
