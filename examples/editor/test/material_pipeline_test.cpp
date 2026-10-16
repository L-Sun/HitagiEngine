#include "interop/gtest_macros.hpp"

import std;
import interop.gtest;

import editor;
import preset.pbr;
import preset.pbr.authoring;
import engine;

using namespace hitagi;
using namespace hitagi::preset::pbr;
using namespace hitagi::asset;
using namespace hitagi::math;

TEST(MaterialContractCookTest, RejectsMissingAndMistypedParameters) {
    const auto missing = std::make_shared<Material>(MaterialParameters{},
        std::pmr::vector<MaterialPass>{{.pass_contract = "Forward", .bindings = {{"roughness", 0.5f}}}}, "Missing");
    EXPECT_THROW(CookEditorMaterial(EditorMaterial{.material = missing}), std::invalid_argument);
    const auto wrong = std::make_shared<Material>(MaterialParameters{{.name = "roughness", .value = std::uint32_t{1}}},
        std::pmr::vector<MaterialPass>{{.pass_contract = "Forward", .bindings = {{"roughness", 0.5f}}}}, "WrongType");
    EXPECT_THROW(CookEditorMaterial(EditorMaterial{.material = wrong}), std::invalid_argument);
}

TEST(PbrMaterialCompilerTest, RequiresMatchingContractDefaults) {
    MaterialGraph graph;
    const auto value = graph.AddNode("roughness", "parameter", {}, {{"value", ShaderValueType::Float}});
    const auto surface = graph.AddNode("surface", "surface_output", {{"roughness", ShaderValueType::Float}}, {{"out", ShaderValueType::Surface}});
    graph.AddEdge(value, "value", surface, "roughness");
    graph.AddTerminal("surface", surface, "out", ShaderValueType::Surface);
    EXPECT_FALSE(PbrMaterialCompiler{}.Compile(graph, {}));
    const MaterialParameters wrong{{.name = "roughness", .value = std::uint32_t{1}}};
    EXPECT_FALSE(PbrMaterialCompiler{}.Compile(graph, wrong));
    const auto result = PbrMaterialCompiler{}.Compile(graph, PbrMaterialDefaults());
    ASSERT_TRUE(result);
    ASSERT_EQ(result.pass.bindings.size(), 1);
    EXPECT_FLOAT_EQ(std::get<float>(result.pass.bindings[0].default_value), 0.5f);
}

namespace hitagi::material_pipeline_tests {

bool HasGraphError(const MaterialGraphValidationResult& result, MaterialGraphValidationErrorCode code) {
    return std::any_of(result.errors.begin(), result.errors.end(), [code](const auto& error) {
        return error.code == code;
    });
}

void WriteTextFile(const std::filesystem::path& path, std::string_view content) {
    if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    ASSERT_TRUE(file.is_open());
    file.write(content.data(), static_cast<std::streamsize>(content.size()));
}

class EditorMaterialCookTest : public ::testing::Test {
protected:
    core::FileIOManager file_io;
    core::JobSystem     job_system;
};

}  // namespace hitagi::material_pipeline_tests

