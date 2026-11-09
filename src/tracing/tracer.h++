//
// Created by 이서현 on 11/4/26.
//

#ifndef CONCAL_TRACER_H
#define CONCAL_TRACER_H
#include <chrono>
#include <cstdint>
#include <cstddef>
#include <span>
#include <mutex>
#include <sstream>

#include "pure/core/id.h++"

// Event Shape in which be expressed.
enum class TraceEventShape : uint8_t {
    Instant, // Instant Event (Ex. Steal Success, Wake Signal)
    Span, // Duration (Ex. Task Duration, Finding Time)
    Counter // Counted
};

enum class TraceEventKind : uint8_t {
    TaskSubmitted,
    TaskEnqueued,

    TaskStarted,
    TaskExecution,
    TaskFinished,

    StealAttempted,
    StealSucceed,
    StealFailed,

    WorkerParking,
    WorkerWaitEntered,
    WorkerWaitExited,

    WakeSignal,
};

std::string to_string(TraceEventKind kind);

struct TraceEvent {
    uint64_t _ts;
    uint64_t _dur; // Duration

    TraceEventShape _shape;
    TraceEventKind _kind;

    WorkerId _workerId;
    TaskId _taskId;

    uint64_t _value; // For only mvp (Basic value storage as pointer size)

};

class TraceSink {
public:
    virtual ~TraceSink() = default;

    virtual void record(const TraceEvent& event) = 0;
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
class WorkerRecordBuffer : public TraceSink {
public:
    using Clock = std::chrono::steady_clock;
private:
    std::vector<TraceEvent> _buffer;
    size_t _count{0}; // Received Events
    size_t _dropped{0}; // Dropped Events due to Capacity Full

    WorkerId _workerId;
    Clock::time_point _origin; // Scheduler Start Time

public:

    WorkerRecordBuffer(size_t capacity, WorkerId workerId, Clock::time_point origin) : _workerId(workerId), _origin(origin), _buffer(capacity) {
    }

    void record(
        const TraceEvent& event
    ) override {
        if (_count >= _buffer.size()) {
            ++_dropped;
            return;
        }
        _buffer[_count++] = event;
    }


    std::span<const TraceEvent> events() const {
        return {_buffer.data(), _count};
    }

    size_t dropped() const { return _dropped; }
};

class TraceRecorder {
public:
    using Clock = std::chrono::steady_clock;
private:
    // Clock::time_point _startTime = Clock::now();
    TraceSink& _sink;
    Clock::time_point _origin;
    std::mutex _mutex;
    std::vector<TraceEvent> _events;
    WorkerId _workerId;
public:

    TraceRecorder(TraceSink& sink, WorkerId workerId, Clock::time_point origin) : _sink(sink), _origin(origin), _workerId(workerId) {}

    uint64_t nowNs() const {
        return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - _origin).count());
    }

    void instant(
        TraceEventKind kind,
        TaskId taskId = {0},
        uint64_t value = 0);

    void span(TraceEventKind kind, TaskId taskId, uint64_t startNs, uint64_t endNs);

    void counter(TraceEventKind kind, uint64_t value);

    std::vector<TraceEvent> snapshot() {
        std::lock_guard<std::mutex> lock(_mutex);
        return _events;
    }
};

class TraceScope {
private:
    TraceRecorder& _recorder;
    TraceEventKind _kind;
    TaskId _taskId;
    uint64_t _value;
    uint64_t _startNs;

public:
    TraceScope(TraceRecorder& recorder, TraceEventKind kind, TaskId taskId = {0}, uint64_t value = 0) : _recorder(recorder), _kind(kind), _taskId(taskId), _startNs(_recorder.nowNs()), _value(value) {}

    TraceScope(const TraceScope&) = delete;
    TraceScope(TraceScope&&) = delete;
    TraceScope& operator=(const TraceScope&) = delete;
    TraceScope& operator=(TraceScope&&) = delete;
    ~TraceScope() {
        _recorder.span(_kind, _taskId, _startNs, _recorder.nowNs());
    }
};

class TraceExporter {
public:

    void exportTrace(
        const std::span<const TraceEvent>& events,
        std::ostream& output);

};

#endif //CONCAL_TRACER_H
