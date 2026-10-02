#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

import editor;
import engine;
import magic_enum;
import pbr_demo_game;

namespace {

auto MakeGeneratedShaderName(const hitagi::asset::Material& material) -> std::pmr::string {
    auto source = std::string_view(material.GetName());
    if (source.empty()) source = "PbrDemoMaterial";

    std::pmr::string result = "PbrDemo_";
    result.reserve(result.size() + source.size());
    for (const auto ch : source) {
        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_')
            result.push_back(ch);
        else
            result.push_back('_');
    }
    return result;
}

auto SanitizeGeneratedPathComponent(std::string_view source) -> std::pmr::string {
    std::pmr::string result;
    result.reserve(source.size());
    for (const auto ch : source) {
        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_')
            result.push_back(ch);
        else
            result.push_back('_');
    }
    if (result.empty()) result = "asset";
    return result;
}

auto StableStringHash(std::string_view value) noexcept -> std::uint64_t {
    auto hash = 14695981039346656037ull;
    for (const auto ch : value) {
        hash = (hash ^ static_cast<std::uint64_t>(static_cast<unsigned char>(ch))) * 1099511628211ull;
    }
    return hash;
}

auto LowerPbrDemoToIR(const game::pbr_demo::PbrMaterialDesc& desc) -> hitagi::MaterialGraph {
    hitagi::MaterialGraph graph;
    const auto            base_color   = graph.AddNode("base_color", "parameter", {}, {{"value", hitagi::ShaderValueType::Color}});
    const auto            metallic     = graph.AddNode("metallic", "parameter", {}, {{"value", hitagi::ShaderValueType::Float}});
    const auto            roughness    = graph.AddNode("roughness", "parameter", {}, {{"value", hitagi::ShaderValueType::Float}});
    const auto            occlusion    = graph.AddNode("occlusion", "parameter", {}, {{"value", hitagi::ShaderValueType::Float}});
    const auto            emissive     = graph.AddNode("emissive_color", "parameter", {}, {{"value", hitagi::ShaderValueType::Color}});
    const auto            opacity      = graph.AddNode("opacity", "parameter", {}, {{"value", hitagi::ShaderValueType::Float}});
    const auto            alpha_cutout = graph.AddNode("alpha_cutout", "parameter", {}, {{"value", hitagi::ShaderValueType::UInt32}});
    const auto            alpha_cutoff = graph.AddNode("alpha_cutoff", "parameter", {}, {{"value", hitagi::ShaderValueType::Float}});
    const auto            uv           = graph.AddNode("uv0", "primvar", {}, {{"uv", hitagi::ShaderValueType::Float2}});
    const auto            surface      = graph.AddNode(
        "pbr_surface",
        "pbr_surface",
        {
            {"base_color", hitagi::ShaderValueType::Color},
            {"metallic", hitagi::ShaderValueType::Float},
            {"roughness", hitagi::ShaderValueType::Float},
            {"metallic_roughness", hitagi::ShaderValueType::Color},
            {"normal", hitagi::ShaderValueType::Float3},
            {"occlusion", hitagi::ShaderValueType::Float},
            {"emissive", hitagi::ShaderValueType::Color},
            {"opacity", hitagi::ShaderValueType::Float},
            {"alpha_cutout", hitagi::ShaderValueType::UInt32},
            {"alpha_cutoff", hitagi::ShaderValueType::Float},
        },
        {{"out", hitagi::ShaderValueType::Surface}});

    const auto add_texture_sample = [&](std::string_view texture_name, hitagi::ShaderValueType sample_type, std::string_view surface_input) {
        const auto texture = graph.AddNode(texture_name, "parameter", {}, {{"texture", hitagi::ShaderValueType::Texture2D}});
        const auto sample  = graph.AddNode(std::format("sample_{}", texture_name), "texture_sample", {{"texture", hitagi::ShaderValueType::Texture2D}, {"uv", hitagi::ShaderValueType::Float2}}, {{"value", sample_type}});
        graph.AddEdge(texture, "texture", sample, "texture");
        graph.AddEdge(uv, "uv", sample, "uv");
        graph.AddEdge(sample, "value", surface, surface_input);
    };

    if (desc.base_color_texture)
        add_texture_sample("base_color_texture", hitagi::ShaderValueType::Color, "base_color");
    else
        graph.AddEdge(base_color, "value", surface, "base_color");
    graph.AddEdge(metallic, "value", surface, "metallic");
    graph.AddEdge(roughness, "value", surface, "roughness");
    if (desc.metallic_roughness_texture) add_texture_sample("metallic_roughness_texture", hitagi::ShaderValueType::Color, "metallic_roughness");
    if (desc.normal_texture) add_texture_sample("normal_texture", hitagi::ShaderValueType::Float3, "normal");
    if (desc.occlusion_texture)
        add_texture_sample("occlusion_texture", hitagi::ShaderValueType::Float, "occlusion");
    else
        graph.AddEdge(occlusion, "value", surface, "occlusion");
    if (desc.emissive_texture)
        add_texture_sample("emissive_texture", hitagi::ShaderValueType::Color, "emissive");
    else
        graph.AddEdge(emissive, "value", surface, "emissive");
    graph.AddEdge(opacity, "value", surface, "opacity");
    graph.AddEdge(alpha_cutout, "value", surface, "alpha_cutout");
    graph.AddEdge(alpha_cutoff, "value", surface, "alpha_cutoff");
    graph.AddTerminal("surface", surface, "out", hitagi::ShaderValueType::Surface);
    return graph;
}

void WriteTextArtifact(const std::filesystem::path& path, std::string_view content) {
    if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error(std::format("failed to create generated artifact '{}'", path.string()));
    }
    output.write(content.data(), static_cast<std::streamsize>(content.size()));
    if (!output) {
        throw std::runtime_error(std::format("failed to write generated artifact '{}'", path.string()));
    }
}