using namespace hitagi::material_pipeline_tests;
TEST_F(EditorMaterialCookTest, CookedMaterialRoundTripPreservesParametersAndPasses) {
    const std::filesystem::path base_texture_path{"assets/test/test.png"};
    const auto                  base_texture = std::make_shared<Texture>(base_texture_path, MakeFileImageLoader(file_io, base_texture_path), "base");
    MaterialPass                gbuffer_pass{
        .pass_contract = "GBuffer",
        .bindings = {{"base_color", Color::White()}, {"metallic", 0.0f}, {"base_color_texture", std::shared_ptr<Texture>{}}},
    };
    const auto material = std::make_shared<Material>(
        MaterialParameters{
            {.name = "base_color", .value = Color{0.2f, 0.4f, 0.6f, 1.0f}},
            {.name = "metallic", .value = 0.75f},
            {.name = "tint", .value = vec3f{0.1f, 0.2f, 0.3f}},
            {.name = "base_color_texture", .value = base_texture},
            {.name = "detail_texture", .value = base_texture},
            {.name = "missing_texture", .value = std::shared_ptr<Texture>{}},
        },
        std::pmr::vector<MaterialPass>{std::move(gbuffer_pass)},
        "BinaryCookedMaterial");

    const auto cooked = CookEditorMaterial(EditorMaterial{.material = material}, EditorCookOptions{.asset_root_path = "assets"});
    ASSERT_FALSE(cooked.Empty());
    EXPECT_TRUE(IsCookedBinary(cooked.Span<std::byte>()));

    AssetManager asset_manager(file_io, job_system, "assets");
    const auto   loaded = asset_manager.LoadCookedMaterial(cooked, "assets");
    ASSERT_TRUE(loaded);
    EXPECT_EQ(loaded->GetName(), "BinaryCookedMaterial");

    ASSERT_EQ(loaded->GetPasses().size(), 1);
    EXPECT_EQ(loaded->GetPasses().front().pass_contract, "GBuffer");
    const auto loaded_passes = loaded->GetPasses();
    const auto& loaded_bindings = loaded_passes.front().bindings;
    ASSERT_EQ(loaded_bindings.size(), 3);
    EXPECT_EQ(loaded_bindings[0].name, "base_color");
    EXPECT_EQ(std::get<Color>(loaded_bindings[0].default_value), Color::White());
    EXPECT_EQ(std::get<float>(loaded_bindings[1].default_value), 0.0f);
    EXPECT_EQ(std::get<std::shared_ptr<Texture>>(loaded_bindings[2].default_value), nullptr);

    const auto loaded_base = loaded->GetParameter<Color>("base_color");
    ASSERT_TRUE(loaded_base);
    EXPECT_EQ(*loaded_base, Color(0.2f, 0.4f, 0.6f, 1.0f));
    const auto loaded_metallic = loaded->GetParameter<float>("metallic");
    ASSERT_TRUE(loaded_metallic);
    EXPECT_EQ(*loaded_metallic, 0.75f);
    const auto loaded_tint = loaded->GetParameter<vec3f>("tint");
    ASSERT_TRUE(loaded_tint);
    EXPECT_EQ(*loaded_tint, vec3f(0.1f, 0.2f, 0.3f));

    const auto loaded_texture = loaded->GetParameter<std::shared_ptr<Texture>>("base_color_texture");
    ASSERT_TRUE(loaded_texture);
    ASSERT_TRUE(*loaded_texture);
    EXPECT_EQ((*loaded_texture)->GetPath().generic_string(), "assets/test/test.png");
    const auto loaded_null_texture = loaded->GetParameter<std::shared_ptr<Texture>>("missing_texture");
    ASSERT_TRUE(loaded_null_texture);
    EXPECT_EQ(*loaded_null_texture, nullptr);

    // P0 registry: both parameters reference the same path, so the AssetManager
    // must resolve them to a single texture instance.
    const auto loaded_detail_texture = loaded->GetParameter<std::shared_ptr<Texture>>("detail_texture");
    ASSERT_TRUE(loaded_detail_texture);
    ASSERT_TRUE(*loaded_detail_texture);
    EXPECT_EQ(*loaded_texture, *loaded_detail_texture);
}

