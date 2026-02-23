module;

#include <spdlog/sinks/stdout_color_sinks.h>

module gfx.base;
import magic_enum;

namespace hitagi::gfx {

Device::Device(Type type, std::string_view name)
    : RuntimeModule(std::format("{}{}", magic_enum::enum_name(type), utils::add_parentheses(name))),
      device_type(type),
      m_ShaderCompiler(name) {}

Device::~Device() {
    if (report_debug_error_after_destroy_fn) {
        report_debug_error_after_destroy_fn();
    }
    m_Logger->trace("graphics device removed successfully!");
}

void Device::Tick() {
    RuntimeModule::Tick();
    m_FrameIndex++;
}

}  // namespace hitagi::gfx