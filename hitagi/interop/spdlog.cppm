module;
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

export module interop.spdlog;
import interop.fmt;

// Export existing global-module entities without changing their ABI or ownership.
export namespace spdlog {
using ::spdlog::default_logger;
using ::spdlog::error;
using ::spdlog::get;
using ::spdlog::get_level;
using ::spdlog::info;
using ::spdlog::logger;
using ::spdlog::set_level;
using ::spdlog::stdout_color_mt;
using ::spdlog::warn;
namespace level {
using ::spdlog::level::from_str;
using ::spdlog::level::level_enum;
using enum ::spdlog::level::level_enum;
using ::spdlog::level::to_string_view;
}
}  // namespace spdlog
