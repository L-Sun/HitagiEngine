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

// void GraphicsManager::Render() {
//     auto device = m_Device.get();
//     auto frame  = GetBcakFrameForRendering();

//     if (m_CurrScene)
//         frame->AppendRenderables(m_CurrScene->GetRenderables());

//     auto          context1 = device->CreateGraphicsCommandContext();
//     std::uint64_t fence1   = frame->PrepareData(context1.get());

//     auto        context2 = device->CreateGraphicsCommandContext();
//     RenderGraph fg(*device);

//     auto render_target_handle = fg.Import(&frame->GetRenderTarget());
//     auto depth_buffer_handle  = fg.Import(&frame->GetDepthBuffer());

//     struct ColorPassData {
//         FrameHandle depth_buffer;
//         FrameHandle output;
//     };

//     // color pass
//     auto color_pass = fg.AddPass<ColorPassData>(
//         "ColorPass",
//         // Setup function
//         [&](RenderGraph::Builder& builder, ColorPassData& data) {
//             data.depth_buffer = builder.Write(depth_buffer_handle);
//             data.output       = builder.Write(render_target_handle);
//         },
//         // Excute function
//         [=](const ResourceHelper& helper, ColorPassData& data) {
//             auto& depth_buffer  = helper.Get<DepthBuffer>(data.depth_buffer);
//             auto& render_target = helper.Get<RenderTarget>(data.output);

//             auto& camera = m_CurrScene->GetCurrentCamera();
//             frame->SetCamera(camera);

//             vec4u view_port;
//             {
//                 std::uint32_t height = render_target.height;
//                 std::uint32_t width  = height * camera.aspect;
//                 if (width > render_target.width) {
//                     width     = render_target.width;
//                     height    = render_target.width / camera.aspect;
//                     view_port = {0, (render_target.height - height) >> 1, width, height};
//                 } else {
//                     view_port = {(render_target.width - width) >> 1, 0, width, height};
//                 }
//             }
//             context2->SetViewPortAndScissor(view_port.x, view_port.y, view_port.z, view_port.w);
//             context2->SetRenderTargetAndDepthBuffer(render_target, depth_buffer);
//             context2->ClearRenderTarget(render_target);
//             context2->ClearDepthBuffer(depth_buffer);

//             frame->Render(context2.get(), resource::Renderable::Type::Default);
//         });

//     struct DebugPassData {
//         FrameHandle output;
//     };
//     auto debug_pass = fg.AddPass<DebugPassData>(
//         "DebugPass",
//         [&](RenderGraph::Builder& builder, DebugPassData& data) {
//             data.output = builder.Write(render_target_handle);
//         },
//         [=](const ResourceHelper& helper, DebugPassData& data) {
//             auto& render_target = helper.Get<RenderTarget>(data.output);
//             context2->SetRenderTarget(render_target);
//             frame->Render(context2.get(), resource::Renderable::Type::Debug);
//         });

//     struct GuiPassData {
//         FrameHandle output;
//     };

//     auto gui_pass = fg.AddPass<GuiPassData>(
//         "GuiPass",
//         [&](RenderGraph::Builder& builder, GuiPassData& data) {
//             data.output = builder.Write(render_target_handle);
//         },
//         [=](const ResourceHelper& helper, GuiPassData& data) {
//             // TODO set a orth camera
//             auto& render_target = helper.Get<RenderTarget>(data.output);
//             context2->SetRenderTarget(render_target);
//             frame->Render(context2.get(), resource::Renderable::Type::UI);
//         });

//     fg.Present(render_target_handle, context2);

//     fg.Compile();

//     fg.Execute();
//     std::uint64_t fence2 = context2->Finish();
//     fg.Retire(fence2);
//     frame->SetFenceValue(fence2);
// }

// Frame* GraphicsManager::GetBcakFrameForRendering() {
//     auto frame = m_Frames.at(m_CurrBackBuffer).get();
//     frame->Wait();
//     return frame;
// }

}  // namespace hitagi::graphics