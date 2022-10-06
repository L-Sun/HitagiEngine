#include <hitagi/gfx/graphics_manager.hpp>
#include <hitagi/utils/exceptions.hpp>

#include <spdlog/sinks/stdout_color_sinks.h>

namespace hitagi::gfx {
bool GraphicsManager::Initialize() {
    RuntimeModule::Initialize();

    try {
        // TODO loading config to create other device
        m_Device = Device::Create(Device::Type::DX12);
    } catch (utils::NoImplemented ex) {
        m_Logger->warn("Create device failed! The error message is:");
        m_Logger->error(ex.what());
        return false;
    }

    return true;
}

void GraphicsManager::Finalize() {
    m_Device.reset();
}

void GraphicsManager::Tick() {
}

}  // namespace hitagi::gfx