void WriteBinaryArtifact(const std::filesystem::path& path, std::span<const std::byte> content) {
    if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error(std::format("failed to create generated artifact '{}'", path.string()));
    }
    output.write(reinterpret_cast<const char*>(content.data()), static_cast<std::streamsize>(content.size()));
    if (!output) {
        throw std::runtime_error(std::format("failed to write generated artifact '{}'", path.string()));
    }
}

auto AbsoluteNormalized(const std::filesystem::path& path) -> std::filesystem::path {
    std::error_code error;
    auto            absolute = std::filesystem::absolute(path, error);
    if (error) return path.lexically_normal();
    return absolute.lexically_normal();
}

auto IsUnderRoot(const std::filesystem::path& path, const std::filesystem::path& root) -> bool {
    if (root.empty()) return true;

    std::error_code error;
    const auto      relative = std::filesystem::relative(AbsoluteNormalized(path), AbsoluteNormalized(root), error);
    return !error &&
           !relative.empty() &&
           *relative.begin() != std::filesystem::path("..") &&
           relative != ".";
}

auto ExistingCookableTexturePath(
    const std::filesystem::path& texture_path,
    const std::filesystem::path& asset_root_path) -> std::optional<std::filesystem::path> {
    if (texture_path.empty()) return std::nullopt;

    std::array candidates{
        texture_path,
        texture_path.is_absolute() || asset_root_path.empty() ? texture_path : asset_root_path / texture_path,
    };

    for (const auto& candidate : candidates) {
        std::error_code error;
        if (!std::filesystem::is_regular_file(candidate, error)) continue;
        if (!IsUnderRoot(candidate, asset_root_path)) continue;
        return candidate;
    }
    return std::nullopt;
}

auto MakeGeneratedTexturePath(
    const std::filesystem::path&            asset_root_path,
    const hitagi::asset::Material&          material,
    const hitagi::asset::MaterialParameter& parameter,
    const hitagi::asset::Texture&           texture) -> std::filesystem::path {
    const auto source = std::format(
        "{}|{}|{}",
        texture.GetName(),
        texture.GetPath().generic_string(),
        parameter.name);
    const auto file_name = std::format(
        "{}_{}_{:016x}.png",
        SanitizeGeneratedPathComponent(material.GetName()),
        SanitizeGeneratedPathComponent(parameter.name),
        StableStringHash(source));
    return asset_root_path / "generated" / "textures" / file_name;
}

