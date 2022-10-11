#include <hitagi/gfx/graphics_manager.hpp>
#include <hitagi/utils/exceptions.hpp>

#include <spdlog/sinks/stdout_color_sinks.h>

namespace hitagi {
gfx::GraphicsManager* graphics_manager = nullptr;
}
namespace hitagi::gfx {
bool GraphicsManager::Initialize() {
    RuntimeModule::Initialize();

    if (m_Device = Device::Create(Device::Type::DX12); m_Device == nullptr) {
        m_Logger->warn("Create device failed!");
        return false;
    }

    m_RenderGraph = std::make_unique<RenderGraph>(GetDevice());

    return true;
}

void GraphicsManager::Finalize() {
    m_RenderGraph.reset();
    m_Device.reset();
}

void GraphicsManager::Tick() {
    if (m_RenderGraph->Compile()) {
        m_RenderGraph->Execute();
    }
}

}  // namespace hitagi::gfx