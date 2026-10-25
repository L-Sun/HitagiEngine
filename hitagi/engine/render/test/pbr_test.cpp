#include "test_macros.hpp"
import interop.gtest;
import std;
import gfx;
import asset;
import math;
import utils;
import core;
import ecs;
import engine;
import platform;
import render;

using namespace hitagi;

namespace pbr_renderer_test {
class FrameProducer final {
public:
    FrameProducer(render::RenderContext& context, render::pbr::PbrRenderer& renderer, std::shared_ptr<gfx::Texture> target)
        : m_Context(context), m_Renderer(renderer), m_Target(std::move(target)) {}
    void ConfigureSchedule(ecs::Schedule& schedule) {
        schedule.Request("Produce", [this] { m_Context.Submit(m_Renderer.Capture({}), m_Target); }, FrameStage::Update);
    }
    ~FrameProducer() { EXPECT_GE(m_Context.GetRenderedFrameCount(), 1u); }

private:
    render::RenderContext&        m_Context;
    render::pbr::PbrRenderer&     m_Renderer;
    std::shared_ptr<gfx::Texture> m_Target;
};
}  // namespace pbr_renderer_test

class PbrRendererTest : public testing::TestWithParam<gfx::Device::Type> {};

#ifdef _WIN32
INSTANTIATE_TEST_SUITE_P(Backends, PbrRendererTest, testing::Values(gfx::Device::Type::DX12, gfx::Device::Type::Vulkan));
#else
INSTANTIATE_TEST_SUITE_P(Backends, PbrRendererTest, testing::Values(gfx::Device::Type::Vulkan));
#endif

TEST_P(PbrRendererTest, EmptyFrameClearsApplicationTargetAcrossExecutions) {
    auto                     device = gfx::create_device(GetParam());
    gfx::CommandQueues       queues(*device);
    auto                     bindings = gfx::BindlessUtils::Create(*device);
    gfx::ShaderCompiler      compiler("PBR pipeline test");
    render::pbr::PbrRenderer pipeline({*device, queues, *bindings, compiler});
    rg::RenderGraph          graph(*device, queues, *bindings);
    gfx::TextureDesc         desc{
        .name        = "ApplicationOutput",
        .width       = 32,
        .height      = 32,
        .format      = gfx::Format::R8G8B8A8_UNORM,
        .clear_value = math::Color::Red(),
        .usages      = gfx::TextureUsageFlags::RenderTarget | gfx::TextureUsageFlags::CopySrc | gfx::TextureUsageFlags::CopyDst,
    };
    EXPECT_FALSE(pipeline.Build(graph, {}).color);
    // Seed different pixels so the test verifies Build's clear pass, rather
    // than the texture factory's initialization with the same clear value.
    const std::array<std::byte, 32 * 32 * 4> initial_pixels{};
    for (unsigned frame = 0; frame < 2; ++frame) {
        desc.clear_value  = frame ? math::Color::Blue() : math::Color::Red();
        const auto output = gfx::Texture::Create(*device, queues, *bindings, desc, initial_pixels);
        const auto target = graph.Import(output);
        const auto result = pipeline.Build(graph, {.target = target});
        EXPECT_EQ(result.color, target);
        ASSERT_TRUE(graph.IsValid(result.depth));
        // Compose an application overlay, retaining this offscreen graph on the
        // graphics queue. Direct readback below verifies the pipeline's pixels.
        rg::RenderPassBuilder overlay(graph);
        overlay.SetRenderTarget(graph.MoveFrom(result.color), false);
        overlay.AllowPassCulling(false);
        overlay.SetExecutor([](const rg::RenderGraph&, const rg::RenderPassNode&) {});
        overlay.Finish();
        ASSERT_TRUE(graph.Compile());
        graph.Execute();
        graph.ClearImportedResources();
        const auto pixels = gfx::readback_texture(*device, queues, *bindings, *output);
        ASSERT_TRUE(pixels.GetDataSize() >= 4u);
        EXPECT_EQ(std::to_integer<unsigned>(pixels.GetData()[0]), frame ? 0u : 255u);
        EXPECT_EQ(std::to_integer<unsigned>(pixels.GetData()[2]), frame ? 255u : 0u);
        EXPECT_EQ(std::to_integer<unsigned>(pixels.GetData()[3]), 255u);
    }
}

TEST_P(PbrRendererTest, SystemConsumesSnapshotsAndRunsAfterProducer) {
    core::Runtime       runtime;
    auto                device = gfx::create_device(GetParam());
    gfx::CommandQueues  queues(*device);
    auto                bindings = gfx::BindlessUtils::Create(*device);
    gfx::ShaderCompiler compiler("PBR module test");
    App                 root(runtime);
    root.AddPlugin(render::RenderPlugin{.device = *device, .queues = queues, .bindings = *bindings});
    root.AddPlugin(render::pbr::PbrPlugin{.resources = {.device = *device, .queues = queues, .bindings = *bindings, .shader_compiler = compiler}});
    auto& render_world = root.GetWorld().GetResource<render::RenderApp>().app.GetWorld();
    auto& context      = render_world.GetResource<render::RenderContext>();

    auto&                                    selected = render_world.GetResource<render::pbr::PbrRenderer>();
    const std::array<std::byte, 16 * 16 * 4> initial_pixels{};
    auto                                     target = gfx::Texture::Create(*device, queues, *bindings, {
                                                                                                           .name        = "SubmittedTarget",
                                                                                                           .width       = 16,
                                                                                                           .height      = 16,
                                                                                                           .format      = gfx::Format::R8G8B8A8_UNORM,
                                                                                                           .clear_value = math::Color::Red(),
                                                                                                           .usages      = gfx::TextureUsageFlags::RenderTarget | gfx::TextureUsageFlags::CopySrc | gfx::TextureUsageFlags::CopyDst,
                                                                                                       },
                                                                           initial_pixels);
    EXPECT_THROW(context.Submit(selected.Capture({}), {}), std::invalid_argument);
    EXPECT_THROW(context.SetFrame(selected.Capture({})), std::logic_error);
    root.TickFrame();
    EXPECT_EQ(context.GetRenderedFrameCount(), 0u);

    std::vector<render::pbr::RenderDrawItem> items{{.mesh = std::make_shared<asset::Mesh>()}};
    std::weak_ptr<asset::Mesh>               mesh = items.front().mesh;
    context.Submit(selected.Capture({.draw_items = items}), target);
    context.Submit(selected.Capture({}), target);
    items.clear();
    items.shrink_to_fit();
    EXPECT_FALSE(mesh.expired());
    root.TickFrame();
    EXPECT_TRUE(mesh.expired());
    EXPECT_EQ(context.GetRenderedFrameCount(), 2u);
    root.TickFrame();
    EXPECT_EQ(context.GetRenderedFrameCount(), 2u);
    const auto pixels = gfx::readback_texture(*device, queues, *bindings, *target);
    ASSERT_TRUE(pixels.GetDataSize() >= 4u);
    EXPECT_EQ(std::to_integer<unsigned>(pixels.GetData()[0]), 255u);
    EXPECT_EQ(std::to_integer<unsigned>(pixels.GetData()[2]), 0u);

    root.GetWorld().RegisterSystem<pbr_renderer_test::FrameProducer>(context, selected, target);
    root.TickFrame();
    EXPECT_EQ(context.GetRenderedFrameCount(), 3u);
    root.GetWorld().GetSystemManager().Unregister<pbr_renderer_test::FrameProducer>();
    root.TickFrame();
}
