#include <hitagi/gfx/graphics_manager.hpp>
#include <hitagi/core/thread_manager.hpp>
#include <hitagi/utils/exceptions.hpp>
#include <hitagi/application.hpp>

#include <spdlog/sinks/stdout_color_sinks.h>
#include <tracy/Tracy.hpp>

namespace hitagi {
gfx::GraphicsManager* graphics_manager = nullptr;
}
namespace hitagi::gfx {
GraphicsManager::GraphicsManager()
    : RuntimeModule("GraphicsManager"),
      m_Device(Device::Create(Device::Type::DX12)),
      m_SwapChain(m_Device->CreateSwapChain(
          {
              .name        = "SwapChain",
              .window_ptr  = app->GetWindow(),
              .frame_count = 2,
              .format      = Format::R8G8B8A8_UNORM,
          })),
      m_RenderGraph(std::make_unique<RenderGraph>(*m_Device)) {
}

void GraphicsManager::Tick() {
    if (thread_manager) {
        auto compile         = thread_manager->RunTask([this]() {
            ZoneScopedN("RenderGraph Compile");
            return m_RenderGraph->Compile();
        });
        auto wait_last_frame = thread_manager->RunTask([this]() {
            ZoneScopedN("Wait Last Frame");
            magic_enum::enum_for_each<CommandType>([this](auto type) {
                m_Device->GetCommandQueue(type())->WaitForFence(m_LastFenceValues[type()]);
            });
        });
        compile.wait();
        wait_last_frame.wait();
    } else {
        {
            ZoneScopedN("RenderGraph Compile");
            m_RenderGraph->Compile();
        }
        {
            ZoneScopedN("Wait Last Frame");
            magic_enum::enum_for_each<CommandType>([this](auto type) {
                m_Device->GetCommandQueue(type())->WaitForFence(m_LastFenceValues[type()]);
            });
        }
    }

    auto fence_value  = m_RenderGraph->Execute();
    m_LastFenceValues = fence_value;

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

GraphicsManager::~GraphicsManager() {
    m_Device->WaitIdle();
}

}  // namespace hitagi::gfx