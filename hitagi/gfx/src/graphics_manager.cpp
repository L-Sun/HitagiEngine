#include <hitagi/gfx/graphics_manager.hpp>
#include <hitagi/utils/exceptions.hpp>
#include <hitagi/core/thread_manager.hpp>

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
    auto compile         = thread_manager->RunTask([this]() {
        return m_RenderGraph->Compile();
    });
    auto wait_last_frame = thread_manager->RunTask([this]() {
        magic_enum::enum_for_each<CommandType>([this](auto type) {
            m_Device->GetCommandQueue(type())->WaitForFence(m_LastFenceValues[type()]);
        });
    });
    compile.wait();
    auto fence_value = m_RenderGraph->Execute();
    wait_last_frame.wait();
    m_LastFenceValues = fence_value;
}

}  // namespace hitagi::gfx