void ExportTextureForCook(
    const hitagi::asset::Material&                 material,
    const hitagi::asset::MaterialParameter&        parameter,
    const std::shared_ptr<hitagi::asset::Texture>& texture,
    const std::filesystem::path&                   asset_root_path) {
    if (!texture) return;
    if (ExistingCookableTexturePath(texture->GetPath(), asset_root_path)) return;

    const auto pixels = texture->GetData();
    if (pixels.empty() || texture->Width() == 0 || texture->Height() == 0) {
        throw std::runtime_error(std::format(
            "texture '{}' used by material '{}' can not be cooked because it has no decoded image data",
            texture->GetName(),
            material.GetName()));
    }

    const auto output_path = MakeGeneratedTexturePath(asset_root_path, material, parameter, *texture);
    const auto png_data    = hitagi::asset::PngEncoder{}.Encode(*texture);
    if (png_data.Empty()) {
        throw std::runtime_error(std::format("failed to encode generated texture '{}'", output_path.string()));
    }

    WriteBinaryArtifact(output_path, png_data.Span<const std::byte>());
    if (!texture->SetPath(output_path)) {
        throw std::runtime_error(std::format("failed to assign generated texture path '{}'", output_path.string()));
    }
}

void ExportMaterialTexturesForCook(hitagi::asset::Material& material, const std::filesystem::path& asset_root_path) {
    for (const auto& parameter : material.GetParameters()) {
        if (const auto* texture = std::get_if<std::shared_ptr<hitagi::asset::Texture>>(&parameter.value)) {
            ExportTextureForCook(material, parameter, *texture, asset_root_path);
        }
    }
}

auto PassToJson(const hitagi::asset::MaterialPass& pass) -> nlohmann::json {
    auto bindings = nlohmann::json::array();
    for (const auto& binding : pass.bindings) bindings.emplace_back(std::string(binding));

    return nlohmann::json{
        {"contract", std::string(pass.pass_contract)},
        {"bindings", std::move(bindings)},
    };
}

auto PipelineToJson(const hitagi::gfx::RenderPipelineDesc& pipeline) -> nlohmann::json {
    return nlohmann::json{
        {"name", std::string(pipeline.name)},
        {"primitive", std::string(magic_enum::enum_name(pipeline.assembly_state.primitive))},
        {"cullMode", std::string(magic_enum::enum_name(pipeline.rasterization_state.cull_mode))},
        {"frontCounterClockwise", pipeline.rasterization_state.front_counter_clockwise},
        {"depthTest", pipeline.depth_stencil_state.depth_test_enable},
        {"depthWrite", pipeline.depth_stencil_state.depth_write_enable},
        {"depthCompare", std::string(magic_enum::enum_name(pipeline.depth_stencil_state.depth_compare_op))},
        {"renderFormat", std::string(magic_enum::enum_name(pipeline.render_format))},
        {"depthStencilFormat", std::string(magic_enum::enum_name(pipeline.depth_stencil_format))},
    };
}

auto SourceInfoToJson(const hitagi::MaterialSourceInfo& source) -> nlohmann::json {
    auto texture_paths = nlohmann::json::array();
    for (const auto& path : source.source_texture_paths) texture_paths.emplace_back(path.generic_string());

    return nlohmann::json{
        {"type", std::string(magic_enum::enum_name(source.type))},
        {"asset", source.source_asset.generic_string()},
        {"materialPath", std::string(source.source_material_path)},
        {"shaderId", std::string(source.source_shader_id)},
        {"texturePaths", std::move(texture_paths)},
        {"unsupportedInputs", source.unsupported_inputs},
        {"unsupportedNodes", source.unsupported_nodes},
    };
}

auto GeneratedMaterialPreviewToJson(
    const hitagi::EditorMaterial&             editor_material,
    const hitagi::MaterialGraphCompileResult& result) -> nlohmann::json {
    const auto& material     = *editor_material.material;
    auto        shader_paths = nlohmann::json::array();
    if (result.pass.pipeline) {
        for (const auto& shader_asset : result.pass.pipeline->GetShaders()) {
            if (!shader_asset) continue;
            const auto& shader = shader_asset->GetDesc();
            shader_paths.emplace_back(nlohmann::json{
                {"name", std::string(shader.name)},
                {"type", std::string(magic_enum::enum_name(shader.type))},
                {"entry", std::string(shader.entry)},
                {"path", shader.path.generic_string()},
            });
        }
    }

    return nlohmann::json{
        {"schema", "Hitagi.Editor.GeneratedMaterialPreview"},
        {"version", 1},
        {"material", std::string(material.GetName())},
        {"source", SourceInfoToJson(editor_material.source_info)},
        {"pass", PassToJson(result.pass)},
        {"pipeline", PipelineToJson(result.pipeline_desc)},
        {"shaders", std::move(shader_paths)},
        {"graphHash", result.graph_hash},
        {"sourceHash", result.source_hash},
        {"cacheKey", result.cache_key},
    };
}

