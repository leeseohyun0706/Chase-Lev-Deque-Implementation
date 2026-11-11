
#ifndef CONCAL_SPSC_QUEUE_H
#define CONCAL_SPSC_QUEUE_H
#include <atomic>
#include <cassert>
#include <span>

/*
 * Lock-Free Single-Producer Single-Consumer Queue.
 */
template<typename T>
class SPSCQueue {
    // static_assert(Capacity > 0, "Capacity must be greater than 0");
    // static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of 2");
private:
    // static constexpr size_t _mask = Capacity - 1;

    alignas(64) std::atomic<size_t> _head{0};
    alignas(64) std::atomic<size_t> _tail{0};
    size_t _capacity{0};
    // Primitive Memory Buffer
    // alignas(64) alignas(T) T _buffer[Capacity]{};

    // New Awesome Unique Pointer Buffer
    std::unique_ptr<T[]> _buffer;
public:

    const T* data() const { return &_buffer[0]; }
    explicit SPSCQueue(size_t capacity = 1024) : _capacity(capacity), _buffer(std::make_unique<T[]>(capacity)) {
        assert(capacity > 0 && "Capacity must be greater than 0");
        assert((capacity & (capacity - 1)) == 0 && "Capacity must be a power of 2");
    }
    ~SPSCQueue() = default;

    // Push at index of tail.
    bool push(const T& data) {
        const auto tail = _tail.load(std::memory_order_acquire);
        const auto head = _head.load(std::memory_order_acquire);

        if (tail - head == _capacity) return false; // Full

        // new (&_buffer[tail & _mask]) T(data);
        _buffer[tail & (_capacity - 1)] = data;
        _tail.store(tail + 1, std::memory_order_release);
        return true;
    }
    // Pop at index of head
    bool pop(T& data) {
        const auto tail = _tail.load(std::memory_order_acquire);
        const auto head = _head.load(std::memory_order_acquire);

        if (head == tail) return false; // Empty

        // T& element = *reinterpret_cast<T*>(&_buffer[head & _mask]);
        data = std::move(_buffer[head & (_capacity - 1)]); // Take it just easy
        // element.~T();

        _head.store(head + 1, std::memory_order_release);
        return true;
    }

    bool empty() const noexcept {
        return _head.load(std::memory_order_relaxed) == _tail.load(std::memory_order_relaxed);
    }

    size_t size() const noexcept {
        const auto head = _head.load(std::memory_order_relaxed);
        const auto tail = _tail.load(std::memory_order_relaxed);
        return (tail >= head) ? (tail - head) : 0;
    }
};


/*
 * Use this buffer for each worker to store events without mutex lock because it's local-thread list.
 *
 * Basic Principles
 * - Fixed Capacity
 * - One Object for One Worker Thread
 * - No Dynamic Memory Allocation
 * - Retain Event Dropped Count
 * - Post Worker-End Event Readable
 */
template<typename T>
class EventSink {
private:
    SPSCQueue<T> _buffer;
    size_t _count{0};
    size_t _dropped{0};

public:
    explicit EventSink(size_t capacity = 1024) : _buffer(capacity) {}

    void record(const T& event) {
        if (!_buffer.push(event)) ++_dropped;
        else ++_count;
    }

    std::span<const T> events() const { return {_buffer.data(), _count}; }
    size_t dropped() const { return _dropped; }
    size_t count() const { return _count; }

};
#endif //CONCAL_SPSC_QUEUE_H