TEST_F(EditorMaterialCookTest, ContractTextureDefaultsRoundTripThroughRegistry) {
    const std::filesystem::path path{"assets/test/test.png"};
    const auto texture = std::make_shared<Texture>(path, MakeFileImageLoader(file_io, path), "default");
    const auto material = std::make_shared<Material>(
        MaterialParameters{{.name = "albedo", .value = texture}},
        std::pmr::vector<MaterialPass>{{.pass_contract = "Forward", .bindings = {{"albedo", texture}}}});
    const auto cooked = CookEditorMaterial(EditorMaterial{.material = material}, {.asset_root_path = "assets"});
    AssetManager manager(file_io, job_system, "assets");
    const auto loaded = manager.LoadCookedMaterial(cooked, "assets");
    ASSERT_TRUE(loaded);
    const MaterialPass* pass = loaded->FindPass("Forward");
    ASSERT_NE(pass, nullptr);
    const auto default_texture = std::get<std::shared_ptr<Texture>>(pass->bindings[0].default_value);
    ASSERT_TRUE(default_texture);
    EXPECT_EQ(default_texture->GetPath().generic_string(), "assets/test/test.png");
    EXPECT_EQ(default_texture, loaded->GetParameter<std::shared_ptr<Texture>>("albedo").value());

    core::Buffer old_version(cooked.GetDataSize(), cooked.GetData());
    CookedFileHeader header;
    std::memcpy(&header, old_version.GetData(), sizeof(header));
    header.version = kCookedVersion - 1;
    std::memcpy(old_version.GetData(), &header, sizeof(header));
    EXPECT_THROW(manager.LoadCookedMaterial(old_version, "assets"), std::runtime_error);
}

TEST_F(EditorMaterialCookTest, CookPreservesSlangModulesAndSpecializationTypes) {
    const gfx::ShaderDesc shader_desc{
        .name = "SpecializedMaterial",
        .type = gfx::ShaderType::Pixel,
        .entry = "shade",
        .program = {.modules = {
            {.name = "material_types", .source_code = "public struct Material {}", .path = "assets/shaders/material_types.slang"},
            {.name = "material_pass", .source_code = "import material_types;", .path = "assets/shaders/material_pass.slang"},
        }},
        .specialization_types = {"Material"},
    };
    auto pipeline = std::make_shared<asset::RenderPipeline>(
        gfx::RenderPipelineDesc{.name = "SpecializedPipeline"},
        std::pmr::vector<std::shared_ptr<asset::Shader>>{std::make_shared<asset::Shader>(shader_desc)});
    const auto material = std::make_shared<Material>(MaterialParameters{},
        std::pmr::vector<MaterialPass>{{.pass_contract = "Forward", .pipeline = pipeline}});
    const auto cooked = CookEditorMaterial(EditorMaterial{.material = material}, {.asset_root_path = "assets"});
    AssetManager manager(file_io, job_system, "assets");
    const auto loaded = manager.LoadCookedMaterial(cooked, "assets");
    ASSERT_TRUE(loaded);
    const auto* pass = loaded->FindPass("Forward");
    ASSERT_NE(pass, nullptr);
    ASSERT_TRUE(pass->pipeline);
    ASSERT_EQ(pass->pipeline->GetShaders().size(), 1);
    const auto& restored = pass->pipeline->GetShaders()[0]->GetDesc();
    EXPECT_EQ(restored.name, shader_desc.name);
    EXPECT_EQ(restored.type, shader_desc.type);
    EXPECT_EQ(restored.entry, shader_desc.entry);
    EXPECT_EQ(restored.specialization_types, shader_desc.specialization_types);
    ASSERT_EQ(restored.program.modules.size(), shader_desc.program.modules.size());
    for (std::size_t i = 0; i < restored.program.modules.size(); ++i) {
        EXPECT_EQ(restored.program.modules[i].name, shader_desc.program.modules[i].name);
        EXPECT_EQ(restored.program.modules[i].source_code, shader_desc.program.modules[i].source_code);
        EXPECT_EQ(restored.program.modules[i].path, shader_desc.program.modules[i].path);
    }
}

