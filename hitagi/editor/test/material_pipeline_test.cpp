#include "interop/gtest_macros.hpp"

import std;
import interop.gtest;

import editor;
import render;
import engine;

using namespace hitagi;
using namespace hitagi::asset;
using namespace hitagi::math;

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

void ExpectCompilesForAvailableBackends(const MaterialPass& pass) {
    ASSERT_TRUE(pass.pipeline);
    gfx::ShaderCompiler compiler("MaterialGraphCompilerTest");
#ifdef __APPLE__
    // Exercise native Metal lowering too; DXIL's downstream compiler is not
    // available on this host. Other hosts retain their existing offline checks.
    auto                 device = gfx::create_device(gfx::Device::Type::Metal);
    constexpr std::array targets{gfx::ShaderTarget::SPIRV};
#else
    constexpr std::array targets{gfx::ShaderTarget::DXIL, gfx::ShaderTarget::SPIRV};
#endif
    for (const auto& shader : pass.pipeline->GetShaders()) {
        const auto& desc = shader->GetDesc();
#ifdef __APPLE__
        EXPECT_NE(gfx::Shader::Create(*device, compiler, desc), nullptr);
#endif
        for (const auto target : targets) {
            EXPECT_FALSE(compiler.LoadProgram(desc.program, target)->Compile(desc.entry, desc.type, desc.specialization_types).bytecode.Empty());
        }
    }
}

// A shell unrelated to PBR: the compiler must only rely on the IMaterial contract.
auto TestPassTemplate() -> MaterialPassTemplate {
    const std::filesystem::path path{"temp/material_compiler_test/test_shell.slang"};
    WriteTextFile(path, R"slang(import bindless;

struct SurfaceInput {
    float2 uv;
    float3 normal;
    float4 color;
};

struct MaterialContext {
    hitagi::SimpleBuffer material_data;
    SamplerState         sampler;
};

struct Surface {
    float4 albedo;
    float3 normal;
};

interface IMaterial {
    static void evaluate(SurfaceInput input, MaterialContext context, inout Surface surface);
}

struct BindlessInfo {
    hitagi::SimpleBuffer material_data;
    hitagi::Sampler      sampler;
};

struct VSInput {
    float3 position : POSITION;
    float2 uv : TEXCOORD;
    float4 color : COLOR;
};

struct PSInput {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

PSInput VSMain<M : IMaterial>(VSInput input) {
    PSInput output;
    output.position = float4(input.position, 1.0f);
    output.uv       = input.uv;
    output.color    = input.color;
    return output;
}

float4 PSMain<M : IMaterial>(PSInput input) : SV_TARGET {
    const BindlessInfo resource = hitagi::load_bindless<BindlessInfo>();
    SurfaceInput surface_input;
    surface_input.uv     = input.uv;
    surface_input.normal = float3(0.0f, 0.0f, 1.0f);
    surface_input.color  = input.color;
    MaterialContext context;
    context.material_data = resource.material_data;
    context.sampler       = resource.sampler.load();
    Surface surface;
    surface.albedo = float4(1.0f, 1.0f, 1.0f, 1.0f);
    surface.normal = float3(0.0f, 0.0f, 1.0f);
    M.evaluate(surface_input, context, surface);
    return float4(surface.albedo.rgb * (surface.normal.z * 0.5f + 0.5f), 1.0f);
}
)slang");
    return {
        .contract = "Test",
        .shell    = {.name = "test_shell", .path = path},
        .stages   = {{.type = gfx::ShaderType::Vertex, .entry = "VSMain"}, {.type = gfx::ShaderType::Pixel, .entry = "PSMain"}},
    };
}

class EditorMaterialCookTest : public ::testing::Test {
protected:
    core::IOService file_io;
    core::JobSystem job_system;
};

}  // namespace hitagi::material_pipeline_tests

using namespace hitagi::material_pipeline_tests;

