#include "test_macros.hpp"
#include "interop/tracy_macros.hpp"
import interop.gtest;
import interop.spdlog;
import interop.tracy;
import interop.magic_enum;

import std;
import gfx;
import render;
import asset;
import core;
import ecs;
import engine;
import utils;
import math;
import app;
import test_utils;

using namespace testing;
using namespace hitagi;
using namespace hitagi::render::deferred;

class DeferredRendererTest : public TestWithParam<gfx::Device::Type> {};

#ifdef _WIN32
INSTANTIATE_TEST_SUITE_P(Backends, DeferredRendererTest, Values(gfx::Device::Type::DX12, gfx::Device::Type::Vulkan));
#elif defined(__APPLE__)
INSTANTIATE_TEST_SUITE_P(Backends, DeferredRendererTest, Values(gfx::Device::Type::Metal, gfx::Device::Type::Vulkan));
#else
INSTANTIATE_TEST_SUITE_P(Backends, DeferredRendererTest, Values(gfx::Device::Type::Vulkan));
#endif

class DeferredExtensionProbe final : public IDeferredRenderExtension {
public:
    void AfterGBuffer(rg::RenderGraph& graph, const RenderView&, const DeferredRenderResources& resources, const DeferredDrawData&) final {
        EXPECT_TRUE(graph.IsValid(resources.depth));
        ++gbuffer_calls;
    }
    void AfterLighting(rg::RenderGraph& graph, const RenderView&, DeferredRenderResources& resources, const DeferredDrawData&) final {
        EXPECT_TRUE(graph.IsValid(resources.color));
        ++lighting_calls;
    }
    unsigned gbuffer_calls  = 0;
    unsigned lighting_calls = 0;
};

class DeferredSnapshotProbe final : public IDeferredRenderExtension {
public:
    void AfterGBuffer(rg::RenderGraph&, const RenderView& view, const DeferredRenderResources&, const DeferredDrawData&) final {
        if (fail) throw std::runtime_error("Injected extension failure");
        camera_positions.emplace_back(view.camera_position.x);
    }
    bool               fail = false;
    std::vector<float> camera_positions;
};

TEST_P(DeferredRendererTest, SystemConsumesSnapshotsAndRecoversAfterExtensionFailure) {
    auto                device = gfx::create_device(GetParam());
    gfx::CommandQueues  queues(*device);
    auto                bindings = gfx::BindlessUtils::Create(*device);
    gfx::ShaderCompiler compiler("Deferred module test");
    core::FileIOManager file_io;
    core::JobSystem frame_jobs{core::JobSystem::Execution::CallingThread};
    ecs::World root("Root", ConfigureFrameStages);

    auto& selected = root.RegisterSystem<DeferredRenderer>(*device, queues, *bindings, compiler, file_io, "assets/shaders", FrameStage::Render);
    const auto probe = std::make_shared<DeferredSnapshotProbe>();
    selected.AddExtension(probe);
    auto target = gfx::Texture::Create(*device, queues, *bindings, {
                                                                       .name        = "DeferredSubmittedTarget",
                                                                       .width       = 16,
                                                                       .height      = 16,
                                                                       .format      = gfx::Format::R8G8B8A8_UNORM,
                                                                       .clear_value = math::Color::Black(),
                                                                       .usages      = gfx::TextureUsageFlags::RenderTarget,
                                                                   });
    EXPECT_THROW(selected.Submit({}, {}), std::invalid_argument);
    EXPECT_THROW(selected.SetFrame({}), std::logic_error);
    std::vector<RenderDrawItem> items{{.mesh = std::make_shared<asset::Mesh>()}};
    std::weak_ptr<asset::Mesh>  mesh = items.front().mesh;
    RenderFrame                 frame{.view = {.camera_position = {1.0f, 0.0f, 0.0f}}, .draw_items = items};
    selected.Submit(frame, target);
    frame.view.camera_position.x = 2.0f;
    selected.Submit(frame, target, RenderGraphDebugView::Normal);
    items.clear();
    items.shrink_to_fit();
    EXPECT_FALSE(mesh.expired());
    root.Update(frame_jobs);
    EXPECT_TRUE(mesh.expired());
    EXPECT_EQ(probe->camera_positions, (std::vector<float>{1.0f, 2.0f}));
    EXPECT_EQ(selected.GetRenderedFrameCount(), 2u);
    root.Update(frame_jobs);
    EXPECT_EQ(selected.GetRenderedFrameCount(), 2u);

    probe->fail = true;
    selected.Submit({}, target);
    EXPECT_THROW(root.Update(frame_jobs), std::runtime_error);
    EXPECT_EQ(selected.GetRenderedFrameCount(), 2u);
    selected.ClearExtensions();
    selected.Submit({}, target);
    root.Update(frame_jobs);
    EXPECT_EQ(selected.GetRenderedFrameCount(), 3u);
}

