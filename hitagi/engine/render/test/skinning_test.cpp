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
                                                                              .pass_contract = deferred ? "GBuffer" : toon ? "ToonForward"
                                                                                                                           : "PBRForward",
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

TEST_P(SkinningRendererTest, ComputeMatchesCpuReferenceAndConservativeBounds) {
    core::JobSystem jobs;
    auto device = gfx::create_device(GetParam());
    gfx::CommandQueues queues(*device);
    auto bindings = gfx::BindlessUtils::Create(*device);
    gfx::ShaderCompiler compiler("GPU skin reference");
    asset::ResourceLoadContext context{*device, queues, *bindings, compiler};
    render::GpuSkinning skinning;
    hitagi::rg::RenderGraph graph(*device, queues, *bindings);
    constexpr std::size_t count = 65; // Exercises the partial final workgroup.
    constexpr std::array attributes{asset::VertexAttribute::Position, asset::VertexAttribute::Normal,
        asset::VertexAttribute::Tangent, asset::VertexAttribute::Bitangent};
    asset::VertexArray::Data data;
    for (auto attribute : attributes) {
        data[attribute] = core::Buffer(count * sizeof(math::vec3f));
        auto span = data[attribute].Span<math::vec3f>();
        for (std::size_t i = 0; i < count; ++i) span[i] = {float(i) / 100, .3f, .7f};
    }
    data[asset::VertexAttribute::BlendIndex] = core::Buffer(count * sizeof(math::vec4u));
    data[asset::VertexAttribute::BlendWeight] = core::Buffer(count * sizeof(math::vec4f));
    for (std::size_t i = 0; i < count; ++i) {
        data[asset::VertexAttribute::BlendIndex].Span<math::vec4u>()[i] = {0, 1, 999, 999};
        data[asset::VertexAttribute::BlendWeight].Span<math::vec4f>()[i] = i % 3 == 0 ? math::vec4f(0) : math::vec4f(2, 1, 0, 0);
    }
    auto vertices = std::make_shared<asset::VertexArray>(count, std::move(data));
    auto mesh = std::make_shared<asset::Mesh>(vertices);
    asset::Scene scene;
    auto& entities = scene.GetWorld().GetEntityManager();
    auto a = entities.Create(asset::Transform{}, asset::RelationShip{});
    auto b = entities.Create(asset::Transform{}, asset::RelationShip{});
    auto entity = entities.Create(asset::Transform{}, asset::RelationShip{}, asset::MeshComponent{mesh});
    entity.Get<asset::Transform>().position = {.1f, -.2f, .3f};
    entity.Get<asset::Transform>().scaling = {.8f, 1.2f, 1.1f};
    entity.Emplace<asset::SkinnedMesh>(std::make_shared<asset::Skin>(std::vector{
        math::translate(math::vec3f(.2f, .1f, -.3f)),
        math::translate(math::vec3f(-.1f, .4f, .2f))}), std::vector{a, b});
    for (bool gpu_palette : {false, true}) for (float scale : {2.0f, -2.0f, 0.0f}) {
        a.Get<asset::Transform>().scaling = {scale, 3, .5f};
        a.Get<asset::Transform>().position = {.4f, -.2f, .1f};
        b.Get<asset::Transform>().scaling = {scale, 3, .5f};
        auto& skin = entity.Get<asset::SkinnedMesh>();
        skin.gpu_palette = gpu_palette;
        skin.cpu_deformation = true;
        scene.GetWorld().Update(jobs);
        const auto reference = skin.deformed_vertices;
        skin.cpu_deformation = false;
        scene.GetWorld().Update(jobs);
        const auto snapshot = skin.pose;
        EXPECT_EQ(snapshot->matrices.empty(), gpu_palette);
        EXPECT_FALSE(skin.deformed_vertices);
        ASSERT_TRUE(snapshot);
        for (auto point : reference->Span<asset::VertexAttribute::Position>()) {
            for (unsigned c = 0; c < 3; ++c) {
                EXPECT_GE(point[c] + 1e-5f, snapshot->bounds.min_point[c]);
                EXPECT_LE(point[c] - 1e-5f, snapshot->bounds.max_point[c]);
            }
        }
        skinning.BeginFrame();
        const auto streams = skinning.Build(graph, context, vertices, snapshot);
        std::array<std::shared_ptr<gfx::GPUBuffer>, 4> readbacks;
        for (std::size_t i = 0; i < attributes.size(); ++i) {
            readbacks[i] = gfx::GPUBuffer::Create(*device, {.size = count * sizeof(math::vec3f),
                .usages = gfx::GPUBufferUsageFlags::CopyDst | gfx::GPUBufferUsageFlags::MapRead});
            const auto from = streams[attributes[i]], to = graph.Import(readbacks[i]);
            rg::CopyPassBuilder copy(graph);
            copy.AllowPassCulling(false);
            copy.BufferToBuffer(from, to);
            copy.SetExecutor([from, to](const rg::RenderGraph&, const rg::CopyPassNode& pass) {
                pass.GetCmd().CopyBuffer(pass.Resolve(from), 0, pass.Resolve(to), 0, count * sizeof(math::vec3f));
            });
            copy.Finish();
        }
        ASSERT_TRUE(graph.Compile());
        graph.Execute();
        for (std::size_t i = 0; i < attributes.size(); ++i) {
            const auto expected = reference->GetAttributeData(attributes[i])->get().cpu_buffer.Span<const math::vec3f>();
            const auto* actual = reinterpret_cast<const math::vec3f*>(readbacks[i]->Map());
            for (std::size_t v = 0; v < count; ++v)
                for (unsigned c = 0; c < 3; ++c) EXPECT_NEAR(actual[v][c], expected[v][c], 2e-5f) << i << ":" << v;
            readbacks[i]->UnMap();
        }
        graph.ClearImportedResources();
    }
}

