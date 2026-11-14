
#ifndef CONCAL_LOGGER_H
#define CONCAL_LOGGER_H
#include <string>
#include "pure/id.h++"
#include <spdlog/spdlog.h>
#include <spdlog/fmt/fmt.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include "pure/configuration.h++"

inline spdlog::level::level_enum to_spdlog_level(const LoggingConfiguration::Level& level) {
    switch (level) {
        case LoggingConfiguration::Level::Trace:
            return spdlog::level::trace;
        case LoggingConfiguration::Level::Debug:
            return spdlog::level::debug;
        case LoggingConfiguration::Level::Info:
            return spdlog::level::info;
        case LoggingConfiguration::Level::Warn:
            return spdlog::level::warn;
        case LoggingConfiguration::Level::Error:
            return spdlog::level::err;
        case LoggingConfiguration::Level::Critical:
            return spdlog::level::critical;
        case LoggingConfiguration::Level::Off:
            return spdlog::level::off;
        default:
            return spdlog::level::off;
    }
}



class LoggingContext {
private:
    std::vector<spdlog::sink_ptr> _sinks;
    std::vector<std::shared_ptr<spdlog::logger>> _all; // IDK why a multi-sink example in spdlog github document used std::shared_ptr instead of unique_ptr.
    LoggingConfiguration _config;
    std::string _prefix;

public:

    LoggingContext(const LoggingConfiguration& config) : _config(config) {
        _sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
        for (auto& s : _sinks)
            s->set_pattern("[%H:%M:%S.%f] [%^%l%$] [%n] [tid %t] %v");
    }

    std::shared_ptr<spdlog::logger> make(std::string_view name) {
        auto l = std::make_shared<spdlog::logger>(
            _prefix + std::string(name),
            _sinks.begin(), _sinks.end()
        );

        l->set_level(_config.enable ? to_spdlog_level(_config.level) : spdlog::level::off);
        l->flush_on(_config.enable ?  to_spdlog_level(_config.flush_on) : spdlog::level::off);
        _all.push_back(l);
        return l;
    }

    void flush();
};

inline void LoggingContext::flush() {
    for (auto& s : _sinks) {
        s->flush();
    }
}


template<typename Tag>
struct fmt::formatter<Id<Tag>> : fmt::formatter<uint64_t> {
    auto format(const Id<Tag>& id, fmt::format_context& ctx) const {
        return fmt::formatter<uint64_t>::format(id._value, ctx);
    }
};

#endif //CONCAL_LOGGER_H
