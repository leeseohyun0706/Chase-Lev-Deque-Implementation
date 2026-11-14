
#ifndef CONCAL_DEQUE_H
#define CONCAL_DEQUE_H
#include <atomic>
#include <mutex>
#include <queue>

/*
 *
 * Memory Order in C++ Standard Library
 *
 * std::memory_order_relaxed : it only ensures atomicity, not ordering (used for Counter and Statistics)
 * std::memory_order_consume : it ensures ordering based on data dependency (used for reading specific pointer)
 * std::memory_order_acquire : it restricts forward moving in subsequent memory accesses (used for data reading and acquire Lock)
 * This synchronizes to properly read released data.
 * std::memory_order_release : it restricts backward moving in previous memory accesses (used for data publishing and unlock Lock)
 * This releases current or certain data into other threads.
 * std::memory_order_acq_rel : Acquire + Release (used for CAS and fetch_add)
 * std::memory_order_seq_cst : it makes single global ordering by atomic operations (simple and strong sync)
 *
 */
/*
 *
 * Chase-Lev work-stealing deque implementation.
 *
 */

/*
 * Type parameters acquire element type T.
 */
template<typename T>
class ChaseLevDeque {

    /*
     * Class Ring has slots and previous ring needed to maintain previous tasks.
     * For slots, the type of each element is atomic pointer for element type of parent class Deque.
     */
    class Ring {
        const std::size_t _capacity;
        std::unique_ptr<std::atomic<T*>[]> _slots;
        std::unique_ptr<Ring> _previous;

    public:

        explicit Ring(const std::size_t capacity) : _capacity(capacity), _slots(nullptr) {
            // Ensure N<2 and (N & (N - 1)) != 0
            // 4 = 100b, 8 = 1000b, 16 = 10000b
            // N = 8 -> 1000b
            // N - 1 = 7 -> 0111b
            // N & (N - 1) = 8 & 7 = 1000b & 0111b = 0000b

            if (_capacity < 2 || (_capacity & (_capacity - 1)) != 0)
                    throw std::invalid_argument("Capacity must be a power of two >= 2");
            _slots = std::make_unique<std::atomic<T*>[]>(_capacity);

            for (std::size_t i = 0; i != _capacity; ++i)
                _slots[i].store(nullptr, std::memory_order_relaxed); // Initialize nullptr

        }

        Ring(const Ring&) = delete;
        Ring& operator=(const Ring&) = delete;

        std::size_t capacity() const noexcept { return _capacity; }

        T* read(std::int64_t index) const noexcept {
            return _slots[static_cast<std::size_t>(index) & (_capacity-1)].load(std::memory_order_acquire);
        }

        void write(std::int64_t index, T* task) noexcept {
            _slots[static_cast<std::size_t>(index) & (_capacity-1)].store(task, std::memory_order_release);
        }

        std::unique_ptr<Ring> grow_copy(
            std::int64_t top,
            std::int64_t bottom
            ) const {
            if (_capacity > static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max() / 2)) {
                throw std::length_error("Capacity limit");
            }
            // Create new ring of Capacity * 2
            auto next = std::make_unique<Ring>(_capacity * 2);

            // Write input range into new one.
            for (auto i = top; i < bottom; ++i) {
                next->write(i, read(i));
            }
            return next;
        }

        void retain_previous(Ring* old) noexcept {
            _previous.reset(old);
        }
    };

private:

    alignas(64) std::atomic<std::int64_t> _top{0};
    alignas(64) std::atomic<std::int64_t> _bottom{0};

    std::atomic<Ring*> _ring;
public:
    explicit ChaseLevDeque(std::size_t capacity = 32) : _ring(new Ring(capacity)) {
    }

    ChaseLevDeque(const ChaseLevDeque&) = delete;
    ChaseLevDeque& operator=(const ChaseLevDeque&) = delete;
    ~ChaseLevDeque() {
        auto p = _ring.load(std::memory_order_relaxed);
        if (p) delete p;
    }
    bool push(T* task) {
        if (!task) return false;

        auto b = _bottom.load(std::memory_order_acquire);
        auto t = _top.load(std::memory_order_acquire);
        auto* ring = _ring.load(std::memory_order_acquire);

        if (b >= std::numeric_limits<std::int64_t>::max() - 1) throw std::overflow_error("Index limit exceeded");

        // Deque Full
        if (b - t >= static_cast<std::int64_t>(ring->capacity())) {
            auto grow = ring->grow_copy(t, b);
            auto* next = grow.get();

            next->retain_previous(ring); // Connect new one to previous ring

            _ring.store(next, std::memory_order_release);
            grow.release();

            ring = next;
        }
        ring->write(b, task);

        std::atomic_thread_fence(std::memory_order_release);

        _bottom.store(b+1, std::memory_order_release);
        return true;
    }

    bool empty() const noexcept {
        return (_bottom.load(std::memory_order::relaxed) - 1) < _top.load(std::memory_order_relaxed);
    }
    T* pop() {
        const auto b = _bottom.load(std::memory_order_acquire) - 1;
        auto* ring = _ring.load(std::memory_order_acquire);
        _bottom.store(b, std::memory_order_relaxed);

        std::atomic_thread_fence(std::memory_order_seq_cst);

        auto t = _top.load(std::memory_order_relaxed);


        if (b < t) {
            _bottom.store(b + 1, std::memory_order_release);
            return nullptr;
        }

        T* task = ring->read(b);

        if (t == b) {
            if (!_top.compare_exchange_strong(t, t+1, std::memory_order_seq_cst, std::memory_order_relaxed)) {
                task = nullptr;
            }

            _bottom.store(b+1, std::memory_order_release);
        }
        return task;
    }

    T* steal() {
        auto t = _top.load(std::memory_order_acquire);
        std::atomic_thread_fence(std::memory_order_seq_cst);
        auto b = _bottom.load(std::memory_order_acquire);

        if (t >= b)
            return nullptr;

        auto* ring = _ring.load(std::memory_order_consume);

        T* task = ring->read(t);

        if (!_top.compare_exchange_strong(t, t+1, std::memory_order_seq_cst, std::memory_order_relaxed)) { return nullptr; }

        return task;
    }

    std::size_t current_capacity() const noexcept {
        return _ring.load(std::memory_order_acquire)->capacity();
    }
};

#endif //CONCAL_DEQUE_H
