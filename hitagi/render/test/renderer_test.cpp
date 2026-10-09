#include "test_macros.hpp"
#include "interop/tracy_macros.hpp"
import interop.gtest;
import interop.spdlog;
import interop.tracy;
import interop.magic_enum;

import std;
import render;
import asset;
import core;
import utils;
import math;
import app;
import test_utils;

using namespace testing;
using namespace hitagi;
using namespace hitagi::render;

class PassthroughRenderer final : public IRenderer {
public:
    PassthroughRenderer() : IRenderer("PassthroughRenderer") {}

    auto Render(RenderContext&, const RenderRequest& request) -> RenderResult override {
        return RenderResult{.color = request.target};
    }
};

class CustomFullscreenPass {
public:
    auto Build(RenderContext& context, rg::TextureHandle input) -> rg::TextureHandle {
        auto& graph  = context.graph;
        auto  output = graph.MoveFrom(input, "CustomFullscreenPassOutput");

        rg::RenderPassBuilder pass_builder(graph);
        pass_builder.SetName("CustomFullscreenPass");
        pass_builder.SetRenderTarget(output, false);
        pass_builder.SetExecutor([output](const rg::RenderGraph&, const rg::RenderPassNode& pass) {
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
        });
        pass_builder.Finish();

        return output;
    }
};

class CustomPassRenderer final : public IRenderer {
public:
    CustomPassRenderer() : IRenderer("CustomPassRenderer") {}

    auto Render(RenderContext& context, const RenderRequest& request) -> RenderResult override {
        return RenderResult{.color = m_CustomPass.Build(context, request.target)};
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
              .log_level   = spdlog::level::to_string_view(spdlog::get_level()).data(),
              .headless    = true,
          })),
          device(gfx::create_device(GetParam())) {}

    std::string                  test_name;
    core::FileIOManager          file_io;
    std::unique_ptr<Application> app;
    std::unique_ptr<gfx::Device> device;
    hitagi::gfx::CommandQueues                  queues{*device};
    std::unique_ptr<hitagi::gfx::BindlessUtils> bindings = hitagi::gfx::BindlessUtils::Create(*device);
    hitagi::gfx::ShaderCompiler                 compiler{"Tests"};
};
INSTANTIATE_TEST_SUITE_P(
    RendererTest,
    RendererTest,
    ValuesIn(supported_device_types),
    [](const TestParamInfo<gfx::Device::Type>& info) -> std::string {
        return std::string{magic_enum::enum_name(info.param)};
    });

TEST_P(RendererTest, DeferredRendererAcceptsExplicitFrame) {
    RenderRuntime   runtime(*device, queues, *bindings, compiler, *app, test_name);
    DefaultRenderer renderer(*device, queues, *bindings, compiler, file_io, *app, test_name);

    const std::array debug_views{
        RenderGraphDebugView::Final,
        RenderGraphDebugView::BaseColor,
        RenderGraphDebugView::Normal,
        RenderGraphDebugView::Metallic,
        RenderGraphDebugView::Roughness,
        RenderGraphDebugView::Occlusion,
        RenderGraphDebugView::MaterialId,
        RenderGraphDebugView::Emissive,
        RenderGraphDebugView::Final,
    };
    std::size_t frame_index = 0;
    while (!app->IsQuit()) {
        const auto width   = runtime.GetSwapChain().GetWidth();
        const auto height  = runtime.GetSwapChain().GetHeight();
        auto       texture = runtime.GetRenderGraph().Create(gfx::TextureDesc{
            .name        = std::pmr::string{std::format("RenderTarget-{}", frame_index)},
            .width       = width,
            .height      = height,
            .format      = gfx::Format::R8G8B8A8_UNORM,
            .clear_value = math::Color(0.0, 0.0, 0.0, 1.0),
            .usages      = gfx::TextureUsageFlags::RenderTarget | gfx::TextureUsageFlags::CopySrc,
        });

        const math::vec3f eye{0.0f, -4.0f, 2.0f};
        const RenderFrame frame{
            .view = RenderView{
                .camera_position = eye,
                .view            = math::look_at(eye, math::vec3f{0.0f, 1.0f, -0.35f}, math::vec3f{0.0f, 0.0f, 1.0f}),
                .projection      = math::perspective(60.0_deg, static_cast<float>(width) / static_cast<float>(height), 0.1f, 100.0f),
            },
        };

        auto       context      = runtime.MakeContext();
        const auto frame_output = renderer.Render(
            context,
            RenderRequest{
                .frame      = frame,
                .target     = texture,
                .debug_view = debug_views[frame_index],
            });
        EXPECT_EQ(frame_output.color, texture);
        EXPECT_TRUE(frame_output.depth);
        EXPECT_TRUE(frame_output.linear_depth);
        EXPECT_TRUE(frame_output.normal);
        const auto graph_dot = runtime.GetRenderGraph().ToDot();
        if (debug_views[frame_index] == RenderGraphDebugView::Final) {
            EXPECT_EQ(graph_dot.find("DeferredDebugViewPass-"), std::pmr::string::npos);
            EXPECT_NE(graph_dot.find("DeferredLightingPass-"), std::pmr::string::npos);
        } else {
            EXPECT_NE(graph_dot.find(std::format("DeferredDebugViewPass-{}-", RenderGraphDebugViewName(debug_views[frame_index]))),
                      std::pmr::string::npos);
            EXPECT_EQ(graph_dot.find("DeferredLightingPass-"), std::pmr::string::npos);
        }
        texture = runtime.GetRenderGraph().MoveFrom(texture);
        runtime.ToSwapChain(texture);
        runtime.Tick();

        app->Tick();

        FrameMark;

        if (++frame_index == debug_views.size()) {
            break;
        }
    }
}

