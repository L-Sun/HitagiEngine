#include "test_macros.hpp"
import interop.gtest;
import std;
import core;
import asset;
import gfx;
import math;
import render;
import utils;

using namespace hitagi;

namespace toon_renderer_test {
auto Bands(std::shared_ptr<asset::Material> material) -> std::shared_ptr<asset::Mesh> {
    asset::VertexArray::Data vertex_data;
    core::Buffer             index_data(18 * sizeof(std::uint16_t));
    {
        vertex_data[asset::VertexAttribute::Position] = core::Buffer(12 * sizeof(asset::VertexDataType<asset::VertexAttribute::Position>));
        auto p                                        = vertex_data[asset::VertexAttribute::Position].Span<asset::VertexDataType<asset::VertexAttribute::Position>>();

        for (unsigned i = 0; i < 3; ++i) {
            const float x = -0.65f + i * 0.65f;
            p[i * 4 + 0]  = {x - 0.22f, -0.4f, 0.5f};
            p[i * 4 + 1]  = {x + 0.22f, -0.4f, 0.5f};
            p[i * 4 + 2]  = {x + 0.22f, 0.4f, 0.5f};
            p[i * 4 + 3]  = {x - 0.22f, 0.4f, 0.5f};
        }
    }
    {
        vertex_data[asset::VertexAttribute::Normal] = core::Buffer(12 * sizeof(asset::VertexDataType<asset::VertexAttribute::Normal>));
        auto n                                      = vertex_data[asset::VertexAttribute::Normal].Span<asset::VertexDataType<asset::VertexAttribute::Normal>>();

        const std::array z{-0.5f, 0.35f, 0.9f};
        for (unsigned i = 0; i < 3; ++i)
            for (unsigned j = 0; j < 4; ++j) n[i * 4 + j] = {std::sqrt(1 - z[i] * z[i]), 0, z[i]};
    }
    {
        vertex_data[asset::VertexAttribute::UV0] = core::Buffer(12 * sizeof(asset::VertexDataType<asset::VertexAttribute::UV0>));
        auto uv                                  = vertex_data[asset::VertexAttribute::UV0].Span<asset::VertexDataType<asset::VertexAttribute::UV0>>();
        std::ranges::fill(uv, math::vec2f(0.5f));
    }
    {
        auto indices = index_data.Span<asset::IndexDataType<asset::IndexType::UINT16>>();

        for (unsigned i = 0; i < 3; ++i) {
            const std::array<unsigned, 6> local{0, 1, 2, 0, 2, 3};
            for (unsigned j = 0; j < 6; ++j) indices[i * 6 + j] = static_cast<std::uint16_t>(i * 4 + local[j]);
        }
    }
    return std::make_shared<asset::Mesh>(std::make_shared<asset::VertexArray>(12, std::move(vertex_data)), std::make_shared<asset::IndexArray>(asset::IndexType::UINT16, std::move(index_data)), std::pmr::vector<asset::Mesh::SubMesh>{{.index_count = 18, .material = std::move(material)}});
}

auto Sphere(std::shared_ptr<asset::Material> material) -> std::shared_ptr<asset::Mesh> {
    constexpr unsigned       rings = 20, segments = 40;
    asset::VertexArray::Data vertex_data;
    core::Buffer             index_data((rings * segments * 6) * sizeof(std::uint16_t));
    std::vector<math::vec3f> normals;
    for (unsigned y = 0; y <= rings; ++y) {
        const float theta = std::numbers::pi_v<float> * y / rings;
        for (unsigned x = 0; x <= segments; ++x) {
            const float phi = 2 * std::numbers::pi_v<float> * x / segments;
            normals.emplace_back(std::sin(theta) * std::cos(phi), std::cos(theta), std::sin(theta) * std::sin(phi));
        }
    }
    {
        vertex_data[asset::VertexAttribute::Position] = core::Buffer(((rings + 1) * (segments + 1)) * sizeof(asset::VertexDataType<asset::VertexAttribute::Position>));
        auto p                                        = vertex_data[asset::VertexAttribute::Position].Span<asset::VertexDataType<asset::VertexAttribute::Position>>();

        for (unsigned i = 0; i < p.size(); ++i) p[i] = normals[i] * 0.38f + math::vec3f(0, 0, 0.5f);
    }
    {
        vertex_data[asset::VertexAttribute::Normal] = core::Buffer(((rings + 1) * (segments + 1)) * sizeof(asset::VertexDataType<asset::VertexAttribute::Normal>));
        auto n                                      = vertex_data[asset::VertexAttribute::Normal].Span<asset::VertexDataType<asset::VertexAttribute::Normal>>();
        std::ranges::copy(normals, n.begin());
    }
    {
        vertex_data[asset::VertexAttribute::UV0] = core::Buffer(((rings + 1) * (segments + 1)) * sizeof(asset::VertexDataType<asset::VertexAttribute::UV0>));
        auto uv                                  = vertex_data[asset::VertexAttribute::UV0].Span<asset::VertexDataType<asset::VertexAttribute::UV0>>();
        std::ranges::fill(uv, math::vec2f(0.5f));
    }
    {
        auto indices = index_data.Span<asset::IndexDataType<asset::IndexType::UINT16>>();

        unsigned i = 0;
        for (unsigned y = 0; y < rings; ++y)
            for (unsigned x = 0; x < segments; ++x) {
                const unsigned a = y * (segments + 1) + x, b = a + segments + 1;
                for (unsigned v : {a, a + 1, b, b, a + 1, b + 1}) indices[i++] = static_cast<std::uint16_t>(v);
            }
    }
    return std::make_shared<asset::Mesh>(std::make_shared<asset::VertexArray>((rings + 1) * (segments + 1), std::move(vertex_data)), std::make_shared<asset::IndexArray>(asset::IndexType::UINT16, std::move(index_data)), std::pmr::vector<asset::Mesh::SubMesh>{{.index_count = rings * segments * 6, .material = std::move(material)}});
}
}  // namespace toon_renderer_test

