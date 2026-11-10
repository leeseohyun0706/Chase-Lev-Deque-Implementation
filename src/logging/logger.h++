
#ifndef CONCAL_LOGGER_H
#define CONCAL_LOGGER_H
#include <chrono>
#include <string>

#include "../concurrency/spsc_queue.h++"

struct LogEvent {
    using Clock = std::chrono::steady_clock;
    enum class LogLevel {
        Info,
        Debug,
        Warning,
        Error
    };

    uint64_t _workerId;
    LogLevel _level;
    std::string _message;
    Clock::time_point _timestamp;
};

class WorkerLogBuffer {
private:
    SPSCQueue<LogEvent> _queue;

    uint64_t _workerId;

public:
    explicit WorkerLogBuffer(const uint64_t& wid) : _workerId(wid) {}


};
class WorkerLogRecorder {

};
#endif //CONCAL_LOGGER_H