TEST(RendererInterfaceTest, CustomRendererOnlyImplementsSceneRender) {
    PassthroughRenderer renderer;
    auto                mock_device = gfx::create_device(gfx::Device::Type::Mock, "CustomRendererInterface");
    hitagi::gfx::CommandQueues  queues(*mock_device);
    auto                        bindings = hitagi::gfx::BindlessUtils::Create(*mock_device);
    hitagi::gfx::ShaderCompiler compiler{"Tests"};
    rg::RenderGraph             graph(*mock_device, queues, *bindings, "CustomRendererInterfaceGraph");

    RenderContext context{
        .device = *mock_device,
        .graph  = graph,
    };
    EXPECT_EQ(renderer.Render(context, RenderRequest{.frame = RenderFrame{}, .target = {}}).color, rg::TextureHandle{});
}

TEST(RendererInterfaceTest, CustomRendererCanComposeCustomRenderGraphPass) {
    CustomPassRenderer renderer;
    auto               mock_device = gfx::create_device(gfx::Device::Type::Mock, "CustomPassRenderer");
    hitagi::gfx::CommandQueues  queues(*mock_device);
    auto                        bindings = hitagi::gfx::BindlessUtils::Create(*mock_device);
    hitagi::gfx::ShaderCompiler compiler{"Tests"};
    rg::RenderGraph             graph(*mock_device, queues, *bindings, "CustomPassRendererGraph");
    auto               input = graph.Create(gfx::TextureDesc{
        .name        = "CustomRendererInput",
        .width       = 16,
        .height      = 16,
        .format      = gfx::Format::R8G8B8A8_UNORM,
        .clear_value = math::Color::Black(),
        .usages      = gfx::TextureUsageFlags::RenderTarget,
    });
    RenderContext      context{
        .device = *mock_device,
        .graph  = graph,
    };

    const auto output = renderer.Render(context, RenderRequest{.frame = RenderFrame{}, .target = input});

    EXPECT_NE(output.color, input);
    EXPECT_TRUE(graph.IsValid(output.color));
}

