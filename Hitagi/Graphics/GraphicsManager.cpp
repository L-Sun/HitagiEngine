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
    m_Driver->CreateSwapChain(config.screenWidth, config.screenHeight, m_Frame.size(), Format::R8G8B8A8_UNORM);
    for (size_t frameIndex = 0; frameIndex < m_Frame.size(); frameIndex++)
        m_Frame[frameIndex] = std::make_unique<Frame>(*m_Driver, *m_ResMgr, frameIndex);

    // Initialize Shader Manader
    m_ShaderManager.Initialize();
    m_ShaderManager.LoadShader("Asset/Shaders/simple_vs.cso", ShaderType::VERTEX, "color_vs");
    m_ShaderManager.LoadShader("Asset/Shaders/simple_ps_1.cso", ShaderType::PIXEL, "color_ps");

    // ShaderVariables
    auto rootSig = std::make_shared<RootSignature>();
    (*rootSig)
        .Add("FrameConstant", ShaderVariableType::CBV, 0, 0)
        .Add("ObjectConstants", ShaderVariableType::CBV, 1, 0)
        .Add("MaterialConstants", ShaderVariableType::CBV, 2, 0)
        // .Add("BaseSampler", ShaderVariableType::Sampler, 0, 0)
        // .Add("BaseMap", ShaderVariableType::SRV, 0, 0)
        .Create(*m_Driver);

    m_PSO     = std::make_unique<PipelineState>("Color");
    auto& pso = *m_PSO;
    pso.SetInputLayout({{"POSITION", 0, Format::R32G32B32_FLOAT, 0, 0},
                        {"NORMAL", 0, Format::R32G32B32_FLOAT, 1, 0},
                        {"TEXCOORD", 0, Format::R32G32_FLOAT, 2, 0}})
        .SetVertexShader(m_ShaderManager.GetVertexShader("color_vs"))
        .SetPixelShader(m_ShaderManager.GetPixelShader("color_ps"))
        .SetRootSignautre(rootSig)
        .SetRenderFormat(Format::R8G8B8A8_UNORM)
        .SetDepthBufferFormat(Format::D32_FLOAT)
        .Create(*m_Driver);
    return 0;
}

void GraphicsManager::Finalize() {
    m_Logger->info("Finalized.");

    m_PSO = nullptr;

    // Rlease frame resource before destruction of backend API
    m_ResMgr = nullptr;
    for (auto&& frame : m_Frame)
        frame = nullptr;
    // Will Release the resource allcateb by backend driver.
    // If not release, a error will occur in the porgram exit:
    // ~GraphicsManager -> ~m_Driver -> static member destory function
    // but the static member may has been destructed before ~GraphicsManager
    // so call static member destory function is invalid.
    m_Driver = nullptr;
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
    auto& config = g_App->GetConfiguration();
    auto  driver = m_Driver.get();
    auto  frame  = m_Frame[m_CurrBackBuffer].get();
    auto  pso    = m_PSO.get();

    frame->SetGeometries(scene.GetGeometries());
    frame->SetCamera(*scene.GetFirstCameraNode());
    frame->SetLight(*scene.GetFirstLightNode());
    FrameGraph fg(*driver);

    struct PassData {
        FrameHandle depthBuffer;
    };

    // color pass
    auto colorPass = fg.AddPass<PassData>(
        "Test",
        // Setup function
        [&](FrameGraph::Builder& builder, PassData& data) {
            data.depthBuffer = builder.Create<DepthBuffer>(DepthBuffer::Description{
                Format::D32_FLOAT,
                config.screenWidth,
                config.screenWidth,
                1.0f,
                0});
        },
        // Excute function
        [=](const ResourceHelper& helper, PassData& data) {
            auto context = driver->GetGraphicsCommandContext();
            context->SetRenderTargetAndDepthBuffer(frame->GetRenerTarget(), helper.Get<DepthBuffer>(data.depthBuffer));
            context->SetViewPort(0, 0, config.screenWidth, config.screenHeight);
            context->SetPipelineState(*pso);
            frame->Draw(context.get());
            context->Present(frame->GetRenerTarget());
            context->Finish(true);
        });

    colorPass.GetData();

    fg.Compile();
    fg.Execute();
}

}  // namespace Hitagi::Graphics