class ToonRendererTest : public testing::TestWithParam<gfx::Device::Type> {};
INSTANTIATE_TEST_SUITE_P(Backends, ToonRendererTest, testing::Values(
#ifdef _WIN32
                                                         gfx::Device::Type::DX12,
#endif
                                                         gfx::Device::Type::Vulkan));

TEST_P(ToonRendererTest, ThreeBandsAndSupersamplingPreserveDepthForLaterPasses) {
    auto                                           device = gfx::create_device(GetParam());
    gfx::CommandQueues                             queues(*device);
    auto                                           bindings = gfx::BindlessUtils::Create(*device);
    gfx::ShaderCompiler                            compiler("Toon tests");
    const asset::ResourceLoadContext               context{*device, queues, *bindings, compiler};
    auto                                           material = render::toon::CreateToonMaterial({.shadow_color      = math::Color(0.4f, 0.4f, 0.4f, 1),
                                                                                                .specular_strength = 0,
                                                                                                .rim_strength      = 0,
                                                                                                .outline_width     = 0});
    auto                                           mesh     = toon_renderer_test::Bands(material);
    const std::array                               items{render::RenderDrawItem{.mesh = mesh}};
    const std::string_view                         overlay_shader = R"(
float4 VSMain(uint id:SV_VertexID):SV_Position { return float4(id==1?3.0:-1.0,id==2?3.0:-1.0,0.8,1); }
float4 PSMain():SV_Target { return float4(0,1,0,1); }
)";
    std::pmr::vector<std::shared_ptr<gfx::Shader>> shaders;
    for (auto type : {gfx::ShaderType::Vertex, gfx::ShaderType::Pixel})
        shaders.emplace_back(gfx::Shader::Create(*device, compiler, {.type = type, .entry = type == gfx::ShaderType::Vertex ? "VSMain" : "PSMain", .program = {.modules = {{.source_code = std::pmr::string(overlay_shader)}}}}));
    auto pipeline = gfx::RenderPipeline::Create(*device, *bindings, {.rasterization_state = {.cull_mode = gfx::CullMode::None}, .depth_stencil_state = {.depth_test_enable = true, .depth_write_enable = true, .depth_compare_op = gfx::CompareOp::Less}, .render_format = gfx::Format::R8G8B8A8_UNORM, .depth_stencil_format = gfx::Format::D32_FLOAT}, shaders);
    for (bool aa : {false, true}) {
        render::toon::ToonRenderer renderer(context, {.supersampling = aa});
        rg::RenderGraph            graph(*device, queues, *bindings);
        auto                       output = gfx::Texture::Create(*device, queues, *bindings, {.width = 96, .height = 96, .format = gfx::Format::R8G8B8A8_UNORM, .clear_value = math::Color::Black(), .usages = gfx::TextureUsageFlags::RenderTarget | gfx::TextureUsageFlags::CopySrc});
        const auto                 result = renderer.Build(graph, {.frame = {.draw_items = items, .light = {.direction_to_light = {0, 0, 1}}}, .target = graph.Import(output)});
        EXPECT_EQ(graph.GetResourceDesc(result.depth).width, 96u);
        rg::RenderPassBuilder overlay(graph);
        overlay.SetRenderTarget(graph.MoveFrom(result.color), false);
        overlay.SetDepthStencil(graph.MoveFrom(result.depth), false);
        overlay.AllowPassCulling(false);
        overlay.SetExecutor([pipeline](const rg::RenderGraph&, const rg::RenderPassNode& pass) {
            auto& cmd = pass.GetCmd();
            cmd.SetPipeline(*pipeline);
            cmd.SetViewPort({.width = 96, .height = 96});
            cmd.SetScissorRect({.width = 96, .height = 96});
            cmd.Draw(3);
        });
        overlay.Finish();
        ASSERT_TRUE(graph.Compile());
        graph.Execute();
        graph.ClearImportedResources();
        auto pixels = gfx::readback_texture(*device, queues, *bindings, *output);
        auto red    = [&](unsigned x) { return std::to_integer<unsigned>(pixels.GetData()[(48 * 96 + x) * 4]); };
        EXPECT_NEAR(red(17), 102u, 3u) << "aa=" << aa;
        EXPECT_NEAR(red(48), 194u, 3u) << "aa=" << aa;
        EXPECT_NEAR(red(79), 255u, 3u) << "aa=" << aa;
        EXPECT_EQ(red(0), 0u);
        EXPECT_EQ(std::to_integer<unsigned>(pixels.GetData()[(48 * 96) * 4 + 1]), 255u);
    }
    asset::Texture::DestroyDefaultTexture();
}

