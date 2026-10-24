#include "test_macros.hpp"
import interop.gtest;

import std;
import asset;
import core;
import ecs;
import gfx;
import math;
import render;
import utils;

using namespace hitagi;

namespace skinning_render_test {

// Keep shading deterministic: positions exercise the deformed stream and UVs
// exercise fallback to the original shared mesh. Camera/model are identities.
constexpr std::string_view shader = R"(
struct Input { float3 position : POSITION; float2 uv : TEXCOORD; };
struct Output { float4 position : SV_Position; float2 uv : TEXCOORD; };
Output VSMain(Input input) {
    Output result;
    result.position = float4(input.position, 1.0);
    result.uv = input.uv;
    return result;
}
float4 PSMain(Output input) : SV_Target { return float4(input.uv.x, 0.0, 0.0, 1.0); }
)";

auto Material(bool deferred, bool toon = false) -> std::shared_ptr<asset::Material> {
    std::pmr::vector<std::shared_ptr<asset::Shader>> shaders;
    for (const auto type : {gfx::ShaderType::Vertex, gfx::ShaderType::Pixel}) {
        shaders.emplace_back(std::make_shared<asset::Shader>(gfx::ShaderDesc{
            .type    = type,
            .entry   = type == gfx::ShaderType::Vertex ? "VSMain" : "PSMain",
            .program = {.modules = {{.source_code = std::pmr::string(shader)}}},
        }));
    }
    auto pipeline = std::make_shared<asset::RenderPipeline>(gfx::RenderPipelineDesc{
                                                                .rasterization_state  = {.cull_mode = gfx::CullMode::None},
                                                                .depth_stencil_state  = {.depth_test_enable = true, .depth_write_enable = true, .depth_compare_op = gfx::CompareOp::Less},
                                                                .render_format        = deferred ? gfx::Format::R32G32B32A32_FLOAT : gfx::Format::R8G8B8A8_UNORM,
                                                                .depth_stencil_format = gfx::Format::D32_FLOAT,
                                                            },
                                                            std::move(shaders));
    // Match the built-in deferred attribute-pass material layout.
    std::pmr::vector<asset::MaterialBinding> bindings{
        {"base_color", math::Color::Red()},
        {"metallic", 0.0f},
        {"roughness", 0.5f},
        {"occlusion", 1.0f},
        {"_padding0", 0.0f},
        {"emissive_color", math::Color::Black()},
        {"opacity", 1.0f},
        {"alpha_cutout", 0u},
        {"alpha_cutoff", 0.5f},
        {"_padding1", 0.0f},
        {"base_color_texture", std::shared_ptr<asset::Texture>{}},
        {"metallic_roughness_texture", std::shared_ptr<asset::Texture>{}},
        {"normal_texture", std::shared_ptr<asset::Texture>{}},
        {"occlusion_texture", std::shared_ptr<asset::Texture>{}},
        {"emissive_texture", std::shared_ptr<asset::Texture>{}},
    };
    return std::make_shared<asset::Material>(asset::MaterialParameters{}, std::pmr::vector<asset::MaterialPass>{{
                                                                              .pass_contract = deferred ? "GBuffer" : toon ? "ToonForward" : "PBRForward",
                                                                              .pipeline      = std::move(pipeline),
                                                                              .bindings      = std::move(bindings),
                                                                          }});
}

auto Triangle(bool deferred, bool toon = false) -> std::shared_ptr<asset::Mesh> {
    asset::VertexArray::Data vertex_data;
    core::Buffer             index_data(3 * sizeof(std::uint16_t));
    {
        vertex_data[asset::VertexAttribute::Position] = core::Buffer(3 * sizeof(asset::VertexDataType<asset::VertexAttribute::Position>));
        auto p                                        = vertex_data[asset::VertexAttribute::Position].Span<asset::VertexDataType<asset::VertexAttribute::Position>>();

        p[0] = {-0.2f, -0.3f, 0.5f};
        p[1] = {0.2f, -0.3f, 0.5f};
        p[2] = {0.0f, 0.3f, 0.5f};
    }
    {
        vertex_data[asset::VertexAttribute::Normal] = core::Buffer(3 * sizeof(asset::VertexDataType<asset::VertexAttribute::Normal>));
        auto p                                      = vertex_data[asset::VertexAttribute::Normal].Span<asset::VertexDataType<asset::VertexAttribute::Normal>>();
        std::ranges::fill(p, math::vec3f(0.0f, 0.0f, 1.0f));
    }
    {
        vertex_data[asset::VertexAttribute::UV0] = core::Buffer(3 * sizeof(asset::VertexDataType<asset::VertexAttribute::UV0>));
        auto p                                   = vertex_data[asset::VertexAttribute::UV0].Span<asset::VertexDataType<asset::VertexAttribute::UV0>>();
        std::ranges::fill(p, math::vec2f(1.0f));
    }
    {
        vertex_data[asset::VertexAttribute::BlendIndex] = core::Buffer(3 * sizeof(asset::VertexDataType<asset::VertexAttribute::BlendIndex>));
        auto p                                          = vertex_data[asset::VertexAttribute::BlendIndex].Span<asset::VertexDataType<asset::VertexAttribute::BlendIndex>>();
        std::ranges::fill(p, math::vec4u(0));
    }
    {
        vertex_data[asset::VertexAttribute::BlendWeight] = core::Buffer(3 * sizeof(asset::VertexDataType<asset::VertexAttribute::BlendWeight>));
        auto p                                           = vertex_data[asset::VertexAttribute::BlendWeight].Span<asset::VertexDataType<asset::VertexAttribute::BlendWeight>>();
        std::ranges::fill(p, math::vec4f(1.0f, 0.0f, 0.0f, 0.0f));
    }
    {
        auto p = index_data.Span<asset::IndexDataType<asset::IndexType::UINT16>>();
        p[0]   = 0;
        p[1]   = 1;
        p[2]   = 2;
    }
    return std::make_shared<asset::Mesh>(std::make_shared<asset::VertexArray>(3, std::move(vertex_data)), std::make_shared<asset::IndexArray>(asset::IndexType::UINT16, std::move(index_data)), std::pmr::vector<asset::Mesh::SubMesh>{{.index_count = 3, .material = Material(deferred, toon)}});
}

}  // namespace skinning_render_test