TEST(MaterialContractCookTest, RejectsMissingAndMistypedParameters) {
    const auto missing = std::make_shared<Material>(MaterialParameters{},
                                                    std::pmr::vector<MaterialPass>{{.pass_contract = "Forward", .bindings = {{"roughness", 0.5f}}}}, "Missing");
    EXPECT_THROW(CookEditorMaterial(EditorMaterial{.material = missing}), std::invalid_argument);
    const auto wrong = std::make_shared<Material>(MaterialParameters{{.name = "roughness", .value = std::uint32_t{1}}},
                                                  std::pmr::vector<MaterialPass>{{.pass_contract = "Forward", .bindings = {{"roughness", 0.5f}}}}, "WrongType");
    EXPECT_THROW(CookEditorMaterial(EditorMaterial{.material = wrong}), std::invalid_argument);
}

TEST(MaterialGraphCompilerTest, RequiresMatchingContractDefaults) {
    MaterialGraph graph;
    const auto    value   = graph.AddNode("roughness", "parameter", {}, {{"value", ShaderValueType::Float}});
    const auto    surface = graph.AddNode("surface", "surface_output", {{"roughness", ShaderValueType::Float}}, {{"out", ShaderValueType::Surface}});
    graph.AddEdge(value, "value", surface, "roughness");
    graph.AddTerminal("surface", surface, "out", ShaderValueType::Surface);
    const auto pass_template = render::pbr::PbrForwardPassTemplate();
    EXPECT_FALSE(CompileMaterialGraph(graph, pass_template, {}));
    const MaterialParameters wrong{{.name = "roughness", .value = std::uint32_t{1}}};
    EXPECT_FALSE(CompileMaterialGraph(graph, pass_template, wrong));
    const auto result = CompileMaterialGraph(graph, pass_template, render::pbr::PbrMaterialDefaults());
    ASSERT_TRUE(result);
    ASSERT_EQ(result.pass.bindings.size(), 1);
    EXPECT_FLOAT_EQ(std::get<float>(result.pass.bindings[0].default_value), 0.5f);
}
TEST_F(EditorMaterialCookTest, CookedMaterialRoundTripPreservesParametersAndPasses) {
    const std::filesystem::path base_texture_path{"assets/test/test.png"};
    const auto                  base_texture = std::make_shared<Texture>(base_texture_path, MakeFileImageLoader(file_io, base_texture_path), "base");
    MaterialPass                gbuffer_pass{
        .pass_contract = "GBuffer",
        .bindings      = {{"base_color", Color::White()}, {"metallic", 0.0f}, {"base_color_texture", std::shared_ptr<Texture>{}}},
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

    AssetService asset_manager(file_io, job_system, "assets");
    const auto   loaded = asset_manager.LoadCookedMaterial(cooked, "assets");
    ASSERT_TRUE(loaded);
    EXPECT_EQ(loaded->GetName(), "BinaryCookedMaterial");

    ASSERT_EQ(loaded->GetPasses().size(), 1);
    EXPECT_EQ(loaded->GetPasses().front().pass_contract, "GBuffer");
    const auto  loaded_passes   = loaded->GetPasses();
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

    // P0 registry: both parameters reference the same path, so the AssetService
    // must resolve them to a single texture instance.
    const auto loaded_detail_texture = loaded->GetParameter<std::shared_ptr<Texture>>("detail_texture");
    ASSERT_TRUE(loaded_detail_texture);
    ASSERT_TRUE(*loaded_detail_texture);
    EXPECT_EQ(*loaded_texture, *loaded_detail_texture);
}

TEST_F(EditorMaterialCookTest, ContractTextureDefaultsRoundTripThroughRegistry) {
    const std::filesystem::path path{"assets/test/test.png"};
    const auto                  texture  = std::make_shared<Texture>(path, MakeFileImageLoader(file_io, path), "default");
    const auto                  material = std::make_shared<Material>(
        MaterialParameters{{.name = "albedo", .value = texture}},
        std::pmr::vector<MaterialPass>{{.pass_contract = "Forward", .bindings = {{"albedo", texture}}}});
    const auto   cooked = CookEditorMaterial(EditorMaterial{.material = material}, {.asset_root_path = "assets"});
    AssetService manager(file_io, job_system, "assets");
    const auto   loaded = manager.LoadCookedMaterial(cooked, "assets");
    ASSERT_TRUE(loaded);
    const MaterialPass* pass = loaded->FindPass("Forward");
    ASSERT_NE(pass, nullptr);
    const auto default_texture = std::get<std::shared_ptr<Texture>>(pass->bindings[0].default_value);
    ASSERT_TRUE(default_texture);
    EXPECT_EQ(default_texture->GetPath().generic_string(), "assets/test/test.png");
    EXPECT_EQ(default_texture, loaded->GetParameter<std::shared_ptr<Texture>>("albedo").value());

    core::Buffer     old_version(cooked.GetDataSize(), cooked.GetData());
    CookedFileHeader header;
    std::memcpy(&header, old_version.GetData(), sizeof(header));
    header.version = kCookedVersion - 1;
    std::memcpy(old_version.GetData(), &header, sizeof(header));
    EXPECT_THROW(manager.LoadCookedMaterial(old_version, "assets"), std::runtime_error);
}

TEST_F(EditorMaterialCookTest, CookPreservesSlangModulesAndSpecializationTypes) {
    const gfx::ShaderDesc shader_desc{
        .name                 = "SpecializedMaterial",
        .type                 = gfx::ShaderType::Pixel,
        .entry                = "shade",
        .program              = {.modules = {
                                     {.name = "material_types", .source_code = "public struct Material {}", .path = "assets/shaders/material_types.slang"},
                                     {.name = "material_pass", .source_code = "import material_types;", .path = "assets/shaders/material_pass.slang"},
                                 }},
        .specialization_types = {"Material"},
    };
    auto pipeline = std::make_shared<asset::RenderPipeline>(
        gfx::RenderPipelineDesc{.name = "SpecializedPipeline"},
        std::pmr::vector<std::shared_ptr<asset::Shader>>{std::make_shared<asset::Shader>(shader_desc)});
    const auto   material = std::make_shared<Material>(MaterialParameters{},
                                                       std::pmr::vector<MaterialPass>{{.pass_contract = "Forward", .pipeline = pipeline}});
    const auto   cooked   = CookEditorMaterial(EditorMaterial{.material = material}, {.asset_root_path = "assets"});
    AssetService manager(file_io, job_system, "assets");
    const auto   loaded = manager.LoadCookedMaterial(cooked, "assets");
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
    auto              scene = std::make_shared<Scene>("binary-cooked-scene");
    VertexArray::Data vertex_data;
    core::Buffer      index_data(3 * sizeof(std::uint16_t));
    {
        vertex_data[VertexAttribute::Position] = core::Buffer(3 * sizeof(VertexDataType<VertexAttribute::Position>));
        auto p                                 = vertex_data[VertexAttribute::Position].Span<VertexDataType<VertexAttribute::Position>>();
        p[0]                                   = {-0.5f, -0.5f, 0.5f};
        p[1]                                   = {0.5f, -0.5f, 0.5f};
        p[2]                                   = {0.5f, 0.5f, 0.5f};
    }
    {
        vertex_data[VertexAttribute::Normal] = core::Buffer(3 * sizeof(VertexDataType<VertexAttribute::Normal>));
        auto p                               = vertex_data[VertexAttribute::Normal].Span<VertexDataType<VertexAttribute::Normal>>();
        std::ranges::fill(p, math::vec3f(0, 0, 1));
    }
    {
        vertex_data[VertexAttribute::UV0] = core::Buffer(3 * sizeof(VertexDataType<VertexAttribute::UV0>));
        auto p                            = vertex_data[VertexAttribute::UV0].Span<VertexDataType<VertexAttribute::UV0>>();
        p[0]                              = {0, 0};
        p[1]                              = {1, 0};
        p[2]                              = {1, 1};
    }
    {
        auto p = index_data.Span<IndexDataType<IndexType::UINT16>>();
        p[0]   = 0;
        p[1]   = 1;
        p[2]   = 2;
    }

    auto material = std::make_shared<Material>(
        MaterialParameters{
            {.name = "base_color", .value = Color{0.3f, 0.5f, 0.7f, 1.0f}},
        },
        std::pmr::vector<MaterialPass>{
            MaterialPass{.pass_contract = "GBuffer", .bindings = {{"base_color", Color::White()}}},
        },
        "BinaryCookedSceneMaterial");
    const auto mesh = std::make_shared<Mesh>(std::make_shared<VertexArray>(3, std::move(vertex_data)), std::make_shared<IndexArray>(IndexType::UINT16, std::move(index_data)), std::pmr::vector<Mesh::SubMesh>{{.index_count = 3, .material = material}});

    (void)scene->GetWorld().GetEntityManager().Create(asset::Transform(math::translate(vec3f{1.0f, 2.0f, 3.0f})), asset::RelationShip{}, asset::MetaInfo("cube"), asset::MeshComponent{mesh});
    (void)scene->GetWorld().GetEntityManager().Create(asset::Transform(mat4f::identity()), asset::RelationShip{}, asset::MetaInfo("camera"), asset::CameraComponent{});
    (void)scene->GetWorld().GetEntityManager().Create(asset::Transform(mat4f::identity()), asset::RelationShip{}, asset::MetaInfo("light"), asset::LightComponent{});
    scene->GetWorld().Update(job_system);

    const auto cooked = CookEditorScene(*scene, EditorCookOptions{.asset_root_path = "assets"});
    ASSERT_FALSE(cooked.Empty());
    EXPECT_TRUE(IsCookedBinary(cooked.Span<std::byte>()));

    AssetService asset_manager(file_io, job_system, "assets");
    const auto   loaded = asset_manager.LoadCookedScene(cooked, "assets");
    ASSERT_TRUE(loaded);
    ASSERT_EQ(loaded->GetWorld().GetEntityManager().GetEntities<asset::MeshComponent>().size(), 1);
    ASSERT_EQ(loaded->GetWorld().GetEntityManager().GetEntities<asset::CameraComponent>().size(), 1);
    ASSERT_EQ(loaded->GetWorld().GetEntityManager().GetEntities<asset::LightComponent>().size(), 1);

    const auto loaded_mesh = loaded->GetWorld().GetEntityManager().GetEntities<asset::MeshComponent>().front().Get<MeshComponent>().mesh;
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

    ASSERT_FALSE(loaded_mesh->sub_meshes.empty());
    ASSERT_TRUE(loaded_mesh->sub_meshes.front().material);
    EXPECT_EQ(loaded_mesh->sub_meshes.front().material->GetName(), "BinaryCookedSceneMaterial");

    const auto cooked_path = std::filesystem::path("build/test/cooked_scene_roundtrip_binary.hcscene");
    WriteTextFile(cooked_path, cooked.Str());

    const auto imported = asset_manager.ImportScene(cooked_path);
    ASSERT_TRUE(imported);
    ASSERT_EQ(imported->GetWorld().GetEntityManager().GetEntities<asset::MeshComponent>().size(), 1);
    const auto imported_material = imported->GetWorld().GetEntityManager().GetEntities<asset::MeshComponent>().front().Get<MeshComponent>().mesh->sub_meshes.front().material;
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
    const auto    texture = graph.AddNode("base_color_texture", "parameter", {}, {{"texture", ShaderValueType::Texture2D}});
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

TEST(MaterialGraphCompilerTest, SpecializesPbrForwardTemplate) {
    MaterialGraph graph;
    const auto    texture = graph.AddNode("base_color_texture", "parameter", {}, {{"texture", ShaderValueType::Texture2D}});
    const auto    uv      = graph.AddNode("uv0", "uv", {}, {{"uv", ShaderValueType::Float2}});
    const auto    sample  = graph.AddNode(
        "sample_base_color",
        "texture_sample",
        {{"texture", ShaderValueType::Texture2D}, {"uv", ShaderValueType::Float2}},
        {{"rgba", ShaderValueType::Color}});
    const auto metallic  = graph.AddNode("metallic", "parameter", {}, {{"value", ShaderValueType::Float}});
    const auto roughness = graph.AddConstant("roughness_default", 0.25f);
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

    const auto result = CompileMaterialGraph(graph, render::pbr::PbrForwardPassTemplate(), render::pbr::PbrMaterialDefaults(), {.name = "SnapshotMaterial"});
    ASSERT_TRUE(result);

    EXPECT_EQ(result.pass.pass_contract, render::pbr::kPbrForwardContract);
    ASSERT_TRUE(result.pass.pipeline);
    EXPECT_EQ(result.pass.pipeline->GetDesc().name, "SnapshotMaterialPipeline");
    EXPECT_EQ(result.pass.pipeline->GetDesc().depth_stencil_format, gfx::Format::D32_FLOAT);
    const auto shaders = result.pass.pipeline->GetShaders();
    ASSERT_EQ(shaders.size(), 2);
    EXPECT_EQ(shaders[0]->GetDesc().type, gfx::ShaderType::Vertex);
    EXPECT_EQ(shaders[1]->GetDesc().type, gfx::ShaderType::Pixel);
    EXPECT_EQ(shaders[1]->GetDesc().name, "SnapshotMaterial.PSMain");
    for (const auto& shader : shaders) {
        const auto& modules = shader->GetDesc().program.modules;
        ASSERT_EQ(modules.size(), 2);
        EXPECT_EQ(modules[0].source_code, result.slang);
        // The shell stays a file module, so cooked materials reference it instead of copying it.
        EXPECT_EQ(modules[1].name, "pbr_forward");
        EXPECT_TRUE(modules[1].source_code.empty());
        EXPECT_EQ(shader->GetDesc().specialization_types, (std::pmr::vector<std::pmr::string>{"Material"}));
    }

    ASSERT_EQ(result.pass.bindings.size(), 2);
    EXPECT_EQ(result.pass.bindings[0].name, "base_color_texture");
    EXPECT_EQ(result.pass.bindings[1].name, "metallic");

    EXPECT_NE(result.slang.find("import pbr_forward;"), std::pmr::string::npos);
    EXPECT_NE(result.slang.find("struct Material : IMaterial"), std::pmr::string::npos);
    EXPECT_NE(result.slang.find("hitagi::Texture base_color_texture;"), std::pmr::string::npos);
    EXPECT_NE(result.slang.find("float metallic;"), std::pmr::string::npos);
    EXPECT_NE(result.slang.find("if (hitagi::valid(material_data.base_color_texture.handle)) sample_base_color_rgba = material_data.base_color_texture.sample<float4>(context.sampler, input.uv);"), std::pmr::string::npos);
    EXPECT_NE(result.slang.find("surface.base_color = sample_base_color_rgba;"), std::pmr::string::npos);
    EXPECT_NE(result.slang.find("surface.metallic = material_data.metallic;"), std::pmr::string::npos);
    EXPECT_NE(result.slang.find("surface.roughness = 0.25f;"), std::pmr::string::npos);
    EXPECT_EQ(result.slang.find("DistributionGGX"), std::pmr::string::npos);
    ExpectCompilesForAvailableBackends(result.pass);
}

TEST(MaterialGraphCompilerTest, CompilesBaseNodeKindsAgainstAnyShell) {
    MaterialGraph graph;
    const auto    vertex_color = graph.AddNode("vertex_color", "vertex_color", {}, {{"color", ShaderValueType::Color}});
    const auto    tint         = graph.AddNode("tint", "parameter", {}, {{"value", ShaderValueType::Color}});
    const auto    mix          = graph.AddNode(
        "mix_vertex_color",
        "mix",
        {{"a", ShaderValueType::Color}, {"b", ShaderValueType::Color}, {"t", ShaderValueType::Float}},
        {{"value", ShaderValueType::Color}});
    const auto normal_texture = graph.AddNode("normal_texture", "parameter", {}, {{"texture", ShaderValueType::Texture2D}});
    const auto flat_normal    = graph.AddConstant("flat_normal", vec4f{0.5f, 0.5f, 1.0f, 1.0f});
    const auto sample         = graph.AddNode(
        "sample_normal",
        "texture_sample",
        {{"texture", ShaderValueType::Texture2D}, {"fallback", ShaderValueType::Float4}},
        {{"rgba", ShaderValueType::Float4}});
    const auto normal_map = graph.AddNode("decode_normal", "normal_map", {{"sample", ShaderValueType::Float4}}, {{"normal", ShaderValueType::Float3}});
    const auto channels   = graph.AddNode("tint_channels", "swizzle", {{"in", ShaderValueType::Color}}, {{"a", ShaderValueType::Float}});
    const auto surface    = graph.AddNode(
        "surface",
        "surface_output",
        {{"albedo", ShaderValueType::Color}, {"normal", ShaderValueType::Float3}},
        {{"out", ShaderValueType::Surface}});

    graph.AddEdge(vertex_color, "color", mix, "a");
    graph.AddEdge(tint, "value", mix, "b");
    graph.AddEdge(tint, "value", channels, "in");
    graph.AddEdge(channels, "a", mix, "t");
    graph.AddEdge(normal_texture, "texture", sample, "texture");
    graph.AddEdge(flat_normal, "value", sample, "fallback");
    graph.AddEdge(sample, "rgba", normal_map, "sample");
    graph.AddEdge(mix, "value", surface, "albedo");
    graph.AddEdge(normal_map, "normal", surface, "normal");
    graph.AddTerminal("surface", surface, "out", ShaderValueType::Surface);

    const MaterialParameters defaults{
        {.name = "tint", .value = Color::White()},
        {.name = "normal_texture", .value = std::shared_ptr<Texture>{}},
    };
    const auto result = CompileMaterialGraph(graph, TestPassTemplate(), defaults, {.name = "BaseNodeMaterial"});
    ASSERT_TRUE(result) << (result.validation.errors.empty() ? "" : result.validation.errors.front().message);
    EXPECT_EQ(result.pass.pass_contract, "Test");
    EXPECT_NE(result.slang.find("lerp(input.color, material_data.tint, (material_data.tint).a)"), std::pmr::string::npos);
    EXPECT_NE(result.slang.find("float4 sample_normal_rgba = float4(0.5f, 0.5f, 1.0f, 1.0f);"), std::pmr::string::npos);
    EXPECT_NE(result.slang.find("surface.normal = normalize(sample_normal_rgba.xyz * 2.0f - 1.0f);"), std::pmr::string::npos);
    ExpectCompilesForAvailableBackends(result.pass);
}

TEST(MaterialGraphCompilerTest, ReportsUnsupportedNodesAndChannelMismatches) {
    const auto pass_template = TestPassTemplate();
    {
        MaterialGraph graph;
        const auto    value   = graph.AddNode("value", "noise", {}, {{"value", ShaderValueType::Float3}});
        const auto    surface = graph.AddNode("surface", "surface_output", {{"normal", ShaderValueType::Float3}}, {{"out", ShaderValueType::Surface}});
        graph.AddEdge(value, "value", surface, "normal");
        graph.AddTerminal("surface", surface, "out", ShaderValueType::Surface);
        EXPECT_TRUE(HasGraphError(CompileMaterialGraph(graph, pass_template, {}).validation, MaterialGraphValidationErrorCode::UnsupportedNode));
        EXPECT_TRUE(HasGraphError(CompileMaterialGraph(graph, pass_template, {}, {.terminal = "missing"}).validation, MaterialGraphValidationErrorCode::MissingTerminal));
    }
    {
        MaterialGraph graph;
        const auto    color    = graph.AddConstant("color", Color::White());
        const auto    channels = graph.AddNode("channels", "swizzle", {{"in", ShaderValueType::Color}}, {{"rg", ShaderValueType::Float3}});
        const auto    surface  = graph.AddNode("surface", "surface_output", {{"normal", ShaderValueType::Float3}}, {{"out", ShaderValueType::Surface}});
        graph.AddEdge(color, "value", channels, "in");
        graph.AddEdge(channels, "rg", surface, "normal");
        graph.AddTerminal("surface", surface, "out", ShaderValueType::Surface);
        EXPECT_TRUE(HasGraphError(CompileMaterialGraph(graph, pass_template, {}).validation, MaterialGraphValidationErrorCode::TypeMismatch));
    }
}

TEST_F(EditorMaterialCookTest, VertexLayoutIsCookedWithoutMutatingPipelineDefinition) {
    auto shader   = std::make_shared<Shader>(gfx::ShaderDesc{
        .type = gfx::ShaderType::Vertex, .entry = "VSMain", .program = {.modules = {{.source_code = "float4 VSMain(float3 p : POSITION) : SV_Position { return float4(p, 1); }"}}}});
    auto pipeline = std::make_shared<asset::RenderPipeline>(gfx::RenderPipelineDesc{}, std::pmr::vector{shader});
    auto material = std::make_shared<Material>(MaterialParameters{}, std::pmr::vector<MaterialPass>{{.pass_contract = "Test", .pipeline = pipeline}});
    auto clone    = material->Clone("clone");
    EXPECT_EQ(clone->FindPass("Test")->pipeline, pipeline);
    auto bytes = CookEditorMaterial({.material = material});
    EXPECT_TRUE(pipeline->GetDesc().vertex_input_layout.empty());
    AssetService manager(file_io, job_system, "assets");
    auto         loaded = manager.LoadCookedMaterial(bytes, "assets");
    const auto&  layout = loaded->FindPass("Test")->pipeline->GetDesc().vertex_input_layout;
    ASSERT_EQ(layout.size(), 1);
    EXPECT_EQ(layout[0].semantic, "POSITION0");
    EXPECT_EQ(layout[0].format, gfx::Format::R32G32B32_FLOAT);
    EXPECT_EQ(layout[0].stride, 12u);
    auto desc                                    = pipeline->GetDesc();
    desc.vertex_input_layout                     = {{.semantic = "POSITION0", .format = gfx::Format::R32G32B32_FLOAT, .binding = 3, .offset = 8, .stride = 32, .per_instance = true, .location = 2}};
    desc.assembly_state.restart_enable           = true;
    desc.rasterization_state.fill_mode           = gfx::FillMode::Wireframe;
    desc.rasterization_state.depth_bias          = 2.5f;
    desc.depth_stencil_state.stencil_test_enable = true;
    desc.depth_stencil_state.front.reference     = 17;
    desc.depth_stencil_state.back.fail_op        = gfx::StencilOp::Replace;
    desc.blend_state.blend_enable                = true;
    desc.blend_state.src_color_blend_factor      = gfx::BlendFactor::SrcAlpha;
    desc.blend_state.blend_constants             = vec4f(0.25f);
    auto        explicit_pipeline                = std::make_shared<asset::RenderPipeline>(desc, std::pmr::vector{shader});
    auto        explicit_material                = std::make_shared<Material>(MaterialParameters{}, std::pmr::vector<MaterialPass>{{.pass_contract = "Test", .pipeline = explicit_pipeline}});
    auto        restored                         = manager.LoadCookedMaterial(CookEditorMaterial({.material = explicit_material}), "assets");
    const auto& attr                             = restored->FindPass("Test")->pipeline->GetDesc().vertex_input_layout[0];
    EXPECT_EQ(attr.binding, 3u);
    EXPECT_EQ(attr.offset, 8u);
    EXPECT_EQ(attr.stride, 32u);
    EXPECT_TRUE(attr.per_instance);
    EXPECT_EQ(attr.location, 2u);
    const auto& state = restored->FindPass("Test")->pipeline->GetDesc();
    EXPECT_TRUE(state.assembly_state.restart_enable);
    EXPECT_EQ(state.rasterization_state.fill_mode, gfx::FillMode::Wireframe);
    EXPECT_FLOAT_EQ(state.rasterization_state.depth_bias, 2.5f);
    EXPECT_TRUE(state.depth_stencil_state.stencil_test_enable);
    EXPECT_EQ(state.depth_stencil_state.front.reference, 17u);
    EXPECT_EQ(state.depth_stencil_state.back.fail_op, gfx::StencilOp::Replace);
    EXPECT_TRUE(state.blend_state.blend_enable);
    EXPECT_EQ(state.blend_state.src_color_blend_factor, gfx::BlendFactor::SrcAlpha);
    EXPECT_EQ(state.blend_state.blend_constants, vec4f(0.25f));
}
