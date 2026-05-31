#include "test_macros.hpp"
#include <spdlog/spdlog.h>
#include <tracy/Tracy.hpp>
#include "imgui.h"

import magic_enum;
import render;
import asset;
import core;
import utils;
import math;
import gui;
import app;
import test_utils;

using namespace testing;
using namespace hitagi;
using namespace hitagi::render;

class PassthroughRenderer final : public IRenderer {
public:
    PassthroughRenderer() : IRenderer("PassthroughRenderer") {}

    auto Render(RenderContext&, const SceneView&, rg::TextureHandle target) -> rg::TextureHandle override {
        return target;
    }
};

class CustomFullscreenPass {
public:
    auto Build(RenderContext& context, rg::TextureHandle input) -> rg::TextureHandle {
        auto& graph  = context.graph;
        auto  output = graph.MoveFrom(input, "CustomFullscreenPassOutput");

        rg::RenderPassBuilder(graph)
            .SetName("CustomFullscreenPass")
            .SetRenderTarget(output, false)
            .SetExecutor([output](const rg::RenderGraph&, const rg::RenderPassNode& pass) {
                auto&       cmd           = pass.GetCmd();
                const auto& render_target = pass.Resolve(output);
                cmd.SetViewPort({
                    .x      = 0,
                    .y      = 0,
                    .width  = static_cast<float>(render_target.GetDesc().width),
                    .height = static_cast<float>(render_target.GetDesc().height),
                });
                cmd.SetScissorRect({
                    .x      = 0,
                    .y      = 0,
                    .width  = render_target.GetDesc().width,
                    .height = render_target.GetDesc().height,
                });
            })
            .Finish();

        return output;
    }
};

class CustomPassRenderer final : public IRenderer {
public:
    CustomPassRenderer() : IRenderer("CustomPassRenderer") {}

    auto Render(RenderContext& context, const SceneView&, rg::TextureHandle target) -> rg::TextureHandle override {
        return m_CustomPass.Build(context, target);
    }

private:
    CustomFullscreenPass m_CustomPass;
};

constexpr std::array supported_device_types = {
#ifdef _WIN32
    gfx::Device::Type::DX12,
#endif
    gfx::Device::Type::Vulkan,
};

class RendererTest : public TestWithParam<gfx::Device::Type> {
protected:
    RendererTest()
        : test_name(UnitTest::GetInstance()->current_test_info()->name()),
          app(Application::CreateApp(AppConfig{
              .gfx_backend = std::pmr::string(magic_enum::enum_name(GetParam())),
              .headless    = true,
          })),
          device(gfx::create_device(GetParam())) {}

    std::string                  test_name;
    std::unique_ptr<Application> app;
    std::unique_ptr<gfx::Device> device;
};
INSTANTIATE_TEST_SUITE_P(
    RendererTest,
    RendererTest,
    ValuesIn(supported_device_types),
    [](const TestParamInfo<gfx::Device::Type>& info) -> std::string {
        return std::string{magic_enum::enum_name(info.param)};
    });

TEST_P(RendererTest, DeferredRenderer) {
    auto            gui_manager = std::make_unique<gui::GuiManager>(*app);
    RenderRuntime   runtime(*device, *app, test_name);
    DefaultRenderer renderer(*device, *app, test_name);

    asset::AssetManager asset_manager("./assets");

    auto scene = asset_manager.ImportScene("assets/test/test.usda");

    std::size_t frame_index = 0;
    while (!app->IsQuit()) {
        auto texture = runtime.GetRenderGraph().Create(gfx::TextureDesc{
            .name        = std::pmr::string{std::format("RenderTarget-{}", frame_index)},
            .width       = runtime.GetSwapChain().GetWidth(),
            .height      = runtime.GetSwapChain().GetHeight(),
            .format      = gfx::Format::R8G8B8A8_UNORM,
            .clear_value = math::Color(0.0, 0.0, 0.0, 1.0),
            .usages      = gfx::TextureUsageFlags::RenderTarget | gfx::TextureUsageFlags::CopySrc,
        });

        gui_manager->DrawGui([=]() {
            auto& light_transform  = scene->GetLightEntities().front().Get<asset::Transform>();
            auto& camera_transform = scene->GetCameraEntities().front().Get<asset::Transform>();
            auto& cube_transform   = scene->GetMeshEntities().front().Get<asset::Transform>();

            ImGui::DragFloat3("Light Position", light_transform.position, 0.1f);
            ImGui::DragFloat3("Camera Position", camera_transform.position, 0.1f);
            ImGui::DragFloat3("cube Position", cube_transform.position, 0.1f);
        });
        gui_manager->Tick();

        const auto camera           = scene->GetCameraEntities().front().Get<asset::CameraComponent>().camera;
        const auto camera_transform = scene->GetCameraEntities().front().Get<asset::Transform>();

        camera->parameters.aspect = static_cast<float>(runtime.GetSwapChain().GetWidth()) / static_cast<float>(runtime.GetSwapChain().GetHeight());

        scene->Update();
        auto context = runtime.MakeContext();
        const auto scene_output = renderer.Render(
            context,
            SceneView{
                .scene            = scene,
                .camera           = camera.get(),
                .camera_transform = camera_transform.world_matrix,
            },
            texture);
        EXPECT_EQ(scene_output, texture);
        texture = runtime.GetRenderGraph().MoveFrom(texture);
        runtime.RenderGui(texture, gui_manager->GetDrawData(), false);
        runtime.ToSwapChain(texture);
        runtime.Tick();

        app->Tick();

        FrameMark;

        if (frame_index++ == 3) {
            break;
        }
    }
    asset::Texture::DestroyDefaultTexture();
}

TEST(RendererInterfaceTest, CustomRendererOnlyImplementsSceneRender) {
    PassthroughRenderer renderer;
    auto                mock_device = gfx::create_device(gfx::Device::Type::Mock, "CustomRendererInterface");
    rg::RenderGraph     graph(*mock_device, "CustomRendererInterfaceGraph");

    RenderContext context{
        .device = *mock_device,
        .graph  = graph,
    };
    EXPECT_EQ(renderer.Render(context, SceneView{}, {}), rg::TextureHandle{});
}

TEST(RendererInterfaceTest, CustomRendererCanComposeCustomRenderGraphPass) {
    CustomPassRenderer renderer;
    auto               mock_device = gfx::create_device(gfx::Device::Type::Mock, "CustomPassRenderer");
    rg::RenderGraph    graph(*mock_device, "CustomPassRendererGraph");
    auto               input = graph.Create(gfx::TextureDesc{
        .name        = "CustomRendererInput",
        .width       = 16,
        .height      = 16,
        .format      = gfx::Format::R8G8B8A8_UNORM,
        .clear_value = math::Color::Black(),
        .usages      = gfx::TextureUsageFlags::RenderTarget,
    });
    RenderContext context{
        .device = *mock_device,
        .graph  = graph,
    };

    const auto output = renderer.Render(context, SceneView{}, input);

    EXPECT_NE(output, input);
    EXPECT_TRUE(graph.IsValid(output));
}
