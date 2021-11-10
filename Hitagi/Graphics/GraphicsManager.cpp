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
    m_Driver->CreateSwapChain(config.screenWidth, config.screenHeight, sm_SwapChianSize, Format::R8G8B8A8_UNORM, g_App->GetWindow());
    for (size_t index = 0; index < sm_SwapChianSize; index++)
        m_Frames.at(index) = std::make_unique<Frame>(*m_Driver, *m_ResMgr, index);

    // Initialize Shader Manader
    m_ShaderManager.Initialize();

    // TODO get and load all shader from AssetManager
    m_ShaderManager.LoadShader("Asset/Shaders/color.vs", ShaderType::VERTEX);
    m_ShaderManager.LoadShader("Asset/Shaders/color.ps", ShaderType::PIXEL);
    m_ShaderManager.LoadShader("Asset/Shaders/debug.vs", ShaderType::VERTEX);
    m_ShaderManager.LoadShader("Asset/Shaders/debug.ps", ShaderType::PIXEL);

    // ShaderVariables
    auto rootSig = std::make_shared<RootSignature>("Color root signature");
    (*rootSig)
        .Add("FrameConstant", ShaderVariableType::CBV, 0, 0)
        .Add("ObjectConstants", ShaderVariableType::CBV, 1, 0)
        .Add("MaterialConstants", ShaderVariableType::CBV, 2, 0)
        // textures
        .Add("AmbientTexture", ShaderVariableType::SRV, 0, 0)
        .Add("DiffuseTexture", ShaderVariableType::SRV, 1, 0)
        .Add("EmissionTexture", ShaderVariableType::SRV, 2, 0)
        .Add("SpecularTexture", ShaderVariableType::SRV, 3, 0)
        .Add("PowerTexture", ShaderVariableType::SRV, 4, 0)
        // sampler
        .Add("BaseSampler", ShaderVariableType::Sampler, 0, 0)
        .Create(*m_Driver);

    m_PSO = std::make_unique<PipelineState>("Color");
    (*m_PSO)
        .SetInputLayout({{"POSITION", 0, Format::R32G32B32_FLOAT, 0, 0},
                         {"NORMAL", 0, Format::R32G32B32_FLOAT, 1, 0},
                         {"TEXCOORD", 0, Format::R32G32_FLOAT, 2, 0}})
        .SetVertexShader(m_ShaderManager.GetVertexShader("color.vs"))
        .SetPixelShader(m_ShaderManager.GetPixelShader("color.ps"))
        .SetRootSignautre(rootSig)
        .SetRenderFormat(Format::R8G8B8A8_UNORM)
        .SetDepthBufferFormat(Format::D32_FLOAT)
        .Create(*m_Driver);

    auto debug_rootSig = std::make_shared<RootSignature>("Debug sig");
    (*debug_rootSig)
        .Add("FrameConstant", ShaderVariableType::CBV, 0, 0)
        .Add("PrimitiveConstant", ShaderVariableType::CBV, 1, 0)
        .Create(*m_Driver);

    m_DebugPSO = std::make_unique<PipelineState>("Debug");
    (*m_DebugPSO)
        .SetInputLayout({{"POSITION", 0, Format::R32G32B32_FLOAT, 0, 0},
                         {"COLOR", 0, Format::R32G32B32A32_FLOAT, 1, 0}})
        .SetVertexShader(m_ShaderManager.GetVertexShader("debug.vs"))
        .SetPixelShader(m_ShaderManager.GetPixelShader("debug.ps"))
        .SetRootSignautre(debug_rootSig)
        .SetRenderFormat(Format::R8G8B8A8_UNORM)
        .Create(*m_Driver);

    // TODO end

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
    auto  resMgr    = m_ResMgr.get();
    auto  pso       = m_PSO.get();
    auto  debug_pso = m_DebugPSO.get();
    auto  frame     = GetBcakFrameForRendering();
    auto  context   = driver->GetGraphicsCommandContext();

    auto     camera = scene.GetFirstCameraNode();
    uint32_t width  = config.screenWidth;
    uint32_t height = config.screenWidth / camera->GetSceneObjectRef().lock()->GetAspect();
    // make view port vertical align
    context->SetViewPort(0, (config.screenHeight - height) >> 1, width, height);

    frame->SetGeometries(scene.GetGeometries());
    frame->SetCamera(*camera);
    frame->SetLight(*scene.GetFirstLightNode());

    FrameGraph fg;

    auto renderTargetHandle = fg.Import(&frame->GetRenderTarget());

    struct ColorPassData {
        FrameHandle depthBuffer;
        FrameHandle output;
    };

    // color pass
    auto colorPass = fg.AddPass<ColorPassData>(
        "ColorPass",
        // Setup function
        [&](FrameGraph::Builder& builder, ColorPassData& data) {
            data.depthBuffer = builder.Create<DepthBuffer>(
                "DepthBuffer",
                DepthBuffer::Description{
                    .format       = Format::D32_FLOAT,
                    .width        = width,
                    .height       = height,
                    .clearDepth   = 1.0f,
                    .clearStencil = 0,
                });
            data.depthBuffer = builder.Write(data.depthBuffer);
            data.output      = builder.Write(renderTargetHandle);
        },
        // Excute function
        [=](const ResourceHelper& helper, ColorPassData& data) {
            auto& depthBuffer  = helper.Get<DepthBuffer>(data.depthBuffer);
            auto& renderTarget = helper.Get<RenderTarget>(data.output);
            context->SetRenderTargetAndDepthBuffer(renderTarget, depthBuffer);
            context->ClearRenderTarget(renderTarget);
            context->ClearDepthBuffer(depthBuffer);

            context->SetPipelineState(*pso);
            context->SetParameter("BaseSampler", resMgr->GetSampler("BaseSampler"));
            frame->Draw(context.get());
        });

    struct DebugPassData {
        FrameHandle depth;
        FrameHandle output;
    };

    // auto debugPass = fg.AddPass<DebugPassData>(
    //     "DebugPass",
    //     [&](FrameGraph::Builder& builder, DebugPassData& data) {
    //         data.depth  = builder.Read(colorPass.GetData().depthBuffer);
    //         data.output = builder.Read(colorPass.GetData().output);
    //         data.output = builder.Write(data.output);
    //     },
    //     [=](const ResourceHelper& helper, DebugPassData& data) {
    //         auto& deepthBuffer = helper.Get<DepthBuffer>(data.depth);
    //         auto& renderTarget = helper.Get<RenderTarget>(data.output);

    //         context->SetPipelineState(*debug_pso);
    //     });

    fg.Present(renderTargetHandle, context);

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