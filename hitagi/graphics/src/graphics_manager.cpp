#include "backend/dx12/dx12_device.hpp"

#include <hitagi/core/memory_manager.hpp>
#include <hitagi/graphics/graphics_manager.hpp>
#include <hitagi/application.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

using namespace hitagi::math;

namespace hitagi {
graphics::GraphicsManager* graphics_manager = nullptr;
}

namespace hitagi::graphics {

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
    for (size_t index = 0; index < sm_SwapChianSize; index++)
        m_Frames[index] = std::make_unique<Frame>(*m_Device, index);

    InitBuiltInPipeline();

    return true;
}

void GraphicsManager::Finalize() {
    // Release all resource
    {
        for (auto&& frame : m_Frames) {
            frame->Wait();
            frame = nullptr;
        }

        m_Device->RetireResource(std::move(builtin_pipeline.gui.gpu_resource));
        m_Device->RetireResource(std::move(builtin_pipeline.debug.gpu_resource));

        for (auto&& [_, pipeline] : m_Pipelines) {
            m_Device->RetireResource(std::move(pipeline.gpu_resource));
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
    m_Frames[(m_CurrBackBuffer + 0) % sm_SwapChianSize]->Execute();
    m_Frames[(m_CurrBackBuffer + 1) % sm_SwapChianSize]->Wait();
    m_Frames[(m_CurrBackBuffer + 2) % sm_SwapChianSize]->Reset();

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

void GraphicsManager::OnSizeChanged() {
    m_Device->IdleGPU();

    auto          rect   = app->GetWindowsRect();
    std::uint32_t width  = rect.right - rect.left,
                  height = rect.bottom - rect.top;

    for (auto& frame : m_Frames) {
        frame->BeforeSwapchainSizeChanged();
    }

    m_CurrBackBuffer = m_Device->ResizeSwapChain(width, height);

    for (auto& frame : m_Frames) {
        frame->AfterSwapchainSizeChanged();
    }
}

void GraphicsManager::DrawScene(const resource::Scene& scene) {
    m_Frames.at(m_CurrBackBuffer)->DrawScene(scene);
}

void GraphicsManager::DrawDebug(const DebugDrawData& debug_data) {
    m_Frames.at(m_CurrBackBuffer)->DrawDebug(debug_data);
}

void GraphicsManager::DrawGui(const GuiDrawData& gui_data) {
    m_Frames.at(m_CurrBackBuffer)->DrawGUI(gui_data);
}

void GraphicsManager::InitBuiltInPipeline() {
    builtin_pipeline.gui = {
        .vs             = std::make_shared<resource::Shader>(resource::Shader::Type::Vertex, "assets/shaders/imgui.hlsl"),
        .ps             = std::make_shared<resource::Shader>(resource::Shader::Type::Pixel, "assets/shaders/imgui.hlsl"),
        .primitive_type = resource::PrimitiveType::TriangleList,
        .blend_state    = {
               .alpha_to_coverage_enable = false,
               .enable_blend             = true,
               .src_blend                = Blend::SrcAlpha,
               .dest_blend               = Blend::InvSrcAlpha,
               .blend_op                 = BlendOp::Add,
               .src_blend_alpha          = Blend::One,
               .dest_blend_alpha         = Blend::InvSrcAlpha,
               .blend_op_alpha           = BlendOp::Add,
        },
        .rasterizer_state = {
            .fill_mode               = FillMode::Solid,
            .cull_mode               = CullMode::None,
            .front_counter_clockwise = false,
            .depth_bias              = 0,
            .depth_bias_clamp        = 0.0f,
            .slope_scaled_depth_bias = 0.0f,
            .depth_clip_enable       = true,
            .multisample_enable      = false,
            .antialiased_line_enable = false,
            .forced_sample_count     = 0,
            .conservative_raster     = false,
        },
        .render_format = resource::Format::R8G8B8A8_UNORM,
    };
    builtin_pipeline.gui.name = "gui";
    m_Device->InitPipelineState(builtin_pipeline.gui);

    builtin_pipeline.debug = {
        .vs                  = std::make_shared<resource::Shader>(resource::Shader::Type::Vertex, "assets/shaders/debug.hlsl"),
        .ps                  = std::make_shared<resource::Shader>(resource::Shader::Type::Pixel, "assets/shaders/debug.hlsl"),
        .primitive_type      = resource::PrimitiveType::LineList,
        .depth_buffer_format = resource::Format::D32_FLOAT,
    };
    builtin_pipeline.debug.name = "debug";
    m_Device->InitPipelineState(builtin_pipeline.debug);
}

}  // namespace hitagi::graphics