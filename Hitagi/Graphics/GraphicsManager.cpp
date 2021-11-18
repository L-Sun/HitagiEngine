#include "GraphicsManager.hpp"

#include "PipelineState.hpp"
#include "FrameGraph.hpp"
#include "SceneManager.hpp"
#include "Application.hpp"
#include "DX12/DX12DriverAPI.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace Hitagi {
std::unique_ptr<Graphics::GraphicsManager> g_GraphicsManager = std::make_unique<Graphics::GraphicsManager>();
}

namespace Hitagi::Graphics {

int GraphicsManager::Initialize() {
    m_Logger = spdlog::stdout_color_mt("GraphicsManager");
    m_Logger->info("Initialize...");
    auto& config = g_App->GetConfiguration();

    // Initialize API
    m_Driver = std::make_unique<backend::DX12::DX12DriverAPI>();
    // Initialize ResourceManager
    m_ResMgr = std::make_unique<ResourceManager>(*m_Driver);

    // Initialize frame
    m_Driver->CreateSwapChain(config.screen_width, config.screen_height, sm_SwapChianSize, Format::R8G8B8A8_UNORM, g_App->GetWindow());
    for (size_t index = 0; index < sm_SwapChianSize; index++)
        m_Frames.at(index) = std::make_unique<Frame>(*m_Driver, *m_ResMgr, index);

    // Initialize Shader Manader
    m_ShaderManager.Initialize();

    // TODO get and load all shader from AssetManager
    m_ShaderManager.LoadShader("Asset/Shaders/color.vs", ShaderType::Vertex);
    m_ShaderManager.LoadShader("Asset/Shaders/color.ps", ShaderType::Pixel);
    m_ShaderManager.LoadShader("Asset/Shaders/debug.vs", ShaderType::Vertex);
    m_ShaderManager.LoadShader("Asset/Shaders/debug.ps", ShaderType::Pixel);

    // ShaderVariables
    auto root_sig = std::make_shared<RootSignature>("Color root signature");
    (*root_sig)
        .Add("FrameConstant", ShaderVariableType::CBV, 0, 0)
        .Add("ObjectConstants", ShaderVariableType::CBV, 1, 0)
        // textures
        .Add("AmbientTexture", ShaderVariableType::SRV, 0, 0)
        .Add("DiffuseTexture", ShaderVariableType::SRV, 1, 0)
        .Add("EmissionTexture", ShaderVariableType::SRV, 2, 0)
        .Add("SpecularTexture", ShaderVariableType::SRV, 3, 0)
        .Add("PowerTexture", ShaderVariableType::SRV, 4, 0)
        // sampler
        .Add("BaseSampler", ShaderVariableType::Sampler, 0, 0)
        .Create(*m_Driver);

    // TODO use AssetManager to manage pipeline state object
    m_PSO = std::make_unique<PipelineState>("Color");
    (*m_PSO)
        .SetInputLayout({{"POSITION", 0, Format::R32G32B32_FLOAT, 0, 0},
                         {"NORMAL", 0, Format::R32G32B32_FLOAT, 1, 0},
                         {"TEXCOORD", 0, Format::R32G32_FLOAT, 2, 0}})
        .SetVertexShader(m_ShaderManager.GetVertexShader("color.vs"))
        .SetPixelShader(m_ShaderManager.GetPixelShader("color.ps"))
        .SetRootSignautre(root_sig)
        .SetPrimitiveType(PrimitiveType::TriangleList)
        .SetRenderFormat(Format::R8G8B8A8_UNORM)
        .SetDepthBufferFormat(Format::D32_FLOAT)
        .Create(*m_Driver);

    auto debug_root_sig = std::make_shared<RootSignature>("Debug sig");
    (*debug_root_sig)
        .Add("FrameConstant", ShaderVariableType::CBV, 0, 0)
        .Add("ObjectConstants", ShaderVariableType::CBV, 1, 0)
        .Create(*m_Driver);

    m_DebugPSO = std::make_unique<PipelineState>("Debug");
    (*m_DebugPSO)
        .SetInputLayout({
            {"POSITION", 0, Format::R32G32B32_FLOAT, 0, 0},
        })
        .SetVertexShader(m_ShaderManager.GetVertexShader("debug.vs"))
        .SetPixelShader(m_ShaderManager.GetPixelShader("debug.ps"))
        .SetPrimitiveType(PrimitiveType::LineList)
        .SetRootSignautre(debug_root_sig)
        .SetRenderFormat(Format::R8G8B8A8_UNORM)
        .SetDepthBufferFormat(Format::D32_FLOAT)
        .Create(*m_Driver);

    return 0;
}

void GraphicsManager::Finalize() {
    m_Logger->info("Finalized.");

    // Release all resource
    {
        m_Driver->IdleGPU();

        m_PSO      = nullptr;
        m_DebugPSO = nullptr;
        m_ResMgr   = nullptr;
        for (auto&& frame : m_Frames)
            frame = nullptr;

        m_Driver = nullptr;
    }
    backend::DX12::DX12DriverAPI::ReportDebugLog();

    // Release shader resource
    m_ShaderManager.Finalize();

    m_Logger = nullptr;
}

void GraphicsManager::Tick() {
    const Asset::Scene& scene = g_SceneManager->GetSceneForRendering();
    // TODO change the parameter to View, if multiple view port is finished
    // views = g_App->GetViewsForRendering();
    // rendertargets =  views.foreach(view : Render(view))
    // ...
    Render(scene);
    m_Driver->Present(m_CurrBackBuffer);
    m_CurrBackBuffer = (m_CurrBackBuffer + 1) % sm_SwapChianSize;
}

void GraphicsManager::Render(const Asset::Scene& scene) {
    auto& config    = g_App->GetConfiguration();
    auto  driver    = m_Driver.get();
    auto& pso       = *m_PSO;
    auto& debug_pso = *m_DebugPSO;
    auto  frame     = GetBcakFrameForRendering();
    auto  context   = driver->GetGraphicsCommandContext();

    auto     camera = scene.GetFirstCameraNode();
    uint32_t width  = config.screen_width;
    uint32_t height = config.screen_width / camera->GetSceneObjectRef().lock()->GetAspect();
    // make view port vertical align
    context->SetViewPort(0, (config.screen_height - height) >> 1, width, height);

    frame->AddGeometries(scene.GetGeometries(), pso);
    frame->AddDebugPrimitives(g_DebugManager->GetDebugPrimitiveForRender(), debug_pso);
    frame->SetCamera(*camera);
    frame->SetLight(*scene.GetFirstLightNode());

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
                DepthBuffer::Description{
                    .format        = Format::D32_FLOAT,
                    .width         = width,
                    .height        = height,
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
            context->SetRenderTargetAndDepthBuffer(render_target, depth_buffer);
            context->ClearRenderTarget(render_target);
            context->ClearDepthBuffer(depth_buffer);

            frame->Draw(context.get());
        });

    struct DebugPassData {
        FrameHandle depth;
        FrameHandle output;
    };

    auto debug_pass = fg.AddPass<DebugPassData>(
        "DebugPass",
        [&](FrameGraph::Builder& builder, DebugPassData& data) {
            data.depth  = builder.Read(color_pass.GetData().depth_buffer);
            data.output = builder.Read(color_pass.GetData().output);
            data.output = builder.Write(data.output);
        },
        [=](const ResourceHelper& helper, DebugPassData& data) {
            auto& depth_buffer  = helper.Get<DepthBuffer>(data.depth);
            auto& render_target = helper.Get<RenderTarget>(data.output);
            context->SetRenderTargetAndDepthBuffer(render_target, depth_buffer);

            frame->DebugDraw(context.get());
        });

    fg.Present(render_target_handle, context);

    fg.Compile();

    fg.Execute(*driver);
    uint64_t fence = context->Finish();
    frame->SetFenceValue(fence);
    fg.Retire(fence, *driver);
}

Frame* GraphicsManager::GetBcakFrameForRendering() {
    auto frame = m_Frames.at(m_CurrBackBuffer).get();
    frame->ResetState();
    return frame;
}

}  // namespace Hitagi::Graphics