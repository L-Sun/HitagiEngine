#include "interop/gtest_macros.hpp"
import std;
import engine;
import anime_viewer;
import interop.gtest;

using namespace hitagi;
using namespace game::anime_viewer;

TEST(AnimeMaterialTest, ImportNormalizesSourceAndAssignsHairParticipation) {
    AnimeMaterialAdapter adapter;
    asset::Material source({{.name = "animeMaterial", .value = 3.0f},
                            {.name = "diffuseColor", .value = math::vec3f{0.2f, 0.4f, 0.6f}},
                            {.name = "opacity", .value = 0.7f}});
    auto hair = adapter.Adapt(&source);
    ASSERT_NE(hair->FindPass(kAnimeHairShadowCaster), nullptr);
    EXPECT_EQ(*hair->GetParameter<std::uint32_t>("kind"), 3u);
    EXPECT_EQ(*hair->GetParameter<math::vec4f>("base_color"), (math::vec4f{0.2f, 0.4f, 0.6f, 0.7f}));
    EXPECT_TRUE(hair->ValidateBindings().empty());
    EXPECT_TRUE(source.GetPasses().empty());
    auto body = adapter.Adapt(nullptr);
    EXPECT_EQ(body->FindPass(kAnimeHairShadowCaster), nullptr);
    for (const auto contract : {kAnimeForward, kAnimeAlbedo, kAnimeNormals, kAnimeShadowCaster}) {
        ASSERT_NE(body->FindPass(contract), nullptr);
        EXPECT_NE(body->FindPass(contract)->pipeline, nullptr);
    }
    source.SetParameter("animeMaterial", 2.5f);
    EXPECT_THROW(adapter.Adapt(&source), std::runtime_error);
    asset::Material mistyped({{.name = "animeHead", .value = 1.0f}});
    EXPECT_THROW(adapter.Adapt(&mistyped), std::runtime_error);
}

TEST(AnimeMaterialTest, PackingUsesBindingsAndRebuildsAfterParameterChange) {
    auto device = gfx::create_device(gfx::Device::Type::Mock);
    gfx::CommandQueues queues{*device};
    auto bindings = gfx::BindlessUtils::Create(*device);
    gfx::ShaderCompiler compiler{"Anime material packing"};
    asset::ResourceLoadContext context{*device, queues, *bindings, compiler};
    // Deliberately reorder parameters; omit alpha_cutoff and mistype kind.
    asset::Material material({{.name = "head", .value = math::vec4f{2, 3, 4, 5}},
                               {.name = "kind", .value = 3.0f},
                               {.name = "base_color", .value = math::vec4f{0.2f, 0.3f, 0.4f, 1}}},
                              {{.pass_contract = std::pmr::string(kAnimeForward), .bindings = AnimeMaterialBindings()}});
    EXPECT_FALSE(material.ValidateBindings().empty());
    material.Load(context);
    const auto* pass = material.FindPass(kAnimeForward);
    ASSERT_NE(pass, nullptr);
    ASSERT_EQ(pass->material_data.GetDataSize(), 112u);
    const auto floats = pass->material_data.Span<const float>();
    EXPECT_FLOAT_EQ(floats[0], 0.2f);
    EXPECT_FLOAT_EQ(floats[4], 0.0f); // alpha cutoff default
    EXPECT_EQ(pass->material_data.Span<const std::uint32_t>()[6], 0u); // typed kind default
    EXPECT_FLOAT_EQ(floats[8], 2.0f); // head offset remains stable
    material.SetParameter("base_color", math::vec4f{0.7f, 0.6f, 0.5f, 1});
    EXPECT_TRUE(pass->material_data.Empty());
    material.Load(context);
    EXPECT_FLOAT_EQ(pass->material_data.Span<const float>()[0], 0.7f);
}

