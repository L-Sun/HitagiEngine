#include "backend/dx12/driver_api.hpp"
#include "frame_graph.hpp"
#include <hitagi/graphics/frame.hpp>
#include <hitagi/graphics/resource_manager.hpp>

#include <hitagi/core/memory_manager.hpp>
#include <hitagi/graphics/graphics_manager.hpp>
#include <hitagi/graphics/pipeline_state.hpp>
#include <hitagi/application.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

using namespace hitagi::math;
using namespace hitagi::resource;

namespace hitagi {
std::unique_ptr<graphics::GraphicsManager> g_GraphicsManager = std::make_unique<graphics::GraphicsManager>();
}

namespace hitagi::graphics {

int GraphicsManager::Initialize() {
    m_Logger = spdlog::stdout_color_mt("GraphicsManager");
    m_Logger->info("Initialize...");

    // Initialize API
    m_Driver = std::make_unique<backend::DX12::DX12DriverAPI>();

    // Initialize ResourceManager
    m_ResMgr = std::make_unique<ResourceManager>(*m_Driver);

    const auto          rect   = g_App->GetWindowsRect();
    const std::uint32_t widht  = rect.right - rect.left,
                        height = rect.bottom - rect.top;

    // Initialize frame
    m_Driver->CreateSwapChain(widht, height, sm_SwapChianSize, Format::R8G8B8A8_UNORM, g_App->GetWindow());
    for (size_t index = 0; index < sm_SwapChianSize; index++)
        m_Frames.at(index) = std::make_unique<Frame>(*m_Driver, *m_ResMgr, index);

    return 0;
}

void GraphicsManager::Finalize() {
    m_Logger->info("Finalized.");

    // Release all resource
    {
        m_Driver->IdleGPU();
        m_ResMgr = nullptr;
        for (auto&& frame : m_Frames)
            frame = nullptr;

        m_Driver = nullptr;
    }

    m_Logger = nullptr;
}

void GraphicsManager::Tick() {
    if (g_App->WindowSizeChanged()) {
        OnSizeChanged();
    }

    // TODO change the parameter to View, if multiple view port is finished
    Render();
    m_Driver->Present(m_CurrBackBuffer);
    m_CurrBackBuffer = (m_CurrBackBuffer + 1) % sm_SwapChianSize;
}

void GraphicsManager::OnSizeChanged() {
    m_Driver->IdleGPU();

    for (auto&& frame : m_Frames) {
        frame->SetRenderTarget(nullptr);
    }

    auto          rect   = g_App->GetWindowsRect();
    std::uint32_t width  = rect.right - rect.left,
                  height = rect.bottom - rect.top;

    m_CurrBackBuffer = m_Driver->ResizeSwapChain(width, height);

    for (size_t index = 0; index < m_Frames.size(); index++) {
        m_Frames.at(index)->SetRenderTarget(m_Driver->CreateRenderFromSwapChain(index));
    }
}

void GraphicsManager::SetCamera(std::shared_ptr<Camera> camera) {
    GetBcakFrameForRendering()->SetCamera(std::move(camera));
}

void GraphicsManager::AppendRenderables(std::pmr::vector<Renderable> renderables) {
    GetBcakFrameForRendering()->AddRenderables(std::move(renderables));
}

void GraphicsManager::Render() {
    auto driver  = m_Driver.get();
    auto frame   = GetBcakFrameForRendering();
    auto context = driver->GetGraphicsCommandContext();

    frame->PrepareData();

    const auto          rect          = g_App->GetWindowsRect();
    const std::uint32_t screen_width  = rect.right - rect.left,
                        screen_height = rect.bottom - rect.top;
    const float camera_aspect         = frame->GetCamera()->GetAspect();

    FrameGraph fg;

    auto render_target_handle = fg.Import(frame->GetRenderTarget());

    struct ColorPassData {
        FrameHandle depth_buffer;
        FrameHandle output;
    };

    // color pass
    auto color_pass = fg.AddPass<ColorPassData>(
        "ColorPass",
        // Setup function
        [&](FrameGraph::Builder& builder, ColorPassData& data) {
            data.depth_buffer = builder.Create<DepthBuffer>(
                "DepthBuffer",
                DepthBufferDesc{
                    .format        = Format::D32_FLOAT,
                    .width         = screen_width,
                    .height        = screen_height,
                    .clear_depth   = 1.0f,
                    .clear_stencil = 0,
                });
            data.depth_buffer = builder.Write(data.depth_buffer);
            data.output       = builder.Write(render_target_handle);
        },
        // Excute function
        [=](const ResourceHelper& helper, ColorPassData& data) {
            auto depth_buffer  = helper.Get<DepthBuffer>(data.depth_buffer);
            auto render_target = helper.Get<RenderTarget>(data.output);

            uint32_t height = screen_height;
            uint32_t width  = height * camera_aspect;
            if (width > screen_width) {
                width  = screen_width;
                height = screen_width / camera_aspect;
                context->SetViewPortAndScissor(0, (screen_height - height) >> 1, width, height);
            } else {
                context->SetViewPortAndScissor((screen_width - width) >> 1, 0, width, height);
            }

            context->SetRenderTargetAndDepthBuffer(render_target, depth_buffer);
            context->ClearRenderTarget(render_target);
            context->ClearDepthBuffer(depth_buffer);

            frame->Draw(context.get());
        });

    fg.Present(render_target_handle, context);

    fg.Compile();

    fg.Execute(*driver);
    std::uint64_t fence = context->Finish();
    frame->SetFenceValue(fence);
    fg.Retire(fence, *driver);
}

Frame* GraphicsManager::GetBcakFrameForRendering() {
    auto frame = m_Frames.at(m_CurrBackBuffer).get();
    if (frame->IsRenderingFinished() && frame->Locked()) {
        frame->Reset();
    }
    return frame;
}

}  // namespace hitagi::graphics