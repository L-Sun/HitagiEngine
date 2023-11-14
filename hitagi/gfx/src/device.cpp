#include <hitagi/gfx/device.hpp>
#include <hitagi/utils/exceptions.hpp>
#include <hitagi/utils/utils.hpp>
#include <hitagi/utils/logger.hpp>

#include <spdlog/sinks/stdout_color_sinks.h>
#include <tracy/Tracy.hpp>

#ifdef _WIN32
#include "dx12/dx12_device.hpp"
#endif

#include "vulkan/vk_device.hpp"
#include "mock/mock_device.hpp"

namespace hitagi::gfx {

Device::Device(Type type, std::string_view name)
    : RuntimeModule(fmt::format("{}{}", magic_enum::enum_name(type), utils::add_parentheses(name))),
      device_type(type),
      m_ShaderCompiler(name) {}

Device::~Device() {
    if (report_debug_error_after_destroy_fn) {
        report_debug_error_after_destroy_fn();
    }
    m_Logger->trace("graphics device removed successfully!");
}

auto Device::Create(Type type, std::string_view name) -> std::unique_ptr<Device> {
    ZoneScoped;

    switch (type) {
        case Type::DX12:
#ifdef _WIN32
            return std::make_unique<DX12Device>(name);
#else
            return nullptr;
#endif
        case Type::Vulkan:
            return std::make_unique<VulkanDevice>(name);
        case Type::Mock:
            return std::make_unique<MockDevice>(name);
        default:
            return nullptr;
    }
}

void Device::Tick() {
    RuntimeModule::Tick();
    m_FrameIndex++;
}

}  // namespace hitagi::gfx