TEST(AnimeMaterialTest, GeneratedLayoutsCompileForBothTargets) {
    gfx::ShaderCompiler compiler{"Anime ABI"};
    for (const auto target : {gfx::ShaderTarget::SPIRV, gfx::ShaderTarget::DXIL}) {
        auto program = compiler.LoadProgram(AnimeShaderProgram(), target);
        for (const auto& [entry, type] : {
                std::pair{"MeshVS", gfx::ShaderType::Vertex}, {"MeshPS", gfx::ShaderType::Pixel},
                {"FullscreenVS", gfx::ShaderType::Vertex}, {"FullscreenPS", gfx::ShaderType::Pixel}}) {
            EXPECT_FALSE(program->Compile(entry, type).bytecode.Empty());
        }
    }
}

TEST(AnimeMaterialTest, BatchedMaterialsKeepDifferentSizesAndDrawOffsets) {
    auto device = gfx::create_device(gfx::Device::Type::Vulkan);
    gfx::CommandQueues queues{*device};
    auto bindings = gfx::BindlessUtils::Create(*device);
    gfx::ShaderCompiler compiler{"Batched anime materials"};
    asset::ResourceLoadContext context{*device, queues, *bindings, compiler};
    ToonRenderer renderer{context};
    AnimeMaterialAdapter adapter;
    auto red = adapter.Adapt(nullptr);
    red->SetParameter("base_color", math::vec4f{1, 0, 0, 1});
    std::pmr::vector<asset::MaterialPass> passes(red->GetPasses().begin(), red->GetPasses().end());
    for (auto& pass : passes) pass.bindings.push_back({"extra", math::vec4f{1, 2, 3, 4}});
    auto green = std::make_shared<asset::Material>(
        asset::MaterialParameters(red->GetParameters().begin(), red->GetParameters().end()), std::move(passes));
    green->SetParameter("base_color", math::vec4f{0, 1, 0, 1});

    auto vertices = std::make_shared<asset::VertexArray>(6);
    vertices->Modify<asset::VertexAttribute::Position>([](auto data) {
        data[0] = {-0.9f, -0.8f, 0.5f}; data[1] = {-0.1f, -0.8f, 0.5f}; data[2] = {-0.5f, 0.8f, 0.5f};
        data[3] = {0.1f, -0.8f, 0.5f}; data[4] = {0.9f, -0.8f, 0.5f}; data[5] = {0.5f, 0.8f, 0.5f};
    });
    vertices->Modify<asset::VertexAttribute::Normal>([](auto data) { std::ranges::fill(data, math::vec3f{0, 0, -1}); });
    vertices->Modify<asset::VertexAttribute::UV0>([](auto data) { std::ranges::fill(data, math::vec2f{0.5f, 0.5f}); });
    auto indices = std::make_shared<asset::IndexArray>(6);
    indices->Modify<asset::IndexType::UINT16>([](auto data) { for (std::size_t i = 0; i < 6; ++i) data[i] = static_cast<std::uint16_t>(i); });
    auto mesh = std::make_shared<asset::Mesh>(vertices, indices);
    mesh->sub_meshes = {{.index_count = 3, .material = red}, {.index_count = 3, .index_offset = 3, .material = green}};
    const std::array items{render::RenderDrawItem{.mesh = mesh, .transform = math::mat4f::identity()}};
    const render::RenderView view{.camera_position = {0, 0, -2}, .view = math::mat4f::identity(), .projection = math::mat4f::identity()};
    rg::RenderGraph graph{*device, queues, *bindings};
    auto output = gfx::Texture::Create(*device, queues, *bindings, {
        .width = 64, .height = 64, .format = gfx::Format::R8G8B8A8_UNORM,
        .usages = gfx::TextureUsageFlags::CopySrc | gfx::TextureUsageFlags::CopyDst | gfx::TextureUsageFlags::RenderTarget,
    });
    // Reverse the draw order too: the second record must use the actual size
    // of the preceding material, not a hard-coded 112-byte stride.
    for (unsigned frame = 0; frame < 2; ++frame) {
        const auto images = renderer.Build(graph, items, view, 64, 64, {-0.6f, 0.8f, -0.7f}, 1,
                                           {-1, -1, 0.4f}, {1, 1, 0.6f}, {.scale = 1});
        ASSERT_NE(red->FindPass(kAnimeAlbedo)->material_data.GetDataSize(), green->FindPass(kAnimeAlbedo)->material_data.GetDataSize());
        graph.QueueTextureExtraction(images.stages.at("albedo"), output);
        ASSERT_TRUE(graph.Compile());
        graph.Execute();
        const auto pixels = gfx::readback_texture(*device, queues, *bindings, *output);
        unsigned red_pixels = 0, green_pixels = 0;
        for (std::size_t i = 0; i < 64 * 64; ++i) {
            const auto* pixel = pixels.GetData() + i * 4;
            if (pixel[3] != std::byte{255}) continue;
            if (pixel[0] == std::byte{255} && pixel[1] == std::byte{0}) ++red_pixels;
            if (pixel[0] == std::byte{0} && pixel[1] == std::byte{255}) ++green_pixels;
        }
        EXPECT_GT(red_pixels, 100u);
        EXPECT_GT(green_pixels, 100u);
        graph.ClearImportedResources();
        std::ranges::reverse(mesh->sub_meshes);
    }
}