TEST_F(EditorMaterialCookTest, CookedSceneRoundTripPreservesVertexDataBitExact) {
    auto scene = std::make_shared<Scene>("binary-cooked-scene");
    auto mesh  = MeshFactory::Cube();

    auto material = std::make_shared<Material>(
        MaterialParameters{
            {.name = "base_color", .value = Color{0.3f, 0.5f, 0.7f, 1.0f}},
        },
        std::pmr::vector<MaterialPass>{
            MaterialPass{.pass_contract = "GBuffer", .bindings = {{"base_color", Color::White()}}},
        },
        "BinaryCookedSceneMaterial");
    for (auto& sub_mesh : mesh->sub_meshes) sub_mesh.material = material;
    mesh->ComputeAABB();

    scene->CreateMeshEntity(mesh, math::translate(vec3f{1.0f, 2.0f, 3.0f}), scene->GetRootEntity(), "cube");
    scene->CreateCameraEntity(std::make_shared<Camera>(Camera::Parameters{}, "camera"), mat4f::identity(), scene->GetRootEntity(), "camera");
    scene->CreateLightEntity(std::make_shared<Light>(Light::Parameters{}, "light"), mat4f::identity(), scene->GetRootEntity(), "light");
    scene->Update();

    const auto cooked = CookEditorScene(*scene, EditorCookOptions{.asset_root_path = "assets"});
    ASSERT_FALSE(cooked.Empty());
    EXPECT_TRUE(IsCookedBinary(cooked.Span<std::byte>()));

    AssetManager asset_manager(file_io, job_system, "assets");
    const auto   loaded = asset_manager.LoadCookedScene(cooked, "assets");
    ASSERT_TRUE(loaded);
    ASSERT_EQ(loaded->GetMeshEntities().size(), 1);
    ASSERT_EQ(loaded->GetCameraEntities().size(), 1);
    ASSERT_EQ(loaded->GetLightEntities().size(), 1);

    const auto loaded_mesh = loaded->GetMeshEntities().front().Get<MeshComponent>().mesh;
    ASSERT_TRUE(loaded_mesh);
    ASSERT_TRUE(loaded_mesh->vertices);
    ASSERT_TRUE(loaded_mesh->indices);
    ASSERT_EQ(loaded_mesh->vertices->Size(), mesh->vertices->Size());
    ASSERT_EQ(loaded_mesh->indices->Size(), mesh->indices->Size());

    const auto original_positions = std::as_const(*mesh->vertices).Span<VertexAttribute::Position>();
    const auto loaded_positions   = std::as_const(*loaded_mesh->vertices).Span<VertexAttribute::Position>();
    ASSERT_EQ(loaded_positions.size(), original_positions.size());
    EXPECT_TRUE(std::ranges::equal(
        std::as_bytes(loaded_positions),
        std::as_bytes(original_positions)));

    const auto original_indices = std::as_const(*mesh->indices).Span<IndexType::UINT16>();
    const auto loaded_indices   = std::as_const(*loaded_mesh->indices).Span<IndexType::UINT16>();
    EXPECT_TRUE(std::ranges::equal(
        std::as_bytes(loaded_indices),
        std::as_bytes(original_indices)));

    EXPECT_EQ(loaded_mesh->aabb.min_point, mesh->aabb.min_point);
    EXPECT_EQ(loaded_mesh->aabb.max_point, mesh->aabb.max_point);

    ASSERT_FALSE(loaded_mesh->sub_meshes.empty());
    ASSERT_TRUE(loaded_mesh->sub_meshes.front().material);
    EXPECT_EQ(loaded_mesh->sub_meshes.front().material->GetName(), "BinaryCookedSceneMaterial");

    const auto cooked_path = std::filesystem::path("build/test/cooked_scene_roundtrip_binary.hcscene");
    WriteTextFile(cooked_path, cooked.Str());

    const auto imported = asset_manager.ImportScene(cooked_path);
    ASSERT_TRUE(imported);
    ASSERT_EQ(imported->GetMeshEntities().size(), 1);
    const auto imported_material = imported->GetMeshEntities().front().Get<MeshComponent>().mesh->sub_meshes.front().material;
    ASSERT_TRUE(imported_material);
    EXPECT_EQ(imported_material->GetName(), "BinaryCookedSceneMaterial");
}

