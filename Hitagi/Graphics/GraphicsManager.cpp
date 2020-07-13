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
    m_Driver->CreateSwapChain(config.screenWidth, config.screenHeight, m_Frame.size(), Format::R8G8B8A8_UNORM, g_App->GetWindow());
    for (size_t frameIndex = 0; frameIndex < m_Frame.size(); frameIndex++)
        m_Frame[frameIndex] = std::make_unique<Frame>(*m_Driver, *m_ResMgr, frameIndex);

    // Initialize Shader Manader
    m_ShaderManager.Initialize();
    m_ShaderManager.LoadShader("Asset/Shaders/color.vs", ShaderType::VERTEX);
    m_ShaderManager.LoadShader("Asset/Shaders/color.ps", ShaderType::PIXEL);

    // ShaderVariables
    auto rootSig = std::make_shared<RootSignature>();
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

    m_PSO     = std::make_unique<PipelineState>("Color");
    auto& pso = *m_PSO;
    pso.SetInputLayout({{"POSITION", 0, Format::R32G32B32_FLOAT, 0, 0},
                        {"NORMAL", 0, Format::R32G32B32_FLOAT, 1, 0},
                        {"TEXCOORD", 0, Format::R32G32_FLOAT, 2, 0}})
        .SetVertexShader(m_ShaderManager.GetVertexShader("color.vs"))
        .SetPixelShader(m_ShaderManager.GetPixelShader("color.ps"))
        .SetRootSignautre(rootSig)
        .SetRenderFormat(Format::R8G8B8A8_UNORM)
        .Create(*m_Driver);

    return 0;
}

void GraphicsManager::Finalize() {
    m_Logger->info("Finalized.");

    // Release all resource
    {
        m_Driver->IdleGPU();

        m_PSO    = nullptr;
        m_ResMgr = nullptr;
        for (auto&& frame : m_Frame)
            frame = nullptr;

        m_Driver = nullptr;
    }
    backend::DX12::DX12DriverAPI::ReportDebugLog();

    // Release shader resource
    m_ShaderManager.Finalize();

    m_Logger = nullptr;
}

void GraphicsManager::Tick() {
    Render(g_SceneManager->GetSceneForRendering());
    m_Driver->Present(m_CurrBackBuffer);
    m_CurrBackBuffer = (m_CurrBackBuffer + 1) % m_Frame.size();
}

void GraphicsManager::Render(const Asset::Scene& scene) {
    auto& config  = g_App->GetConfiguration();
    auto  driver  = m_Driver.get();
    auto  resMgr  = m_ResMgr.get();
    auto  frame   = m_Frame[m_CurrBackBuffer].get();
    auto  pso     = m_PSO.get();
    auto  context = driver->GetGraphicsCommandContext();

    auto     camera = scene.GetFirstCameraNode();
    uint32_t h      = config.screenWidth / camera->GetSceneObjectRef().lock()->GetAspect();
    uint32_t y      = (config.screenHeight - h) >> 1;
    context->SetViewPort(0, y, config.screenWidth, h);

    frame->SetGeometries(scene.GetGeometries());
    frame->SetCamera(*camera);
    frame->SetLight(*scene.GetFirstLightNode());
    FrameGraph fg(*driver);

    struct PassData {
        FrameHandle depthBuffer;
    };

    // color pass
    auto colorPass = fg.AddPass<PassData>(
        "ColorPass",
        // Setup function
        [&](FrameGraph::Builder& builder, PassData& data) {
            // data.depthBuffer = builder.Create<DepthBuffer>(DepthBuffer::Description{
            //     Format::D32_FLOAT,
            //     config.screenWidth,
            //     config.screenWidth,
            //     1.0f,
            //     0});
            // data.depthBuffer = builder.Write(data.depthBuffer);
        },
        // Excute function
        [=](const ResourceHelper& helper, PassData& data) {
            context->SetRenderTarget(frame->GetRenerTarget());
            context->SetPipelineState(*pso);
            context->SetParameter("BaseSampler", resMgr->GetSampler("BaseSampler"));
            frame->Draw(context.get());
            context->Present(frame->GetRenerTarget());
        });

    fg.Compile();
    fg.Execute();
    fg.Retire(context->Finish());
}

}  // namespace Hitagi::Graphics