TEST(RendererMaterialPassTest, PassParticipationRoutesMaterialContracts) {
    const auto make_material = [](std::initializer_list<std::string_view> contracts) {
        std::pmr::vector<asset::MaterialPass> passes;
        for (const auto contract : contracts) {
            passes.emplace_back(asset::MaterialPass{.pass_contract = std::pmr::string(contract)});
        }
        return asset::Material({}, std::move(passes));
    };

    const auto pbr = GetMaterialPassParticipation(make_material({"DepthPrepass", "GBuffer", "ShadowCaster"}));
    EXPECT_EQ(pbr.queue, RenderQueue::Opaque);
    EXPECT_TRUE(pbr.IsInLayer(RenderLayer::Default));
    EXPECT_TRUE(pbr.Participates(MaterialPass::DepthPrepass));
    EXPECT_TRUE(pbr.Participates(MaterialPass::GBuffer));
    EXPECT_TRUE(pbr.Participates(MaterialPass::ShadowCaster));
    EXPECT_FALSE(pbr.Participates(MaterialPass::Forward));

    const auto forward = GetMaterialPassParticipation(make_material({"DepthPrepass", "Forward"}));
    EXPECT_TRUE(forward.Participates(MaterialPass::DepthPrepass));
    EXPECT_TRUE(forward.Participates(MaterialPass::Forward));
    EXPECT_FALSE(forward.Participates(MaterialPass::GBuffer));

    const auto transparent = GetMaterialPassParticipation(make_material({"ForwardTransparent"}));
    EXPECT_EQ(transparent.queue, RenderQueue::Transparent);
    EXPECT_FALSE(transparent.Participates(MaterialPass::DepthPrepass));
    EXPECT_FALSE(transparent.Participates(MaterialPass::ShadowCaster));
    EXPECT_TRUE(transparent.Participates(MaterialPass::Forward));

    const auto custom = GetMaterialPassParticipation(make_material({"Forward", "Debug"}));
    EXPECT_TRUE(custom.Participates(MaterialPass::Forward));
    EXPECT_TRUE(custom.Participates(MaterialPass::Debug));

    const auto toon = GetMaterialPassParticipation(make_material({"ToonGBuffer", "ToonBase", "ToonLighting", "ToonComposite", "OutlineMask"}));
    EXPECT_TRUE(toon.IsInLayer(RenderLayer::Toon));
    EXPECT_TRUE(toon.IsInLayer(RenderLayer::Outline));
    EXPECT_TRUE(toon.Participates(MaterialPass::GBuffer));
    EXPECT_TRUE(toon.Participates(MaterialPass::ToonBase));
    EXPECT_TRUE(toon.Participates(MaterialPass::ToonLighting));
    EXPECT_TRUE(toon.Participates(MaterialPass::ToonComposite));
    EXPECT_TRUE(toon.Participates(MaterialPass::Outline));
    EXPECT_FALSE(toon.Participates(MaterialPass::Forward));
}

TEST(RendererMaterialPassTest, ToonRenderPathAndOutlineDefaultsAreExplicit) {
    const auto toon_path = MakeDefaultToonRenderPathDesc();
    EXPECT_TRUE(toon_path.enabled);
    EXPECT_TRUE(toon_path.ramp_lighting);
    EXPECT_TRUE(toon_path.quantized_shadows);
    EXPECT_TRUE(toon_path.vertex_shadow_weight);
    EXPECT_TRUE(toon_path.rim_light);
    EXPECT_TRUE(toon_path.matcap);
    EXPECT_TRUE(toon_path.emission);
    EXPECT_TRUE(toon_path.face_shadow);
    EXPECT_EQ(toon_path.debug_view, ToonDebugView::None);

    const auto outline = MakeDefaultToonOutlineDesc();
    EXPECT_TRUE(outline.Enabled());
    EXPECT_EQ(outline.mode, OutlineMode::Hybrid);
    EXPECT_TRUE(outline.use_material_width);
    EXPECT_TRUE(outline.use_material_color);
    EXPECT_TRUE(outline.use_vertex_color_width);
    EXPECT_TRUE(outline.distance_scale);
}

TEST(RendererMaterialPassTest, RenderQueueKeySortsByQueuePriorityLayerAndObject) {
    const std::array keys = {
        RenderQueueKey{.queue = RenderQueue::Transparent, .layers = RenderLayerBit(RenderLayer::Default), .queue_priority = 0, .object_id = 1},
        RenderQueueKey{.queue = RenderQueue::Opaque, .layers = RenderLayerBit(RenderLayer::Outline), .queue_priority = 1, .object_id = 2},
        RenderQueueKey{.queue = RenderQueue::Opaque, .layers = RenderLayerBit(RenderLayer::Default), .queue_priority = 0, .object_id = 3},
    };

    auto sorted = keys;
    std::ranges::sort(sorted, [](const RenderQueueKey& lhs, const RenderQueueKey& rhs) {
        return lhs < rhs;
    });

    EXPECT_EQ(sorted[0].queue, RenderQueue::Opaque);
    EXPECT_EQ(sorted[0].queue_priority, 0);
    EXPECT_EQ(sorted[0].object_id, 3);
    EXPECT_EQ(sorted[1].layers, RenderLayerBit(RenderLayer::Outline));
    EXPECT_EQ(sorted[2].queue, RenderQueue::Transparent);
}