TEST(MaterialGraphTest, SingleNodeGraphValid) {
    MaterialGraph graph;
    const auto    surface = graph.AddNode(
        "surface",
        "surface_output",
        {},
        {{"out", ShaderValueType::Surface}},
        {.source_node_path = "/Looks/TestMaterial", .source_shader_id = "HitagiSurface", .source_socket_name = "outputs:surface"});
    graph.AddTerminal("surface", surface, "out", ShaderValueType::Surface);

    EXPECT_TRUE(graph.Validate());
    ASSERT_EQ(graph.GetNodes().size(), 1);
    EXPECT_EQ(graph.GetNodes()[0].metadata.source_node_path, "/Looks/TestMaterial");
}

TEST(MaterialGraphTest, TextureSampleToSurfaceGraphValid) {
    MaterialGraph graph;
    const auto    texture = graph.AddNode("base_color_texture", "texture_2d", {}, {{"texture", ShaderValueType::Texture2D}});
    const auto    uv      = graph.AddNode("uv0", "primvar", {}, {{"uv", ShaderValueType::Float2}});
    const auto    sample  = graph.AddNode(
        "sample_base_color",
        "texture_sample",
        {{"texture", ShaderValueType::Texture2D}, {"uv", ShaderValueType::Float2}},
        {{"rgba", ShaderValueType::Color}});
    const auto surface = graph.AddNode(
        "surface",
        "pbr_surface",
        {{"base_color", ShaderValueType::Color}},
        {{"out", ShaderValueType::Surface}});

    graph.AddEdge(texture, "texture", sample, "texture");
    graph.AddEdge(uv, "uv", sample, "uv");
    graph.AddEdge(sample, "rgba", surface, "base_color");
    graph.AddTerminal("surface", surface, "out", ShaderValueType::Surface);

    EXPECT_TRUE(graph.Validate());
}

TEST(MaterialGraphTest, MissingSocketReportsError) {
    MaterialGraph graph;
    const auto    value   = graph.AddNode("value", "constant", {}, {{"out", ShaderValueType::Float}});
    const auto    surface = graph.AddNode("surface", "surface", {{"roughness", ShaderValueType::Float}}, {{"out", ShaderValueType::Surface}});

    graph.AddEdge(value, "missing", surface, "roughness");
    graph.AddTerminal("surface", surface, "out", ShaderValueType::Surface);

    const auto result = graph.Validate();
    EXPECT_FALSE(result);
    EXPECT_TRUE(HasGraphError(result, MaterialGraphValidationErrorCode::MissingSocket));
}

TEST(MaterialGraphTest, TypeMismatchReportsError) {
    MaterialGraph graph;
    const auto    color   = graph.AddNode("color", "constant", {}, {{"out", ShaderValueType::Color}});
    const auto    surface = graph.AddNode("surface", "surface", {{"roughness", ShaderValueType::Float}}, {{"out", ShaderValueType::Surface}});

    graph.AddEdge(color, "out", surface, "roughness");
    graph.AddTerminal("surface", surface, "out", ShaderValueType::Surface);

    const auto result = graph.Validate();
    EXPECT_FALSE(result);
    EXPECT_TRUE(HasGraphError(result, MaterialGraphValidationErrorCode::TypeMismatch));
}

TEST(MaterialGraphTest, TerminalValidationReportsError) {
    MaterialGraph graph;
    const auto    surface = graph.AddNode("surface", "surface", {}, {{"out", ShaderValueType::Surface}});

    graph.AddTerminal("surface", surface, "missing", ShaderValueType::Surface);

    const auto result = graph.Validate();
    EXPECT_FALSE(result);
    EXPECT_TRUE(HasGraphError(result, MaterialGraphValidationErrorCode::MissingTerminal));
}