void WriteGeneratedMaterialArtifacts(
    const hitagi::EditorMaterial&             material,
    const hitagi::MaterialGraphCompileResult& result) {
    WriteTextArtifact(result.shader.path, result.hlsl);

    auto preview_path = result.shader.path;
    preview_path.replace_extension(".material.json");
    WriteTextArtifact(preview_path, GeneratedMaterialPreviewToJson(material, result).dump(2));
}

auto ReadUsdColor(const hitagi::asset::Material& material, std::string_view name) -> std::optional<hitagi::math::Color> {
    if (const auto value = material.GetParameter<hitagi::math::Color>(name)) return *value;
    if (const auto value = material.GetParameter<hitagi::math::vec4f>(name)) return hitagi::math::Color{(*value)[0], (*value)[1], (*value)[2], (*value)[3]};
    if (const auto value = material.GetParameter<hitagi::math::vec3f>(name)) return hitagi::math::Color{(*value)[0], (*value)[1], (*value)[2], 1.0f};
    return std::nullopt;
}

auto ReadUsdTexture(const hitagi::asset::Material& material, std::string_view name) -> std::shared_ptr<hitagi::asset::Texture> {
    const auto texture = material.GetParameter<std::shared_ptr<hitagi::asset::Texture>>(name);
    return texture ? *texture : nullptr;
}

auto CopyMaterialParameters(const hitagi::asset::Material& material) -> hitagi::asset::MaterialParameters {
    hitagi::asset::MaterialParameters parameters;
    parameters.reserve(material.GetParameters().size());
    for (const auto& parameter : material.GetParameters()) parameters.emplace_back(parameter);
    return parameters;
}

auto CopyMaterialPasses(const hitagi::asset::Material& material) -> std::pmr::vector<hitagi::asset::MaterialPass> {
    std::pmr::vector<hitagi::asset::MaterialPass> passes;
    passes.reserve(material.GetPasses().size());
    for (const auto& pass : material.GetPasses()) passes.emplace_back(pass);
    return passes;
}

void ReplaceMaterialPass(hitagi::asset::Material& material, hitagi::asset::MaterialPass pass) {
    auto       passes = CopyMaterialPasses(material);
    const auto iter   = std::ranges::find_if(passes, [&](const auto& current) {
        return current.pass_contract == pass.pass_contract;
    });
    if (iter != passes.end())
        *iter = std::move(pass);
    else
        passes.emplace_back(std::move(pass));

    material = hitagi::asset::Material(
        CopyMaterialParameters(material),
        std::move(passes),
        material.GetName());
}

void ApplyPbrDemoUsdPreviewSurfaceParameters(hitagi::EditorMaterial& editor_material) {
    if (editor_material.source_info.source_shader_id != "UsdPreviewSurface") return;
    auto& material = *editor_material.material;

    game::pbr_demo::PbrMaterialDesc desc;
    if (const auto base_color = ReadUsdColor(material, "diffuseColor")) desc.base_color = *base_color;
    if (const auto metallic = material.GetParameter<float>("metallic")) desc.metallic = std::clamp(*metallic, 0.0f, 1.0f);
    if (const auto roughness = material.GetParameter<float>("roughness")) desc.roughness = std::clamp(*roughness, 0.04f, 1.0f);
    if (const auto occlusion = material.GetParameter<float>("occlusion")) desc.occlusion = std::clamp(*occlusion, 0.0f, 1.0f);
    if (const auto emissive = ReadUsdColor(material, "emissiveColor")) desc.emissive_color = *emissive;
    if (const auto opacity = material.GetParameter<float>("opacity")) {
        desc.opacity      = std::clamp(*opacity, 0.0f, 1.0f);
        desc.base_color.a = desc.opacity;
    }

    desc.base_color_texture = ReadUsdTexture(material, "diffuseColor");
    desc.normal_texture     = ReadUsdTexture(material, "normal");
    desc.occlusion_texture  = ReadUsdTexture(material, "occlusion");
    desc.emissive_texture   = ReadUsdTexture(material, "emissiveColor");
    if (auto roughness_texture = ReadUsdTexture(material, "roughness")) {
        desc.metallic_roughness_texture = std::move(roughness_texture);
    } else {
        desc.metallic_roughness_texture = ReadUsdTexture(material, "metallic");
    }

    // USD inputs can be textures where the runtime PBR model uses scalars.
    // Lowering defines a new parameter set rather than assigning across types.
    auto parameters = CopyMaterialParameters(material);
    for (const auto& parameter : game::pbr_demo::CreatePbrMaterialParameters(desc)) {
        const auto iter = std::ranges::find_if(parameters, [&](const auto& current) { return current.name == parameter.name; });
        if (iter == parameters.end())
            parameters.emplace_back(parameter);
        else
            *iter = parameter;
    }
    material = hitagi::asset::Material(std::move(parameters), CopyMaterialPasses(material), material.GetName());
}

