module;
#include <tracy/Tracy.hpp>

module gfx;
import std;
#ifdef _WIN32
import gfx.dx12;
#endif
import gfx.vulkan;
import gfx.mock;

namespace hitagi::gfx {

auto create_device(Device::Type type, std::string_view name) -> std::unique_ptr<Device> {
    ZoneScoped;

    switch (type) {
        case Device::Type::DX12:
#ifdef _WIN32
            return std::make_unique<DX12Device>(name);
#else
            return nullptr;
#endif
        case Device::Type::Vulkan:
            return std::make_unique<VulkanDevice>(name);
        case Device::Type::Mock:
            return std::make_unique<MockDevice>(name);
        default:
            return nullptr;
    }
}

}  // namespace hitagi::gfx