class SkinningRendererTest : public testing::TestWithParam<gfx::Device::Type> {};
#ifdef _WIN32
INSTANTIATE_TEST_SUITE_P(Backends, SkinningRendererTest, testing::Values(gfx::Device::Type::DX12, gfx::Device::Type::Vulkan));
#else
INSTANTIATE_TEST_SUITE_P(Backends, SkinningRendererTest, testing::Values(gfx::Device::Type::Vulkan));
#endif

TEST_P(SkinningRendererTest, AllRenderersKeepSharedMeshInstanceStreamsAndSubmittedSnapshots) {
    core::JobSystem     jobs;
    auto                device = gfx::create_device(GetParam());
    gfx::CommandQueues  queues(*device);
    auto                bindings = gfx::BindlessUtils::Create(*device);
    gfx::ShaderCompiler compiler("Skinning renderer test");
    core::FileIOManager file_io;
    for (unsigned variant : {0u, 1u, 2u, 3u, 4u, 5u}) {
        const unsigned           kind       = variant % 3;
        const bool               hide_right = variant >= 3;
        const bool deferred = kind == 1, toon = kind == 2;
        asset::Scene             scene;
        const auto               mesh = skinning_render_test::Triangle(deferred, toon);
        const auto               skin = std::make_shared<asset::Skin>(std::vector{math::mat4f::identity()});
        std::vector<ecs::Entity> joints, instances;
        for (float offset : {-0.5f, 0.5f}) {
            auto joint    = scene.GetWorld().GetEntityManager().Create(asset::Transform(math::translate(math::vec3f(offset, 0.0f, 0.0f))), asset::RelationShip{}, asset::MetaInfo("Joint"));
            auto instance = scene.GetWorld().GetEntityManager().Create(asset::Transform(math::mat4f::identity()), asset::RelationShip{}, asset::MetaInfo("Instance"), asset::MeshComponent{mesh});
            instance.Emplace<asset::SkinnedMesh>(skin, std::vector{joint});
            joints.emplace_back(joint);
            instances.emplace_back(instance);
        }
        instances[1].Get<asset::MeshComponent>().material_overrides = {
            hide_right ? std::make_shared<asset::Material>() : mesh->sub_meshes[0].material->Clone("right")};
        scene.GetWorld().Update(jobs);
        auto       target        = gfx::Texture::Create(*device, queues, *bindings, {
                                                                                        .name        = "SkinningOutput",
                                                                                        .width       = 64,
                                                                                        .height      = 64,
                                                                                        .format      = gfx::Format::R8G8B8A8_UNORM,
                                                                                        .clear_value = math::Color::Black(),
                                                                                        .usages      = gfx::TextureUsageFlags::RenderTarget | gfx::TextureUsageFlags::CopySrc,
                                                                                    });
        const auto advance_scene = [&] {
            instances[1].Get<asset::MeshComponent>().material_overrides.clear();
            for (auto joint : joints) joint.Get<asset::Transform>().position.x = 5.0f;
            scene.GetWorld().Update(jobs);
        };
        if (deferred) {
            render::deferred::DeferredRenderer            renderer(*device, queues, *bindings, compiler, file_io, "assets/shaders");
            std::vector<render::deferred::RenderDrawItem> items;
            for (auto entity : instances) items.emplace_back(render::deferred::RenderDrawItem::FromEntity(entity));
            renderer.Submit({.draw_items = items}, target, render::deferred::RenderGraphDebugView::BaseColor);
            items.clear();
            advance_scene();
            renderer.Tick();
        } else if (toon) {
            render::toon::ToonRenderer renderer({*device, queues, *bindings, compiler});
            std::vector<render::RenderDrawItem> items;
            for (auto entity : instances) items.emplace_back(render::RenderDrawItem::FromEntity(entity));
            renderer.Submit({.draw_items = items}, target);
            items.clear();
            advance_scene();
            renderer.Tick();
        } else {
            render::pbr::PbrRenderer                 renderer({*device, queues, *bindings, compiler});
            std::vector<render::pbr::RenderDrawItem> items;
            for (auto entity : instances) items.emplace_back(render::pbr::RenderDrawItem::FromEntity(entity));
            renderer.Submit({.draw_items = items}, target);
            items.clear();
            advance_scene();
            renderer.Tick();
        }
        const auto pixels = gfx::readback_texture(*device, queues, *bindings, *target);
        ASSERT_EQ(pixels.GetDataSize(), 64u * 64u * 4u);
        const auto red = [&](unsigned x) { return std::to_integer<unsigned>(pixels.GetData()[(32 * 64 + x) * 4]); };
        EXPECT_GT(red(16), 230u) << "left instance, renderer=" << kind;
        if (hide_right)
            EXPECT_LT(red(48), 10u) << "override without pass must hide right instance, renderer=" << kind;
        else
            EXPECT_GT(red(48), 230u) << "right instance, renderer=" << kind;
        EXPECT_TRUE(mesh->sub_meshes[0].material->GetPasses()[0].pipeline->GetDesc().vertex_input_layout.empty());
        EXPECT_LT(red(32), 10u) << "bind pose must not be drawn, renderer=" << kind;
    }
}
