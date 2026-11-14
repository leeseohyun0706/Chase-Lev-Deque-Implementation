
#ifndef CONCAL_CONFIGURATION_H
#define CONCAL_CONFIGURATION_H
#include <cstdint>

struct WorkerConfiguration {
    enum class IdleStrategy {
        Park, // default
        SpinThenPark, // spin before park
        Yield, // use Thread Yield (For special purposes)
    };
    uint32_t initial_deque_capacity = 32;
    IdleStrategy idle = IdleStrategy::Park;
    uint32_t spin_before_park = 0; // Only used for SpinThenPark
};

// Configuration for Scheduler
struct SchedulerConfiguration {
    uint32_t worker_size = 4; // Threads
    bool work_stealing = true;

    uint32_t global_queue_batch_size = 8;
    WorkerConfiguration worker;
};

struct TaskConfiguration {

};

struct TracingConfiguration {
    bool enable = true;
    uint32_t buffer_size = 4096 * 2;
};


struct LoggingConfiguration {
    // Copy from spdlog::level::level_enum structure
    enum class Level {
        Trace,
        Debug,
        Info,
        Warn,
        Error,
        Critical,
        Off
    };

    bool enable = true;

    Level level = Level::Debug;
    Level flush_on = Level::Warn;

    bool console = true;


};

// each Runtime owns its RuntimeConfiguration
struct RuntimeConfiguration {
    SchedulerConfiguration scheduler;
    TracingConfiguration tracing;
    LoggingConfiguration logging;
};


#endif //CONCAL_CONFIGURATION_H