TEST_P(DeferredRendererTest, BuildsExplicitFramesAndExtensionsIntoApplicationGraph) {
    auto                   device = gfx::create_device(GetParam());
    gfx::CommandQueues     queues(*device);
    auto                   bindings = gfx::BindlessUtils::Create(*device);
    gfx::ShaderCompiler    compiler("Deferred pipeline test");
    core::FileIOManager    file_io;
    DeferredRenderer       pipeline(*device, queues, *bindings, compiler, file_io, "assets/shaders");
    rg::RenderGraph        graph(*device, queues, *bindings);
    const gfx::TextureDesc desc{
        .name        = "ApplicationOutput",
        .width       = 32,
        .height      = 32,
        .format      = gfx::Format::R8G8B8A8_UNORM,
        .clear_value = math::Color::Black(),
        .usages      = gfx::TextureUsageFlags::RenderTarget | gfx::TextureUsageFlags::CopySrc | gfx::TextureUsageFlags::CopyDst,
    };
    const auto extension = std::make_shared<DeferredExtensionProbe>();
    pipeline.AddExtension(extension);
    EXPECT_FALSE(pipeline.Build(graph, {}).color);
    const std::array views{
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
    for (const auto view : views) {
        const auto target = graph.Create(desc);
        const auto result = pipeline.Build(graph, {.target = target, .debug_view = view});
        EXPECT_EQ(result.color, target);
        EXPECT_TRUE(graph.IsValid(result.depth));
        EXPECT_TRUE(graph.IsValid(result.normal));
        EXPECT_TRUE(graph.IsValid(result.linear_depth));
        const auto dot = graph.ToDot();
        if (view == RenderGraphDebugView::Final) {
            EXPECT_NE(dot.find("DeferredLightingPass-"), std::pmr::string::npos);
            EXPECT_EQ(dot.find("DeferredDebugViewPass-"), std::pmr::string::npos);
        } else {
            EXPECT_NE(dot.find("DeferredDebugViewPass-"), std::pmr::string::npos);
            EXPECT_EQ(dot.find("DeferredLightingPass-"), std::pmr::string::npos);
        }
        rg::RenderPassBuilder overlay(graph);
        overlay.SetRenderTarget(graph.MoveFrom(result.color), false);
        overlay.AllowPassCulling(false);
        overlay.SetExecutor([](const rg::RenderGraph&, const rg::RenderPassNode&) {});
        overlay.Finish();
        ASSERT_TRUE(graph.Compile());
        graph.Execute();
        graph.ClearImportedResources();
    }
    EXPECT_EQ(extension->gbuffer_calls, views.size());
    EXPECT_EQ(extension->lighting_calls, 2u);
    pipeline.ClearExtensions();
    pipeline.Build(graph, {.target = graph.Create(desc)});
    EXPECT_EQ(extension->gbuffer_calls, views.size());
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
    auto                        mock_device = gfx::create_device(gfx::Device::Type::Mock, "RendererPassBuilderTest");
    hitagi::gfx::CommandQueues  queues(*mock_device);
    auto                        bindings = hitagi::gfx::BindlessUtils::Create(*mock_device);
    hitagi::gfx::ShaderCompiler compiler{"Tests"};
    rg::RenderGraph             graph(*mock_device, queues, *bindings, "RendererPassBuilderGraph");

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

    const auto depth = passes::DepthPrepass::CreateTarget(graph, {.width = 64, .height = 32});
    passes::DepthPrepass::Build(
        graph,
        passes::DepthPrepass::BuildDesc{
            .pass_name         = "UnitDepthPrepass",
            .depth             = depth,
            .frame_constant    = frame_constant,
            .instance_constant = instance_constant,
            .pipeline          = pipeline,
            .width             = 64,
            .height            = 32,
        });

    const auto shadow = passes::ShadowMapPass::CreateTarget(graph, {.width = 128, .height = 128});
    passes::ShadowMapPass::Build(
        graph,
        passes::ShadowMapPass::BuildDesc{
            .pass_name            = "UnitShadowMapPass",
            .shadow_map           = shadow,
            .light_frame_constant = frame_constant,
            .instance_constant    = instance_constant,
            .pipeline             = pipeline,
            .width                = 128,
            .height               = 128,
        });

    const auto      id_buffer = passes::ObjectMaterialIdPass::CreateTarget(graph, {.width = 64, .height = 32});
    passes::GBuffer gbuffer_pass(*mock_device, *bindings, compiler, {.path = "unused.slang"});
    const auto      gbuffer = gbuffer_pass.CreateTargets(graph, {.width = 64, .height = 32});

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

TEST_P(DeferredRendererTest, LightingReadsFirstAndLastLightFromTheExistingFrameBuffer) {
    auto                     device = gfx::create_device(GetParam());
    gfx::CommandQueues       queues(*device);
    auto                     bindings = gfx::BindlessUtils::Create(*device);
    gfx::ShaderCompiler      compiler("Deferred light array layout");
    core::FileIOManager      file_io;
    passes::DeferredLighting lighting(*device, *bindings, compiler, LoadShaderSource(file_io, "assets/shaders/deferred_lighting.slang"));
    const auto               make_texture = [&](math::vec4f pixel) {
        return gfx::Texture::Create(*device, queues, *bindings,
                                    {.width = 1, .height = 1, .format = gfx::Format::R32G32B32A32_FLOAT, .usages = gfx::TextureUsageFlags::SRV | gfx::TextureUsageFlags::CopyDst},
                                    std::as_bytes(std::span(&pixel, 1)));
    };
    const auto                  albedo   = make_texture({0.5f, 0.5f, 0.5f, 1.0f});
    const auto                  normal   = make_texture({0.5f, 0.5f, 1.0f, 1.0f});
    const auto                  material = make_texture({0.0f, 0.5f, 0.0f, 0.0f});
    const auto                  emissive = make_texture({0.0f, 0.0f, 0.0f, 0.0f});
    const auto                  sampler  = gfx::Sampler::Create(*device, *bindings, {});
    const auto                  output   = gfx::Texture::Create(*device, queues, *bindings,
                                                                {.width = 4, .height = 4, .format = gfx::Format::R8G8B8A8_UNORM, .usages = gfx::TextureUsageFlags::RenderTarget | gfx::TextureUsageFlags::CopySrc});
    const auto                  pipeline = lighting.GetPipeline(output->GetDesc().format);
    std::array<core::Buffer, 4> images;
    const std::array            counts{0u, 1u, MaxDeferredLights, MaxDeferredLights + 1};
    for (std::size_t run = 0; run < counts.size(); ++run) {
        FrameConstant frame{};
        frame.inv_projection                                              = math::inverse(math::perspective(1.0f, 1.0f, 0.1f, 100.0f));
        frame.light_count                                                 = counts[run];
        frame.exposure                                                    = 1.0f;
        frame.viewport                                                    = {4.0f, 4.0f, 0.25f, 0.25f};
        frame.lights[run < 2 ? 0 : MaxDeferredLights - 1].color_intensity = {1.0f, 1.0f, 1.0f, 1.0f};
        auto            frame_buffer                                      = gfx::GPUBuffer::Create(*device,
                                                                                                   {.size = sizeof(frame), .usages = gfx::GPUBufferUsageFlags::StorageRead | gfx::GPUBufferUsageFlags::MapWrite},
                                                                                                   std::as_bytes(std::span(&frame, 1)));
        rg::RenderGraph graph(*device, queues, *bindings);
        const auto      target = graph.Import(output);
        lighting.Build(graph,
                       {.albedo = graph.Import(albedo), .normal = graph.Import(normal), .material = graph.Import(material), .emissive = graph.Import(emissive)},
                       graph.Import(frame_buffer),
                       graph.Create(gfx::GPUBufferDesc{.size   = sizeof(passes::DeferredLighting::BindlessInfo),
                                                       .usages = gfx::GPUBufferUsageFlags::StorageRead | gfx::GPUBufferUsageFlags::MapWrite}),
                       graph.Import(sampler), pipeline, target);
        rg::RenderPassBuilder retain(graph);
        retain.SetRenderTarget(graph.MoveFrom(target), false);
        retain.AllowPassCulling(false);
        retain.SetExecutor([](const rg::RenderGraph&, const rg::RenderPassNode&) {});
        retain.Finish();
        ASSERT_TRUE(graph.Compile());
        graph.Execute();
        graph.ClearImportedResources();
        images[run] = gfx::readback_texture(*device, queues, *bindings, *output);
    }
    EXPECT_FALSE(std::ranges::equal(images[0].Span<const std::byte>(), images[1].Span<const std::byte>()));
    EXPECT_TRUE(std::ranges::equal(images[1].Span<const std::byte>(), images[2].Span<const std::byte>()));
    EXPECT_TRUE(std::ranges::equal(images[2].Span<const std::byte>(), images[3].Span<const std::byte>()));
}