TEST(RendererPassBuilderTest, CreatesDepthShadowGBufferAndIdResources) {
    auto            mock_device = gfx::create_device(gfx::Device::Type::Mock, "RendererPassBuilderTest");
    hitagi::gfx::CommandQueues  queues(*mock_device);
    auto                        bindings = hitagi::gfx::BindlessUtils::Create(*mock_device);
    hitagi::gfx::ShaderCompiler compiler{"Tests"};
    rg::RenderGraph             graph(*mock_device, queues, *bindings, "RendererPassBuilderGraph");
    RenderContext   context{
        .device = *mock_device,
        .graph  = graph,
    };

    const auto frame_constant    = graph.Create(gfx::GPUBufferDesc{
        .name   = "frame_constant",
        .size   = (sizeof(FrameConstant)) * (1),
        .usages = gfx::GPUBufferUsageFlags::StorageRead,
    });
    const auto instance_constant = graph.Create(gfx::GPUBufferDesc{
        .name   = "instance_constant",
        .size   = (sizeof(InstanceConstant)) * (1),
        .usages = gfx::GPUBufferUsageFlags::StorageRead,
    });
    const auto pipeline          = hitagi::gfx::RenderPipeline::Create(*mock_device, *bindings, {}, {});

    const auto depth = passes::DepthPrepass::CreateTarget(context, {.width = 64, .height = 32});
    passes::DepthPrepass::Build(
        context,
        passes::DepthPrepass::BuildDesc{
            .pass_name         = "UnitDepthPrepass",
            .depth             = depth,
            .frame_constant    = frame_constant,
            .instance_constant = instance_constant,
            .pipeline          = pipeline,
            .width             = 64,
            .height            = 32,
        });

    const auto shadow = passes::ShadowMapPass::CreateTarget(context, {.width = 128, .height = 128});
    passes::ShadowMapPass::Build(
        context,
        passes::ShadowMapPass::BuildDesc{
            .pass_name            = "UnitShadowMapPass",
            .shadow_map           = shadow,
            .light_frame_constant = frame_constant,
            .instance_constant    = instance_constant,
            .pipeline             = pipeline,
            .width                = 128,
            .height               = 128,
        });

    const auto      id_buffer = passes::ObjectMaterialIdPass::CreateTarget(context, {.width = 64, .height = 32});
    passes::GBuffer gbuffer_pass(*mock_device, *bindings, compiler, {.path = "unused.hlsl"});
    const auto      gbuffer = gbuffer_pass.CreateTargets(context, {.width = 64, .height = 32});

    EXPECT_TRUE(graph.IsValid(depth));
    EXPECT_EQ(graph.GetResourceDesc(depth).format, gfx::Format::D32_FLOAT);
    EXPECT_TRUE(graph.IsValid(shadow));
    EXPECT_EQ(graph.GetResourceDesc(shadow).width, 128);
    EXPECT_TRUE(graph.IsValid(id_buffer));
    EXPECT_EQ(graph.GetResourceDesc(id_buffer).format, gfx::Format::R32G32_UINT);
    EXPECT_TRUE(graph.IsValid(gbuffer.normal));
    EXPECT_TRUE(graph.IsValid(gbuffer.material));
    EXPECT_TRUE(graph.IsValid(gbuffer.object_material_id));
    EXPECT_EQ(graph.GetResourceDesc(gbuffer.object_material_id).format, gfx::Format::R32G32_UINT);

    const auto dot = graph.ToDot();
    EXPECT_NE(dot.find("UnitDepthPrepass"), std::pmr::string::npos);
    EXPECT_NE(dot.find("UnitShadowMapPass"), std::pmr::string::npos);
}

TEST(RendererPassBuilderTest, RenderGraphDebugViewNamesMapToGBufferViews) {
    EXPECT_EQ(RenderGraphDebugViewName(RenderGraphDebugView::Final), "final");
    EXPECT_EQ(RenderGraphDebugViewName(RenderGraphDebugView::BaseColor), "albedo");
    EXPECT_EQ(RenderGraphDebugViewName(RenderGraphDebugView::Normal), "normal");
    EXPECT_EQ(RenderGraphDebugViewName(RenderGraphDebugView::MaterialId), "material_id");
    EXPECT_EQ(RenderGraphDebugViewName(RenderGraphDebugView::Emissive), "emissive");
}