TEST_P(SkinningRendererTest, AllRenderersKeepSharedMeshInstanceStreamsAndSubmittedSnapshots) {
    core::JobSystem     jobs;
    auto                device = gfx::create_device(GetParam());
    gfx::CommandQueues  queues(*device);
    auto                bindings = gfx::BindlessUtils::Create(*device);
    gfx::ShaderCompiler compiler("Skinning renderer test");
    core::IOService     file_io;
    for (bool gpu_palette : {false, true}) for (unsigned variant : {0u, 1u, 2u, 3u, 4u, 5u}) {
        const unsigned           kind       = variant % 3;
        const bool               hide_right = variant >= 3;
        const bool               deferred = kind == 1, toon = kind == 2;
        asset::Scene             scene;
        const auto               mesh = skinning_render_test::Triangle(deferred, toon);
        const auto               skin = std::make_shared<asset::Skin>(std::vector{math::mat4f::identity()});
        std::vector<ecs::Entity> joints, instances;
        for (float offset : {-0.5f, 0.5f}) {
            auto joint    = scene.GetWorld().GetEntityManager().Create(asset::Transform(math::translate(math::vec3f(offset, 0.0f, 0.0f))), asset::RelationShip{}, asset::MetaInfo("Joint"));
            auto instance = scene.GetWorld().GetEntityManager().Create(asset::Transform(math::mat4f::identity()), asset::RelationShip{}, asset::MetaInfo("Instance"), asset::MeshComponent{mesh});
            instance.Emplace<asset::SkinnedMesh>(skin, std::vector{joint});
            instance.Get<asset::SkinnedMesh>().gpu_palette = gpu_palette;
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
        for (unsigned i = 0; i < 2; ++i) {
            const auto pose = instances[i].Get<asset::SkinnedMesh>().pose;
            const auto& matrix = gpu_palette ? (*pose->joint_worlds)[0] : pose->matrices[0];
            EXPECT_NEAR(matrix[0][3], i ? .5f : -.5f, 1e-6f);
        }
        const auto advance_scene = [&] {
            instances[1].Get<asset::MeshComponent>().material_overrides.clear();
            for (auto joint : joints) joint.Get<asset::Transform>().position.x = 5.0f;
            scene.GetWorld().Update(jobs);
        };
        if (deferred) {
            render::deferred::DeferredRenderer            renderer(*device, queues, *bindings, compiler, file_io, "assets/shaders");
            render::RenderContext                         rendering(*device, queues, *bindings);
            std::vector<render::deferred::RenderDrawItem> items;
            for (auto entity : instances) items.emplace_back(render::deferred::RenderDrawItem::FromEntity(entity));
            auto capture = renderer.Capture({.draw_items = items}, render::deferred::RenderGraphDebugView::BaseColor);
            rendering.Submit(capture, target);
            items.clear();
            advance_scene();
            rendering.RenderPending();
            // Reuse pooled output buffers after graphics consumed them. DX12
            // must not carry vertex-input barriers onto the compute queue.
            for (unsigned frame = 0; frame < 2; ++frame) {
                rendering.Submit(capture, target);
                rendering.RenderPending();
            }
        } else if (toon) {
            render::toon::ToonRenderer          renderer({*device, queues, *bindings, compiler});
            render::RenderContext               rendering(*device, queues, *bindings);
            std::vector<render::RenderDrawItem> items;
            for (auto entity : instances) items.emplace_back(render::RenderDrawItem::FromEntity(entity));
            auto capture = renderer.Capture({.draw_items = items});
            rendering.Submit(capture, target);
            items.clear();
            advance_scene();
            rendering.RenderPending();
            // Reuse pooled output buffers after graphics consumed them. DX12
            // must not carry vertex-input barriers onto the compute queue.
            for (unsigned frame = 0; frame < 2; ++frame) {
                rendering.Submit(capture, target);
                rendering.RenderPending();
            }
        } else {
            render::pbr::PbrRenderer                 renderer({*device, queues, *bindings, compiler});
            render::RenderContext                    rendering(*device, queues, *bindings);
            std::vector<render::pbr::RenderDrawItem> items;
            for (auto entity : instances) items.emplace_back(render::pbr::RenderDrawItem::FromEntity(entity));
            auto capture = renderer.Capture({.draw_items = items});
            rendering.Submit(capture, target);
            items.clear();
            advance_scene();
            rendering.RenderPending();
            // Reuse pooled output buffers after graphics consumed them. DX12
            // must not carry vertex-input barriers onto the compute queue.
            for (unsigned frame = 0; frame < 2; ++frame) {
                rendering.Submit(capture, target);
                rendering.RenderPending();
            }
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
