#include <hitagi/gfx/device.hpp>
#include <hitagi/utils/exceptions.hpp>

#include <spdlog/sinks/stdout_color_sinks.h>

#include "dx12/dx12_device.hpp"

namespace hitagi::gfx {

Device::Device(Type type, std::string_view name)
    : device_type(type),
      m_Logger(spdlog::stdout_color_mt(
          !name.empty()
              ? std::string(name)
              : fmt::format("{}-Device", magic_enum::enum_name(device_type)))) {}

auto Device::Create(Type type, std::string_view name) -> std::unique_ptr<Device> {
    switch (type) {
        case Type::DX12:
            return std::make_unique<DX12Device>(name);
        default:
            return nullptr;
    }
}
}  // namespace hitagi::gfx