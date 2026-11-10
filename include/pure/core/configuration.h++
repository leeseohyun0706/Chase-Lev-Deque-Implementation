
#ifndef CONCAL_CONFIGURATION_H
#define CONCAL_CONFIGURATION_H
#include <cstdint>


// Configuration for Scheduler
struct SchedulerConfiguration {
    uint32_t worker_size = 4; // Threads

    bool work_stealing = true;
};

struct TaskConfig {

};

struct TracingConfig {
    bool enable = false;

    uint32_t local_buffer_size = 4096 * 2;
};


#endif //CONCAL_CONFIGURATION_H