auto ReadPbrDemoMaterialDesc(const hitagi::EditorMaterial& editor_material) -> std::optional<game::pbr_demo::PbrMaterialDesc> {
    if (editor_material.source_info.source_shader_id != "UsdPreviewSurface") return std::nullopt;
    const auto& material = *editor_material.material;

    game::pbr_demo::PbrMaterialDesc desc;
    const auto                      base_color   = material.GetParameter<hitagi::math::Color>("base_color");
    const auto                      metallic     = material.GetParameter<float>("metallic");
    const auto                      roughness    = material.GetParameter<float>("roughness");
    const auto                      occlusion    = material.GetParameter<float>("occlusion");
    const auto                      emissive     = material.GetParameter<hitagi::math::Color>("emissive_color");
    const auto                      opacity      = material.GetParameter<float>("opacity");
    const auto                      alpha_cutout = material.GetParameter<std::uint32_t>("alpha_cutout");
    const auto                      alpha_cutoff = material.GetParameter<float>("alpha_cutoff");
    if (!base_color || !metallic || !roughness || !occlusion || !emissive || !opacity || !alpha_cutout || !alpha_cutoff) return std::nullopt;

    desc.base_color     = *base_color;
    desc.metallic       = *metallic;
    desc.roughness      = *roughness;
    desc.occlusion      = *occlusion;
    desc.emissive_color = *emissive;
    desc.opacity        = *opacity;
    desc.alpha_cutout   = *alpha_cutout != 0;
    desc.alpha_cutoff   = *alpha_cutoff;

    const auto read_texture = [&](std::string_view name, std::shared_ptr<hitagi::asset::Texture>& texture) {
        const auto parameter = material.GetParameter<std::shared_ptr<hitagi::asset::Texture>>(name);
        if (!parameter) return false;
        texture = *parameter;
        return true;
    };
    if (!read_texture("base_color_texture", desc.base_color_texture) ||
        !read_texture("metallic_roughness_texture", desc.metallic_roughness_texture) ||
        !read_texture("normal_texture", desc.normal_texture) ||
        !read_texture("occlusion_texture", desc.occlusion_texture) ||
        !read_texture("emissive_texture", desc.emissive_texture)) {
        return std::nullopt;
    }

    return desc;
}

auto CompilePbrDemoMaterialPass(hitagi::EditorMaterial& editor_material) -> std::optional<hitagi::asset::MaterialPass> {
    auto&      material = *editor_material.material;
    const auto pbr_desc = ReadPbrDemoMaterialDesc(editor_material);
    if (!pbr_desc) return std::nullopt;

    auto graph  = LowerPbrDemoToIR(*pbr_desc);
    auto result = hitagi::MaterialGraphCompiler{}.Compile(graph, hitagi::MaterialGraphCompileOptions{
                                                                     .shader_name = MakeGeneratedShaderName(material),
                                                                     .entry_point = "PSMain",
                                                                 });
    if (!result) return std::nullopt;
    result.pass.pass_contract = game::pbr_demo::kPbrForwardContract;
    WriteGeneratedMaterialArtifacts(editor_material, result);
    return std::move(result.pass);
}

bool CompilePbrDemoMaterial(hitagi::EditorMaterial& editor_material) {
    auto& material = *editor_material.material;
    ApplyPbrDemoUsdPreviewSurfaceParameters(editor_material);
    auto pass = CompilePbrDemoMaterialPass(editor_material);
    if (!pass) return false;
    ReplaceMaterialPass(material, std::move(*pass));
    return true;
}

auto MakePbrDemoMaterialProcessor(std::filesystem::path asset_root_path) {
    return [asset_root_path = std::move(asset_root_path)](hitagi::EditorMaterial& material) {
        ExportMaterialTexturesForCook(*material.material, asset_root_path);
        if (!CompilePbrDemoMaterial(material)) {
            throw std::runtime_error(std::format("PBR demo material compilation failed for '{}'", material.material->GetName()));
        }
    };
}