TEST(AnimeMaterialTest, RendererUsesContractPipelineAndUpdatedMaterialBytes) {
    auto device = gfx::create_device(gfx::Device::Type::Vulkan);
    gfx::CommandQueues queues{*device};
    auto bindings = gfx::BindlessUtils::Create(*device);
    gfx::ShaderCompiler compiler{"Anime GPU contract"};
    asset::ResourceLoadContext context{*device, queues, *bindings, compiler};
    ToonRenderer renderer{context};
    AnimeMaterialAdapter adapter;
    auto material = adapter.Adapt(nullptr);
    auto vertices = std::make_shared<asset::VertexArray>(3);
    vertices->Modify<asset::VertexAttribute::Position>([](auto data) {
        data[0] = {-0.8f, 0, 0.5f}; data[1] = {0.8f, 0, 0.5f}; data[2] = {0, 0.9f, 0.5f};
    });
    vertices->Modify<asset::VertexAttribute::Normal>([](auto data) { std::ranges::fill(data, math::vec3f{0, 0, -1}); });
    vertices->Modify<asset::VertexAttribute::UV0>([](auto data) { std::ranges::fill(data, math::vec2f{0.5f, 0.5f}); });
    auto indices = std::make_shared<asset::IndexArray>(3);
    indices->Modify<asset::IndexType::UINT16>([](auto data) { data[0] = 0; data[1] = 1; data[2] = 2; });
    auto mesh = std::make_shared<asset::Mesh>(vertices, indices);
    mesh->sub_meshes = {{.index_count = 3, .material = material}};
    const std::array items{render::RenderDrawItem{.mesh = mesh, .transform = math::mat4f::identity()}};
    render::RenderView view{.camera_position = {0, 0, -2}, .view = math::mat4f::identity(), .projection = math::mat4f::identity()};
    rg::RenderGraph graph{*device, queues, *bindings};
    auto output = gfx::Texture::Create(*device, queues, *bindings, {
        .name = "Contract capture", .width = 64, .height = 64, .format = gfx::Format::R8G8B8A8_UNORM,
        .usages = gfx::TextureUsageFlags::CopySrc | gfx::TextureUsageFlags::CopyDst | gfx::TextureUsageFlags::RenderTarget,
    });
    const auto capture = [&] {
        const auto images = renderer.Build(graph, items, view, 64, 64, {-0.6f, 0.8f, -0.7f}, 1,
                                           {-0.8f, 0, 0.4f}, {0.8f, 0.9f, 0.6f}, {.scale = 1});
        graph.QueueTextureExtraction(images.stages.at("albedo"), output);
        if (!graph.Compile()) throw std::runtime_error("Contract graph failed");
        graph.Execute();
        auto pixels = gfx::readback_texture(*device, queues, *bindings, *output);
        // Uniform test materials: inspect a covered pixel without depending on
        // the viewport's Y direction. An empty contract must leave all alpha zero.
        std::array<unsigned, 4> center{};
        for (unsigned i = 0; i < 64 * 64; ++i) {
            if (pixels.GetData()[i * 4 + 3] == std::byte{0}) continue;
            for (unsigned c = 0; c < 4; ++c) center[c] = std::to_integer<unsigned>(pixels.GetData()[i * 4 + c]);
            break;
        }
        graph.ClearImportedResources();
        return center;
    };
    material->SetParameter("base_color", math::vec4f{1, 0, 0, 1});
    EXPECT_EQ(capture(), (std::array<unsigned, 4>{255, 0, 0, 255}));
    material->SetParameter("base_color", math::vec4f{0, 1, 0, 1});
    EXPECT_EQ(capture(), (std::array<unsigned, 4>{0, 255, 0, 255}));

    // The current gfx API infers a 1D image from height=1; exercise Texture2D.
    const std::array<std::byte, 16> texel{
        std::byte{255}, std::byte{128}, std::byte{0}, std::byte{255},
        std::byte{255}, std::byte{128}, std::byte{0}, std::byte{255},
        std::byte{255}, std::byte{128}, std::byte{0}, std::byte{255},
        std::byte{255}, std::byte{128}, std::byte{0}, std::byte{255}};
    auto texture = std::make_shared<asset::Texture>(2, 2, gfx::Format::R8G8B8A8_UNORM,
                                                   core::Buffer(texel.size(), texel.data()));
    material->SetParameter("base_color", math::vec4f{1, 1, 1, 1});
    material->SetParameter("base_color_texture", texture);
    material->SetParameter("textured", std::uint32_t{1});
    EXPECT_EQ(capture(), (std::array<unsigned, 4>{255, 128, 0, 255}));
    const auto* texture_pass = material->FindPass(kAnimeAlbedo);
    const auto dependencies = AnimeMaterialViews(*material, *texture_pass);
    ASSERT_EQ(dependencies.size(), 1u);
    EXPECT_EQ(dependencies.front(), texture->GetGPUView());
    EXPECT_TRUE(AnimeMaterialViews(*material, asset::MaterialPass{}).empty());

    std::pmr::vector<asset::MaterialPass> passes(material->GetPasses().begin(), material->GetPasses().end());
    auto& albedo = *std::ranges::find(passes, kAnimeAlbedo, &asset::MaterialPass::pass_contract);
    auto custom_pixel = std::make_shared<asset::Shader>(gfx::ShaderDesc{
        .name = "Contract blue", .type = gfx::ShaderType::Pixel, .entry = "Blue",
        .program = {.modules = {{.source_code = "float4 Blue() : SV_Target { return float4(0,0,1,1); }"}}},
    });
    albedo.pipeline = std::make_shared<asset::RenderPipeline>(albedo.pipeline->GetDesc(),
        std::pmr::vector<std::shared_ptr<asset::Shader>>{albedo.pipeline->GetShaders().front(), custom_pixel});
    const asset::MaterialParameters parameters(material->GetParameters().begin(), material->GetParameters().end());
    mesh->sub_meshes.front().material = std::make_shared<asset::Material>(parameters, passes);
    EXPECT_EQ(capture(), (std::array<unsigned, 4>{0, 0, 255, 255}));
    std::erase_if(passes, [](const auto& pass) { return pass.pass_contract == kAnimeAlbedo; });
    mesh->sub_meshes.front().material = std::make_shared<asset::Material>(parameters, passes);
    EXPECT_EQ(capture(), (std::array<unsigned, 4>{0, 0, 0, 0}));
}
