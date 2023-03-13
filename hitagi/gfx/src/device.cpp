#include <hitagi/gfx/device.hpp>
#include <hitagi/utils/exceptions.hpp>

#include <spdlog/sinks/stdout_color_sinks.h>

#ifdef _WIN32
#include "dx12/dx12_device.hpp"
#endif

#include "vulkan/vk_device.hpp"

namespace hitagi::gfx {

Device::Device(Type type, std::string_view name)
    : device_type(type),
      m_Logger(spdlog::stdout_color_mt(
          !name.empty()
              ? std::string(name)
              : fmt::format("{}-Device", magic_enum::enum_name(device_type)))) {}

Device::~Device() {
    if (report_debug_error_after_destroy_fn) {
        report_debug_error_after_destroy_fn();
    }
    m_Logger->debug("graphics device removed successfully!");
}

auto Device::Create(Type type, std::string_view name) -> std::unique_ptr<Device> {
    switch (type) {
        case Type::DX12:
#ifdef _WIN32
            return std::make_unique<DX12Device>(name);
#else
            return nullptr;
#endif
        case Type::Vulkan:
            return std::make_unique<VulkanDevice>(name);
        default:
            return nullptr;
    }
}
}  // namespace hitagi::gfx