void CookPbrDemoScene(
    hitagi::asset::Scene&            scene,
    const std::filesystem::path&     output_path,
    const std::filesystem::path&     asset_root_path,
    const hitagi::EditorCookContext& cook_context) {
    game::pbr_demo::ConfigurePbrDemoScene(scene);
    const auto cooked = hitagi::CookEditorScene(scene, hitagi::EditorCookOptions{
                                                                 .asset_root_path = asset_root_path,
                                                                 .context         = std::addressof(cook_context),
                                                             });
    WriteBinaryArtifact(output_path, cooked.Span<std::byte>());
}

auto IsCookCommand(int argc, char** argv) -> bool {
    for (int i = 1; i < argc; ++i) {
        if (std::string_view(argv[i]) == "--cook") return true;
    }
    return false;
}

auto CookInputPath(int argc, char** argv) -> std::filesystem::path {
    for (int i = 1; i < argc; ++i) {
        if (std::string_view(argv[i]) == "--cook") {
            if (i + 1 < argc && !std::string_view(argv[i + 1]).starts_with("--")) {
                return argv[i + 1];
            }
            break;
        }
    }
    return game::pbr_demo::FindDefaultSourceScenePath();
}

auto CookOutputPath(int argc, char** argv) -> std::filesystem::path {
    for (int i = 1; i < argc; ++i) {
        if (std::string_view(argv[i]) == "--out" && i + 1 < argc) {
            return argv[i + 1];
        }
    }
    for (int i = 1; i < argc; ++i) {
        if (std::string_view(argv[i]) == "--cook" && i + 2 < argc && !std::string_view(argv[i + 2]).starts_with("--")) {
            return argv[i + 2];
        }
    }
    return game::pbr_demo::DefaultCookedScenePath();
}

auto RunCookCommand(int argc, char** argv) -> int {
    // Headless composition root for the cook tool. Declaration order is dependency
    // order, so the language destroys every consumer before the services it uses.
    hitagi::core::MemoryManager memory_manager;
    hitagi::core::FileIOManager file_io;
    hitagi::core::JobSystem     job_system;
    hitagi::asset::AssetManager asset_manager(file_io, job_system, "assets");

    const auto input  = CookInputPath(argc, argv);
    const auto output = CookOutputPath(argc, argv);

    try {
        hitagi::EditorCookContext cook_context;
        auto                      scene = hitagi::ImportEditorScene(
            asset_manager,
            input,
            input.parent_path(),
            [&asset_manager](std::string_view name) { return asset_manager.GetMaterial(name); },
            nullptr,
            MakePbrDemoMaterialProcessor("assets"),
            std::addressof(cook_context));
        if (!scene) {
            spdlog::error("Failed to import scene for cook: {}", input.string());
            return 1;
        }
        CookPbrDemoScene(*scene, output, "assets", cook_context);
        spdlog::info("Cooked PBR demo scene: {} -> {}", input.string(), output.string());
        return 0;
    } catch (const std::exception& error) {
        spdlog::error("Cook failed: {}", error.what());
        return 1;
    }
}

}  // namespace

auto main(int argc, char** argv) -> int {
#ifdef HITAGI_DEBUG
    spdlog::set_level(spdlog::level::debug);
#endif

    if (IsCookCommand(argc, argv)) {
        return RunCookCommand(argc, argv);
    }

    auto config  = hitagi::LoadEditorAppConfig();
    config.title = "Hitagi PBR Demo Editor";

    auto options = hitagi::ParseEditorLaunchOptions(argc, argv);
    if (!options.open_scene) {
        options.open_scene = game::pbr_demo::FindDefaultSourceScenePath();
    }
    options.material_processor = MakePbrDemoMaterialProcessor(config.asset_root_path);

    hitagi::Engine engine(std::move(config));
    engine.SetRenderer(std::make_unique<game::pbr_demo::PbrDemoRenderer>(engine.RenderRuntime().GetRenderGraph().GetDevice()));
    engine.AddSubModule(std::make_unique<hitagi::Editor>(engine, std::move(options)));

    while (!engine.App().IsQuit()) {
        engine.Tick();
    }

    hitagi::SaveEditorAppConfig(engine.App().GetConfig());
    return 0;
}