TEST_P(ToonRendererTest, OutlineExpandsSilhouetteAndCutoutRemovesBothPasses) {
    auto                             device = gfx::create_device(GetParam());
    gfx::CommandQueues               queues(*device);
    auto                             bindings = gfx::BindlessUtils::Create(*device);
    gfx::ShaderCompiler              compiler("Toon outline tests");
    const asset::ResourceLoadContext context{*device, queues, *bindings, compiler};
    render::toon::ToonRenderer       renderer(context);
    auto                             output = gfx::Texture::Create(*device, queues, *bindings, {.width = 96, .height = 96, .format = gfx::Format::R8G8B8A8_UNORM, .clear_value = math::Color::White(), .usages = gfx::TextureUsageFlags::RenderTarget | gfx::TextureUsageFlags::CopySrc});
    std::array<unsigned, 4>          dark{};
    for (unsigned run = 0; run < 4; ++run) {
        auto material = render::toon::CreateToonMaterial({.base_color = math::Color::Red(), .shadow_color = math::Color::White(), .outline_color = math::Color::Black(), .specular_strength = 0, .rim_strength = 0, .outline_width = run ? 3.0f : 0.0f, .opacity = run == 2 ? 0.0f : 1.0f, .alpha_cutout = true});
        if (run == 3) {
            const std::array<std::uint8_t, 16> transparent{255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0};
            material->SetParameter("base_color_texture", std::make_shared<asset::Texture>(
                                                             2, 2, gfx::Format::R8G8B8A8_UNORM, core::Buffer(std::as_bytes(std::span(transparent)))));
        }
        auto        mesh = toon_renderer_test::Sphere(material);
        std::vector items{render::RenderDrawItem{.mesh = mesh}};
        renderer.Submit({.draw_items = items}, output);
        items.clear();
        mesh.reset();
        material.reset();
        renderer.Tick();
        auto pixels = gfx::readback_texture(*device, queues, *bindings, *output);
        for (unsigned i = 0; i < 96 * 96; ++i)
            if (std::to_integer<unsigned>(pixels.GetData()[i * 4]) < 80) ++dark[run];
        if (run >= 2) EXPECT_TRUE(std::ranges::all_of(pixels.Span<const std::byte>(), [](auto byte) { return byte == std::byte{255}; }));
    }
    EXPECT_EQ(dark[0], 0u);
    EXPECT_GT(dark[1], 120u);
    EXPECT_EQ(dark[2], 0u);
    EXPECT_EQ(dark[3], 0u);
    EXPECT_EQ(renderer.GetRenderedFrameCount(), 4u);
    renderer.Tick();
    EXPECT_EQ(renderer.GetRenderedFrameCount(), 4u);
    EXPECT_THROW(renderer.Submit({}, {}), std::invalid_argument);
    EXPECT_THROW(renderer.SetFrame({}), std::logic_error);
    asset::Texture::DestroyDefaultTexture();
}

TEST(ToonShaderTest, OutlineAvoidsImplicitLodAndFragmentsKeepIt) {
    gfx::ShaderCompiler compiler("Toon stage sampling");
    unsigned            checked = 0;
    for (const auto& pass : render::toon::CreateToonMaterialPasses()) {
        for (const auto& shader : pass.pipeline->GetShaders()) {
            const auto& desc = shader->GetDesc();
            if (desc.entry != "VSOutline" && desc.type != gfx::ShaderType::Pixel) continue;
            auto       program  = compiler.LoadProgram(desc.program, gfx::ShaderTarget::SPIRV);
            auto       compiled = program->Compile(desc.entry, desc.type, desc.specialization_types);
            const auto words    = compiled.bytecode.Span<const std::uint32_t>();
            ASSERT_TRUE(words.size() >= 5u);
            unsigned implicit_lod = 0;
            for (std::size_t offset = 5; offset < words.size();) {
                const auto count = words[offset] >> 16;
                ASSERT_TRUE(count > 0u);
                ASSERT_TRUE(offset + count <= words.size());
                // SPIR-V OpImageSampleImplicitLod.
                implicit_lod += (words[offset] & 0xffffu) == 87;
                offset += count;
            }
            if (desc.type == gfx::ShaderType::Vertex) {
                EXPECT_EQ(implicit_lod, 0u);
            } else {
                EXPECT_GT(implicit_lod, 0u);
            }
            ++checked;
        }
    }
    EXPECT_EQ(checked, 3u);
}