TEST(MaterialGraphTest, CycleReportsError) {
    MaterialGraph graph;
    const auto    a = graph.AddNode("a", "math", {{"in", ShaderValueType::Float}}, {{"out", ShaderValueType::Float}});
    const auto    b = graph.AddNode("b", "math", {{"in", ShaderValueType::Float}}, {{"out", ShaderValueType::Float}});

    graph.AddEdge(a, "out", b, "in");
    graph.AddEdge(b, "out", a, "in");
    graph.AddTerminal("value", a, "out", ShaderValueType::Float);

    const auto result = graph.Validate();
    EXPECT_FALSE(result);
    EXPECT_TRUE(HasGraphError(result, MaterialGraphValidationErrorCode::Cycle));
}

TEST(MaterialGraphTest, TopologicalSortIsStable) {
    MaterialGraph graph;
    const auto    a = graph.AddNode("a", "constant", {}, {{"out", ShaderValueType::Float}});
    const auto    b = graph.AddNode("b", "constant", {}, {{"out", ShaderValueType::Float}});
    const auto    c = graph.AddNode("c", "add", {{"lhs", ShaderValueType::Float}, {"rhs", ShaderValueType::Float}}, {{"out", ShaderValueType::Float}});
    const auto    d = graph.AddNode("d", "surface", {{"value", ShaderValueType::Float}}, {{"out", ShaderValueType::Surface}});

    graph.AddEdge(a, "out", c, "lhs");
    graph.AddEdge(b, "out", c, "rhs");
    graph.AddEdge(c, "out", d, "value");
    graph.AddTerminal("surface", d, "out", ShaderValueType::Surface);

    const auto order = graph.TopologicalSort();
    ASSERT_EQ(order.size(), 4);
    EXPECT_EQ(order[0], a);
    EXPECT_EQ(order[1], b);
    EXPECT_EQ(order[2], c);
    EXPECT_EQ(order[3], d);
}

TEST(MaterialGraphTest, CollectTerminalDependenciesOnlyUsedNodes) {
    MaterialGraph graph;
    const auto    used_texture = graph.AddNode("used_texture", "texture", {}, {{"texture", ShaderValueType::Texture2D}});
    const auto    used_uv      = graph.AddNode("used_uv", "primvar", {}, {{"uv", ShaderValueType::Float2}});
    const auto    sample       = graph.AddNode(
        "sample",
        "texture_sample",
        {{"texture", ShaderValueType::Texture2D}, {"uv", ShaderValueType::Float2}},
        {{"rgba", ShaderValueType::Color}});
    const auto unused  = graph.AddNode("unused", "constant", {}, {{"out", ShaderValueType::Color}});
    const auto surface = graph.AddNode(
        "surface",
        "surface",
        {{"base_color", ShaderValueType::Color}},
        {{"out", ShaderValueType::Surface}});

    graph.AddEdge(used_texture, "texture", sample, "texture");
    graph.AddEdge(used_uv, "uv", sample, "uv");
    graph.AddEdge(sample, "rgba", surface, "base_color");
    graph.AddTerminal("surface", surface, "out", ShaderValueType::Surface);

    const auto dependencies = graph.CollectTerminalDependencies("surface");
    ASSERT_EQ(dependencies.size(), 4);
    EXPECT_EQ(dependencies[0], used_texture);
    EXPECT_EQ(dependencies[1], used_uv);
    EXPECT_EQ(dependencies[2], sample);
    EXPECT_EQ(dependencies[3], surface);
    EXPECT_EQ(std::find(dependencies.begin(), dependencies.end(), unused), dependencies.end());
}

