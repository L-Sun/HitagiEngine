#include "test_macros.hpp"
import interop.gtest;
import std;
import gfx;
import asset;
import math;
import utils;
import core;
import render;

using namespace hitagi;

namespace pbr_renderer_test {
class FrameProducer final : public core::RuntimeModule {
public:
    FrameProducer(render::pbr::PbrRenderer& renderer, std::shared_ptr<gfx::Texture> target)
        : RuntimeModule("FrameProducer"), m_Renderer(renderer), m_Target(std::move(target)) {}
    void Tick() final { m_Renderer.Submit({}, m_Target); }
    ~FrameProducer() final { EXPECT_GE(m_Renderer.GetRenderedFrameCount(), 1u); }

private:
    render::pbr::PbrRenderer&     m_Renderer;
    std::shared_ptr<gfx::Texture> m_Target;
};
}  // namespace pbr_renderer_test

class PbrRendererTest : public testing::TestWithParam<gfx::Device::Type> {};

INSTANTIATE_TEST_SUITE_P(Backends, PbrRendererTest, testing::Values(
#ifdef _WIN32
                                                        gfx::Device::Type::DX12,
#endif
                                                        gfx::Device::Type::Vulkan));

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

TEST_P(PbrRendererTest, ModuleConsumesSnapshotsOnceAndOwnsChildrenUntilDestruction) {
    auto                device = gfx::create_device(GetParam());
    gfx::CommandQueues  queues(*device);
    auto                bindings = gfx::BindlessUtils::Create(*device);
    gfx::ShaderCompiler compiler("PBR module test");
    core::RuntimeModule root("Root");
    auto                renderer = std::make_unique<render::pbr::PbrRenderer>(asset::ResourceLoadContext{*device, queues, *bindings, compiler});
    auto&               selected = *renderer;
    root.AddSubModule(std::move(renderer));
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
    EXPECT_THROW(selected.Submit({}, {}), std::invalid_argument);
    EXPECT_THROW(selected.SetFrame({}), std::logic_error);
    root.Tick();
    EXPECT_EQ(selected.GetRenderedFrameCount(), 0u);

    std::vector<render::pbr::RenderDrawItem> items{{.mesh = std::make_shared<asset::Mesh>()}};
    std::weak_ptr<asset::Mesh>               mesh = items.front().mesh;
    selected.Submit({.draw_items = items}, target);
    selected.Submit({}, target);
    items.clear();
    items.shrink_to_fit();
    EXPECT_FALSE(mesh.expired());
    root.Tick();
    EXPECT_TRUE(mesh.expired());
    EXPECT_EQ(selected.GetRenderedFrameCount(), 2u);
    root.Tick();
    EXPECT_EQ(selected.GetRenderedFrameCount(), 2u);
    const auto pixels = gfx::readback_texture(*device, queues, *bindings, *target);
    ASSERT_TRUE(pixels.GetDataSize() >= 4u);
    EXPECT_EQ(std::to_integer<unsigned>(pixels.GetData()[0]), 255u);
    EXPECT_EQ(std::to_integer<unsigned>(pixels.GetData()[2]), 0u);

    selected.AddSubModule(std::make_unique<pbr_renderer_test::FrameProducer>(selected, target));
    root.Tick();
    EXPECT_EQ(selected.GetRenderedFrameCount(), 3u);
    root.UnloadSubModule("PbrRenderer");
}
