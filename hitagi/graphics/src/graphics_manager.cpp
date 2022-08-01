#include "backend/dx12/dx12_device.hpp"
#include "render_graph.hpp"

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

    return true;
}

void GraphicsManager::Finalize() {
    // Release all resource
    {
        for (auto&& frame : m_Frames)
            m_Device->RetireResource(frame->GetRenderTarget().gpu_resource.lock());
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
    if (app->WindowSizeChanged()) {
        OnSizeChanged();
    }
    // TODO change the parameter to View, if multiple view port is finished
    Render();
    m_Device->Present(m_CurrBackBuffer);
    m_CurrBackBuffer = (m_CurrBackBuffer + 1) % sm_SwapChianSize;
}

void GraphicsManager::Draw(const resource::Scene& scene) {
    m_CurrScene = &scene;
}

void GraphicsManager::AppendRenderables(std::pmr::vector<resource::Renderable> renderables) {
    GetBcakFrameForRendering()->AppendRenderables(std::move(renderables));
}

PipelineState& GraphicsManager::GetPipelineState(const std::shared_ptr<resource::Material>& material) {
    if (m_Pipelines.count(material) == 0) {
        // TODO more infomation
        PipelineState pipeline;
        pipeline.vs             = material->GetVertexShader();
        pipeline.ps             = material->GetPixelShader();
        pipeline.primitive_type = material->primitive;
        pipeline.render_format  = resource::Format::R8G8B8A8_UNORM;

        // TODO more universe impletement
        if (material->name == "imgui") {
            pipeline.static_samplers.emplace_back(Sampler{
                .filter         = Filter::Min_Mag_Mip_Linear,
                .address_u      = TextureAddressMode::Wrap,
                .address_v      = TextureAddressMode::Wrap,
                .address_w      = TextureAddressMode::Wrap,
                .mip_lod_bias   = 0.0f,
                .max_anisotropy = 0,
                .comp_func      = ComparisonFunc::Always,
                .border_color   = vec4f(0.0f, 0.0f, 0.0f, 1.0f),
                .min_lod        = 0.0f,
                .max_lod        = 0.0f,
            });
            pipeline.blend_state = {
                .alpha_to_coverage_enable = false,
                .enable_blend             = true,
                .src_blend                = Blend::SrcAlpha,
                .dest_blend               = Blend::InvSrcAlpha,
                .blend_op                 = BlendOp::Add,
                .src_blend_alpha          = Blend::One,
                .dest_blend_alpha         = Blend::InvSrcAlpha,
                .blend_op_alpha           = BlendOp::Add,
            };
        }
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
        m_Device->RetireResource(frame->GetRenderTarget().gpu_resource.lock());
        m_Device->IdleGPU();
    }

    m_CurrBackBuffer = m_Device->ResizeSwapChain(width, height);

    for (size_t index = 0; index < m_Frames.size(); index++) {
        m_Device->InitRenderFromSwapChain(m_Frames.at(index)->GetRenderTarget(), index);
    }
}

void GraphicsManager::Render() {
    auto device = m_Device.get();
    auto frame  = GetBcakFrameForRendering();

    if (m_CurrScene)
        frame->AppendRenderables(m_CurrScene->GetRenderables());

    frame->PrepareData();

    const auto          rect          = app->GetWindowsRect();
    const std::uint32_t screen_width  = rect.right - rect.left,
                        screen_height = rect.bottom - rect.top;

    auto        context = device->CreateGraphicsCommandContext();
    RenderGraph fg(*device);

    auto render_target_handle = fg.Import(&frame->GetRenderTarget());

    struct ColorPassData {
        FrameHandle depth_buffer;
        FrameHandle output;
    };

    // color pass
    auto color_pass = fg.AddPass<ColorPassData>(
        "ColorPass",
        // Setup function
        [&](RenderGraph::Builder& builder, ColorPassData& data) {
            data.depth_buffer = builder.Create("DepthBuffer",
                                               DepthBuffer{
                                                   .format        = resource::Format::D32_FLOAT,
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
            auto& depth_buffer  = helper.Get<DepthBuffer>(data.depth_buffer);
            auto& render_target = helper.Get<RenderTarget>(data.output);

            if (m_CurrScene)
                frame->SetCamera(m_CurrScene->GetCurrentCamera());

            context->SetRenderTargetAndDepthBuffer(render_target, depth_buffer);
            context->ClearRenderTarget(render_target);
            context->ClearDepthBuffer(depth_buffer);
            frame->Render(context.get(), resource::Renderable::Type::Default);
        });

    struct DebugPassData {
        FrameHandle output;
    };
    auto debug_pass = fg.AddPass<DebugPassData>(
        "DebugPass",
        [&](RenderGraph::Builder& builder, DebugPassData& data) {
            data.output = builder.Write(render_target_handle);
        },
        [=](const ResourceHelper& helper, DebugPassData& data) {
            auto& render_target = helper.Get<RenderTarget>(data.output);
            context->SetRenderTarget(render_target);
            frame->Render(context.get(), resource::Renderable::Type::Debug);
        });

    struct GuiPassData {
        FrameHandle output;
    };
    auto gui_pass = fg.AddPass<GuiPassData>(
        "GuiPass",
        [&](RenderGraph::Builder& builder, GuiPassData& data) {
            data.output = builder.Write(render_target_handle);
        },
        [=](const ResourceHelper& helper, GuiPassData& data) {
            // TODO set a orth camera
            auto& render_target = helper.Get<RenderTarget>(data.output);
            context->SetRenderTarget(render_target);
            frame->Render(context.get(), resource::Renderable::Type::UI);
        });

    fg.Present(render_target_handle, context);

    fg.Compile();

    fg.Execute();
    std::uint64_t fence = context->Finish();
    fg.Retire(fence);
    frame->SetFenceValue(fence);
}

Frame* GraphicsManager::GetBcakFrameForRendering() {
    auto frame = m_Frames.at(m_CurrBackBuffer).get();
    if (frame->IsRenderingFinished() && frame->Locked()) {
        frame->Reset();
    }
    return frame;
}

}  // namespace hitagi::graphics