TEST(PbrMaterialCompilerTest, GeneratesReadablePixelShaderAndPassBindingSnapshot) {
    MaterialGraph graph;
    const auto    texture = graph.AddNode("base_color_texture", "texture_2d", {}, {{"texture", ShaderValueType::Texture2D}});
    const auto    uv      = graph.AddNode("uv0", "uv", {}, {{"uv", ShaderValueType::Float2}});
    const auto    sample  = graph.AddNode(
        "sample_base_color",
        "texture_sample",
        {{"texture", ShaderValueType::Texture2D}, {"uv", ShaderValueType::Float2}},
        {{"rgba", ShaderValueType::Color}});
    const auto metallic  = graph.AddNode("metallic", "parameter", {}, {{"value", ShaderValueType::Float}});
    const auto roughness = graph.AddNode("roughness_default", "constant", {}, {{"value", ShaderValueType::Float}});
    const auto surface   = graph.AddNode(
        "surface",
        "surface_output",
        {{"base_color", ShaderValueType::Color}, {"metallic", ShaderValueType::Float}, {"roughness", ShaderValueType::Float}},
        {{"out", ShaderValueType::Surface}});

    graph.AddEdge(texture, "texture", sample, "texture");
    graph.AddEdge(uv, "uv", sample, "uv");
    graph.AddEdge(sample, "rgba", surface, "base_color");
    graph.AddEdge(metallic, "value", surface, "metallic");
    graph.AddEdge(roughness, "value", surface, "roughness");
    graph.AddTerminal("surface", surface, "out", ShaderValueType::Surface);

    const auto result = PbrMaterialCompiler{}.Compile(graph, PbrMaterialDefaults(), {.shader_name = "SnapshotMaterial"});

    ASSERT_TRUE(result);
    EXPECT_EQ(result.shader.name, "SnapshotMaterial");
    EXPECT_EQ(result.shader.type, gfx::ShaderType::Pixel);
    EXPECT_EQ(result.shader.entry, "main");
    EXPECT_EQ(result.shader.program.modules.back().source_code, result.slang);
    ASSERT_TRUE(result.pass.pipeline);
    const auto shaders = result.pass.pipeline->GetShaders();
    ASSERT_EQ(shaders.size(), 2);
    EXPECT_EQ(shaders[0]->GetDesc().type, gfx::ShaderType::Vertex);
    EXPECT_EQ(shaders[1]->GetDesc().name, result.shader.name);
    EXPECT_EQ(result.pipeline_desc.name, "SnapshotMaterialPipeline");

    ASSERT_EQ(result.pass.bindings.size(), 2);
    EXPECT_EQ(result.pass.bindings[0].name, "base_color_texture");
    EXPECT_EQ(result.pass.bindings[1].name, "metallic");

    EXPECT_NE(result.slang.find("import bindless;"), std::pmr::string::npos);
    EXPECT_EQ(result.slang.find("textures[7]"), std::pmr::string::npos);
    EXPECT_NE(result.slang.find("struct MaterialData"), std::pmr::string::npos);
    EXPECT_EQ(result.slang.find("float4 color : COLOR"), std::pmr::string::npos);
    EXPECT_NE(result.slang.find("hitagi::Texture base_color_texture;"), std::pmr::string::npos);
    EXPECT_NE(result.slang.find("float metallic;"), std::pmr::string::npos);
    EXPECT_NE(result.slang.find("if (hitagi::valid(material_data.base_color_texture.handle)) sample_base_color_rgba = material_data.base_color_texture.sample<float4>(sampler, input.uv);"), std::pmr::string::npos);
    EXPECT_NE(result.slang.find("surface.base_color = sample_base_color_rgba;"), std::pmr::string::npos);
    EXPECT_NE(result.slang.find("surface.metallic = material_data.metallic;"), std::pmr::string::npos);
    EXPECT_NE(result.slang.find("surface.roughness = 0.0f;"), std::pmr::string::npos);
    gfx::ShaderCompiler compiler("PbrTextureContract");
    for (const auto& shader : result.pass.pipeline->GetShaders()) {
        EXPECT_FALSE(compiler.LoadProgram(shader->GetDesc().program, gfx::ShaderTarget::DXIL)->Compile(shader->GetDesc().entry, shader->GetDesc().type, shader->GetDesc().specialization_types).bytecode.Empty());
        EXPECT_FALSE(compiler.LoadProgram(shader->GetDesc().program, gfx::ShaderTarget::SPIRV)->Compile(shader->GetDesc().entry, shader->GetDesc().type, shader->GetDesc().specialization_types).bytecode.Empty());
    }
}

