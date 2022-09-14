#include "backend/dx12/dx12_device.hpp"

#include <hitagi/core/memory_manager.hpp>
#include <hitagi/graphics/graphics_manager.hpp>
#include <hitagi/application.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <winuser.h>

using namespace hitagi::math;

namespace hitagi {
gfx::GraphicsManager* graphics_manager = nullptr;
}

namespace hitagi::gfx {

bool GraphicsManager::Initialize() {
    m_Logger = spdlog::stdout_color_mt("GraphicsManager");
    m_Logger->info("Initialize...");

    // Initialize API
    m_Device = std::make_unique<backend::DX12::DX12Device>();

    const auto          rect   = app->GetWindowsRect();
    const std::uint32_t widht  = rect.right - rect.left,
                        height = rect.bottom - rect.top;

    // Initialize frame
    m_Device->CreateSwapChain(widht, height, sm_SwapChianSize, sm_BackBufferFormat, app->GetWindow());
    for (std::size_t frame_index = 0; frame_index < m_FrameBuffers.size(); frame_index++) {
        m_Device->InitRenderTargetFromSwapChain(m_FrameBuffers[frame_index], frame_index);
        m_RenderGraphs[frame_index] = std::make_unique<RenderGraph>(*m_Device);
    }

    InitBuiltInPipeline();

    return true;
}

void GraphicsManager::Finalize() {
    // Release all resource
    {
        for (auto& render_graph : m_RenderGraphs) {
            render_graph = nullptr;
        }

        m_Device->RetireResource(std::move(builtin_pipeline.debug.gpu_resource));

        for (auto&& [_, pipeline] : m_Pipelines) {
            m_Device->RetireResource(std::move(pipeline.gpu_resource));
        }
        for (auto& frame : m_FrameBuffers) {
            m_Device->RetireResource(std::move(frame.gpu_resource));
        }

        if (m_Device) {
            m_Device->IdleGPU();
            m_Device = nullptr;
        }
    }

    backend::DX12::DX12Device::ReportDebugLog();

    m_Logger->info("Finalized.");
    m_Logger = nullptr;
}

void GraphicsManager::Tick() {
    m_RenderGraphs[(m_CurrBackBuffer + 0) % sm_SwapChianSize]->Execute();
    m_RenderGraphs[(m_CurrBackBuffer + 1) % sm_SwapChianSize]->Compile();
    m_RenderGraphs[(m_CurrBackBuffer + 2) % sm_SwapChianSize]->Reset();

    m_Device->Present();
    m_CurrBackBuffer = (m_CurrBackBuffer + 1) % sm_SwapChianSize;

    if (app->WindowSizeChanged()) {
        OnSizeChanged();
    }
}

PipelineState& GraphicsManager::GetPipelineState(const resource::Material* material) {
    assert(material);
    if (!m_Pipelines.contains(material)) {
        // TODO more infomation
        PipelineState pipeline{
            .vs                  = material->GetVertexShader(),
            .ps                  = material->GetPixelShader(),
            .primitive_type      = material->GetPrimitiveType(),
            .render_format       = resource::Format::R8G8B8A8_UNORM,
            .depth_buffer_format = resource::Format::D32_FLOAT,
        };
        pipeline.name = material->GetName();

        m_Device->InitPipelineState(pipeline);
        m_Pipelines.emplace(material, std::move(pipeline));
    }
    return m_Pipelines.at(material);
}

resource::Texture& GraphicsManager::GetBackBuffer() noexcept {
    return m_FrameBuffers[(m_CurrBackBuffer + 1) % sm_SwapChianSize];
}

RenderGraph* GraphicsManager::GetRenderGraph() const noexcept {
    return m_RenderGraphs[(m_CurrBackBuffer + 1) % sm_SwapChianSize].get();
}

void GraphicsManager::OnSizeChanged() {
    for (auto& frame : m_FrameBuffers) {
        m_Device->RetireResource(std::move(frame.gpu_resource));
    }
    for (auto& render_graph : m_RenderGraphs) {
        render_graph->Reset();
    }
    m_Device->IdleGPU();

    auto          rect   = app->GetWindowsRect();
    std::uint32_t width  = rect.right - rect.left,
                  height = rect.bottom - rect.top;

    m_CurrBackBuffer = m_Device->ResizeSwapChain(width, height);

    for (std::size_t frame_index = 0; frame_index < m_FrameBuffers.size(); frame_index++) {
        m_Device->InitRenderTargetFromSwapChain(m_FrameBuffers[frame_index], frame_index);
    }
}

void GraphicsManager::DrawScene(const resource::Scene& scene, const std::shared_ptr<resource::Texture>& render_texture) {
}

void GraphicsManager::DrawDebug(const DebugDrawData& debug_data) {
}

void GraphicsManager::InitBuiltInPipeline() {
    builtin_pipeline.debug = {
        .vs                  = std::make_shared<resource::Shader>(resource::Shader::Type::Vertex, "assets/shaders/debug.hlsl"),
        .ps                  = std::make_shared<resource::Shader>(resource::Shader::Type::Pixel, "assets/shaders/debug.hlsl"),
        .primitive_type      = resource::PrimitiveType::LineList,
        .depth_buffer_format = resource::Format::D32_FLOAT,
    };
    builtin_pipeline.debug.name = "debug";
    m_Device->InitPipelineState(builtin_pipeline.debug);
}

}  // namespace hitagi::gfx