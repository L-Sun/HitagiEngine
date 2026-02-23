module;

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

export module utils:logger;

export namespace hitagi::utils {
auto try_create_logger(std::string_view name) -> std::shared_ptr<spdlog::logger> {
    std::string logger_name{name};

    auto logger = spdlog::get(logger_name);
    if (logger == nullptr) {
        logger = spdlog::stdout_color_mt(logger_name);
    }
    return logger;
}

}  // namespace hitagi::utils