TEST(PbrMaterialCompilerTest, GeneratedIndexedRecordShadersCompileForBothBackends) {
    MaterialGraph graph;
    const auto    color   = graph.AddNode("base_color", "parameter", {}, {{"value", ShaderValueType::Color}});
    const auto    surface = graph.AddNode("surface", "surface_output", {{"base_color", ShaderValueType::Color}}, {{"out", ShaderValueType::Surface}});
    graph.AddEdge(color, "value", surface, "base_color");
    graph.AddTerminal("surface", surface, "out", ShaderValueType::Surface);

    const auto result = PbrMaterialCompiler{}.Compile(graph, PbrMaterialDefaults(), {.shader_name = "CompilableMaterial"});
    ASSERT_TRUE(result);

    EXPECT_NE(result.slang.find("uint instance_index;"), std::string::npos);
    EXPECT_NE(result.slang.find("resource.instance_index * resource.instance_stride"), std::string::npos);
    gfx::ShaderCompiler compiler("PbrMaterialCompilerTest.GeneratedIndexedRecordShaders");
    for (const auto& shader : result.pass.pipeline->GetShaders()) {
        EXPECT_FALSE(compiler.LoadProgram(shader->GetDesc().program, gfx::ShaderTarget::DXIL)->Compile(shader->GetDesc().entry, shader->GetDesc().type, shader->GetDesc().specialization_types).bytecode.Empty());
        EXPECT_FALSE(compiler.LoadProgram(shader->GetDesc().program, gfx::ShaderTarget::SPIRV)->Compile(shader->GetDesc().entry, shader->GetDesc().type, shader->GetDesc().specialization_types).bytecode.Empty());
    }
}

TEST(PbrMaterialCompilerTest, SupportsBaseNodeKinds) {
    MaterialGraph graph;
    const auto    vertex_color = graph.AddNode("vertex_color", "vertex_color", {}, {{"color", ShaderValueType::Color}});
    const auto    normal       = graph.AddNode("geometry_normal", "normal", {}, {{"normal", ShaderValueType::Float3}});
    const auto    tint         = graph.AddNode("tint", "parameter", {}, {{"value", ShaderValueType::Color}});
    const auto    mix          = graph.AddNode(
        "mix_vertex_color",
        "mix",
        {{"a", ShaderValueType::Color}, {"b", ShaderValueType::Color}, {"t", ShaderValueType::Float}},
        {{"value", ShaderValueType::Color}});
    const auto normal_map = graph.AddNode(
        "normal_from_sample",
        "normal_map",
        {{"sample", ShaderValueType::Color}},
        {{"normal", ShaderValueType::Float3}});
    const auto surface = graph.AddNode(
        "surface",
        "surface_output",
        {{"base_color", ShaderValueType::Color}, {"normal", ShaderValueType::Float3}},
        {{"out", ShaderValueType::Surface}});

    graph.AddEdge(vertex_color, "color", mix, "a");
    graph.AddEdge(tint, "value", mix, "b");
    graph.AddEdge(vertex_color, "color", normal_map, "sample");
    graph.AddEdge(mix, "value", surface, "base_color");
    graph.AddEdge(normal_map, "normal", surface, "normal");
    graph.AddTerminal("surface", surface, "out", ShaderValueType::Surface);

    auto defaults = PbrMaterialDefaults();
    defaults.push_back({.name = "tint", .value = Color::White()});
    const auto first = PbrMaterialCompiler{}.Compile(graph, defaults, {.shader_name = "BaseNodeMaterial"});
    ASSERT_TRUE(first);
    EXPECT_NE(first.slang.find("float4 color : COLOR"), std::pmr::string::npos);
    EXPECT_NE(first.slang.find("input.color"), std::pmr::string::npos);
    EXPECT_NE(first.slang.find("lerp(input.color, material_data.tint, 0.5f)"), std::pmr::string::npos);
    EXPECT_NE(first.slang.find("normal_from_sample_normal"), std::pmr::string::npos);
}
