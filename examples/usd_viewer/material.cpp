module;
#include <cstddef>
module anime_viewer;
import std;

namespace game::anime_viewer {

auto AnimeMaterialBindings() -> std::pmr::vector<asset::MaterialBinding> {
    return {
        {"base_color", math::vec4f{1, 1, 1, 1}},
        {"alpha_cutoff", 0.0f},
        {"textured", std::uint32_t{0}},
        {"kind", std::uint32_t{0}},
        {"controlled", std::uint32_t{0}},
        {"head", math::vec4f{0, 0, 0, 1}},
        {"head_right", math::vec4f{1, 0, 0, 0}},
        {"head_up", math::vec4f{0, 1, 0, 0}},
        {"base_color_texture", std::shared_ptr<asset::Texture>{}},
        {"control_texture", std::shared_ptr<asset::Texture>{}},
    };
}

auto AnimeShaderProgram() -> gfx::ShaderProgramDesc {
    std::pmr::string declarations = "import bindless;\n";
    const auto material_bindings = AnimeMaterialBindings();
    const auto material_fields = asset::MaterialDataFields(material_bindings);
    declarations += gfx::GenerateShaderStruct("AnimeMaterialData", material_fields,
        material_fields.back().offset + material_fields.back().size);

#define ANIME_FIELD(Type, field) gfx::MakeShaderField<decltype(Type::field)>(#field, offsetof(Type, field))
#define ANIME_RESOURCE(Type, field, kind) gfx::MakeShaderResourceField(#field, offsetof(Type, field), gfx::BindlessHandleType::kind)
    const std::array frame_fields{
        ANIME_FIELD(AnimeFrameData, inverse_vp), ANIME_FIELD(AnimeFrameData, shadow_mvp),
        ANIME_FIELD(AnimeFrameData, eye), ANIME_FIELD(AnimeFrameData, light),
        ANIME_FIELD(AnimeFrameData, shadow_settings), ANIME_FIELD(AnimeFrameData, options),
    };
    declarations += gfx::GenerateShaderStruct("AnimeFrameData", frame_fields, sizeof(AnimeFrameData));
    const std::array instance_fields{
        ANIME_FIELD(AnimeInstanceData, mvp), ANIME_FIELD(AnimeInstanceData, model),
        ANIME_FIELD(AnimeInstanceData, normal_matrix),
    };
    declarations += gfx::GenerateShaderStruct("AnimeInstanceData", instance_fields, sizeof(AnimeInstanceData));
    const std::array pass_fields{
        ANIME_FIELD(AnimePassData, mode), ANIME_RESOURCE(AnimePassData, shadow, Texture),
        ANIME_RESOURCE(AnimePassData, hair_shadow, Texture), ANIME_RESOURCE(AnimePassData, sampler, Sampler),
    };
    declarations += gfx::GenerateShaderStruct("AnimePassData", pass_fields, sizeof(AnimePassData));
    const std::array draw_fields{
        ANIME_RESOURCE(AnimeDrawBindings, frame, Buffer), ANIME_RESOURCE(AnimeDrawBindings, instance, Buffer),
        ANIME_RESOURCE(AnimeDrawBindings, material_data, Buffer), ANIME_RESOURCE(AnimeDrawBindings, pass_data, Buffer),
        ANIME_FIELD(AnimeDrawBindings, instance_index), ANIME_FIELD(AnimeDrawBindings, instance_stride),
        ANIME_FIELD(AnimeDrawBindings, material_offset),
    };
    declarations += gfx::GenerateShaderStruct("AnimeDrawBindings", draw_fields, sizeof(AnimeDrawBindings));
    const std::array fullscreen_fields{
        ANIME_RESOURCE(AnimeFullscreenData, frame, Buffer), ANIME_FIELD(AnimeFullscreenData, settings),
        ANIME_FIELD(AnimeFullscreenData, scale), ANIME_RESOURCE(AnimeFullscreenData, texture0, Texture),
        ANIME_RESOURCE(AnimeFullscreenData, texture1, Texture), ANIME_RESOURCE(AnimeFullscreenData, texture2, Texture),
        ANIME_RESOURCE(AnimeFullscreenData, texture3, Texture), ANIME_RESOURCE(AnimeFullscreenData, sampler, Sampler),
    };
    declarations += gfx::GenerateShaderStruct("AnimeFullscreenData", fullscreen_fields, sizeof(AnimeFullscreenData));
#undef ANIME_FIELD
#undef ANIME_RESOURCE
    return {.modules = {
        {.name = "anime_contract", .source_code = std::move(declarations)},
        {.name = "viewer", .path = "examples/usd_viewer/viewer.slang"},
    }};
}

auto AnimeMaterialViews(const asset::Material& material, const asset::MaterialPass& pass)
    -> std::vector<std::shared_ptr<gfx::TextureView>> {
    std::vector<std::shared_ptr<gfx::TextureView>> views;
    for (const auto& binding : pass.bindings) {
        const auto* fallback = std::get_if<std::shared_ptr<asset::Texture>>(&binding.default_value);
        if (!fallback) continue;
        const auto texture = material.GetParameter<std::shared_ptr<asset::Texture>>(binding.name).value_or(*fallback);
        if (!texture) continue;
        auto view = texture->GetGPUView();
        if (!texture->Empty() && !view) {
            if (const auto placeholder = asset::Texture::DefaultTexture()) view = placeholder->GetGPUView();
        }
        if (view && std::ranges::find(views, view) == views.end()) views.push_back(std::move(view));
    }
    return views;
}

AnimeMaterialAdapter::AnimeMaterialAdapter() {
    const auto program = AnimeShaderProgram();
    std::pmr::vector<std::shared_ptr<asset::Shader>> shaders;
    for (const auto& [type, entry] : {std::pair{gfx::ShaderType::Vertex, "MeshVS"}, {gfx::ShaderType::Pixel, "MeshPS"}})
        shaders.push_back(std::make_shared<asset::Shader>(gfx::ShaderDesc{
            .name = entry, .type = type, .entry = entry, .program = program,
        }));
    const auto pipeline = [&](bool shadow) {
        return std::make_shared<asset::RenderPipeline>(gfx::RenderPipelineDesc{
            .name = shadow ? "Anime shadow contract" : "Anime surface contract",
            .vertex_input_layout = {
                {.semantic = "POSITION", .format = gfx::Format::R32G32B32_FLOAT, .binding = 0, .stride = 12},
                {.semantic = "NORMAL", .format = gfx::Format::R32G32B32_FLOAT, .binding = 1, .stride = 12},
                {.semantic = "TEXCOORD", .format = gfx::Format::R32G32_FLOAT, .binding = 2, .stride = 8},
            },
            .rasterization_state = {.cull_mode = gfx::CullMode::None, .depth_bias_enable = shadow, .depth_bias = 1.0f, .depth_bias_slope_factor = 1.5f},
            .depth_stencil_state = {.depth_test_enable = true, .depth_write_enable = true, .depth_compare_op = gfx::CompareOp::Less},
            .render_format = gfx::Format::R8G8B8A8_UNORM,
            .depth_stencil_format = gfx::Format::D32_FLOAT,
        }, shaders);
    };
    m_Surface = pipeline(false);
    m_Shadow = pipeline(true);
}

auto AnimeMaterialAdapter::Adapt(const asset::Material* source) const -> std::shared_ptr<asset::Material> {
    // Source aliases and type conversion belong at the import boundary. Runtime
    // rendering consumes only the canonical contract, including typed defaults.
    math::vec4f color{1, 1, 1, 1}, tint{1, 1, 1, 1};
    math::vec4f head{0, 0, 0, 1}, right{1, 0, 0, 0}, up{0, 1, 0, 0};
    float kind = 0, cutoff = 0, opacity = 1;
    std::shared_ptr<asset::Texture> texture, control;
    if (source) {
        const auto assign = [&](auto& value, std::string_view name) {
            const auto parameters = source->GetParameters();
            const auto parameter = std::ranges::find(parameters, name, &asset::MaterialParameter::name);
            if (parameter == parameters.end()) return;
            if (const auto* typed = std::get_if<std::remove_cvref_t<decltype(value)>>(&parameter->value)) value = *typed;
            else throw std::runtime_error(std::format("Anime material '{}': source parameter '{}' has the wrong type", source->GetName(), name));
        };
        assign(kind, "animeMaterial");
        assign(head, "animeHead");
        assign(right, "animeHeadRight");
        assign(up, "animeHeadUp");
        assign(tint, "animeBaseTint");
        assign(opacity, "opacity");
        assign(cutoff, "opacityThreshold");
        assign(control, "animeControl");
        for (const auto name : {"diffuseColor", "base_color"}) {
            if (auto value = source->GetParameter<std::shared_ptr<asset::Texture>>(name); value && *value) texture = *value;
            if (auto value = source->GetParameter<math::vec3f>(name)) color = math::vec4f(*value, 1);
            if (auto value = source->GetParameter<math::vec4f>(name)) color = *value;
            if (auto value = source->GetParameter<math::Color>(name)) color = math::vec4f(*value);
        }
    }
    if (!std::isfinite(kind) || kind < 0 || kind > 4 || kind != std::floor(kind))
        throw std::runtime_error("Anime material kind must be an integer in [0,4]");
    if (texture) color = tint;
    else color.a = opacity;
    asset::MaterialParameters parameters{
        {.name = "base_color", .value = color},
        {.name = "alpha_cutoff", .value = cutoff},
        {.name = "textured", .value = std::uint32_t(texture != nullptr)},
        {.name = "kind", .value = static_cast<std::uint32_t>(kind)},
        {.name = "controlled", .value = std::uint32_t(control != nullptr)},
        {.name = "head", .value = head},
        {.name = "head_right", .value = right},
        {.name = "head_up", .value = up},
        {.name = "base_color_texture", .value = texture},
        {.name = "control_texture", .value = control},
    };
    std::pmr::vector<asset::MaterialPass> passes;
    for (const auto contract : {kAnimeForward, kAnimeAlbedo, kAnimeNormals, kAnimeShadowCaster, kAnimeHairShadowCaster}) {
        if (contract == kAnimeHairShadowCaster && kind != 3) continue;
        const bool shadow = contract == kAnimeShadowCaster || contract == kAnimeHairShadowCaster;
        passes.push_back({.pass_contract = std::pmr::string(contract), .pipeline = shadow ? m_Shadow : m_Surface,
                          .bindings = AnimeMaterialBindings()});
    }
    auto material = std::make_shared<asset::Material>(std::move(parameters), std::move(passes), source ? source->GetName() : "Anime default");
    if (!material->ValidateBindings().empty()) throw std::logic_error("Invalid adapted anime material contract");
    return material;
}
} // namespace game::anime_viewer
