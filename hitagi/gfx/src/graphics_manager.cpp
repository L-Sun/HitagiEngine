#include <hitagi/gfx/graphics_manager.hpp>
#include <hitagi/utils/exceptions.hpp>
#include <hitagi/application.hpp>

#include <spdlog/sinks/stdout_color_sinks.h>
#include <tracy/Tracy.hpp>

namespace hitagi {
gfx::GraphicsManager* graphics_manager = nullptr;
}
namespace hitagi::gfx {
bool GraphicsManager::Initialize() {
    RuntimeModule::Initialize();
    m_Clock.Start();
    m_FrameIndex = 0;

    if (m_Device = Device::Create(Device::Type::DX12); m_Device == nullptr) {
        m_Logger->warn("Create device failed!");
        return false;
    }
    m_SwapChain = m_Device->CreateSwapChain(
        {
            .name        = "SwapChain",
            .window_ptr  = app->GetWindow(),
            .frame_count = 2,
            .format      = Format::R8G8B8A8_UNORM,
        });

    m_RenderGraph = std::make_unique<RenderGraph>(GetDevice());

    return true;
}

void GraphicsManager::Finalize() {
    m_RenderGraph.reset();
    m_SwapChain.reset();
    m_Device.reset();

    m_Clock.Reset();
}

void GraphicsManager::Tick() {
    m_RenderGraph->Compile();
    {
        ZoneScopedN("Wati Last Frame");
        magic_enum::enum_for_each<CommandType>([this](auto type) {
            m_Device->GetCommandQueue(type())->WaitForFence(m_LastFenceValues[type()]);
        });
    }
    m_LastFenceValues = m_RenderGraph->Execute();
    m_RenderGraph->Reset();

    {
        ZoneScopedN("Present");
        m_SwapChain->Present();
    }

    if (app->WindowSizeChanged()) {
        m_SwapChain->Resize();
    }
    m_FrameIndex++;

    m_Device->Profile(m_FrameIndex);
    m_Clock.Tick();
}

}  // namespace hitagi::gfx