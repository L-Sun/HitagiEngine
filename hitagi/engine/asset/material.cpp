module;

#include <concepts>

export module asset:material;
import std;
import core;
import gfx;
import math;
import utils;
import :resource;
import :texture;
import :pipeline;
import interop.spdlog;

export namespace hitagi::asset {

using MaterialParameterValue = std::variant<
    float,
    std::int32_t,
    std::uint32_t,
    math::vec2i,
    math::vec2u,
    math::vec2f,
    math::vec3i,
    math::vec3u,
    math::vec3f,
    math::vec4i,
    math::vec4u,
    math::vec4f,
    math::Color,
    math::mat4f,
    std::shared_ptr<Texture>>;

using MaterialDataValue = std::variant<
    float,
    std::int32_t,
    std::uint32_t,
    math::vec2i,
    math::vec2u,
    math::vec2f,
    math::vec3i,
    math::vec3u,
    math::vec3f,
    math::vec4i,
    math::vec4u,
    math::vec4f,
    math::Color,
    math::mat4f,
    gfx::BindlessHandle>;

auto EncodeMaterialData(std::span<const MaterialDataValue> values) -> core::Buffer {
    std::size_t size = 0;
    for (const auto& value : values) std::visit([&](const auto& data) { size += sizeof(data); }, value);
    core::Buffer result(size);
    std::size_t  offset = 0;
    for (const auto& value : values) {
        std::visit([&](const auto& data) {
            // Material encoding follows the shader's 32-bit field ABI, not binding alignment.
            static_assert(sizeof(data) % sizeof(std::uint32_t) == 0);
            std::memcpy(result.GetData() + offset, &data, sizeof(data));
            offset += sizeof(data);
        },
                   value);
    }
    return result;
}

template <typename T>
concept MaterialParametric = requires(const MaterialParameterValue& parameter) {
    { std::get<T>(parameter) } -> std::same_as<const T&>;
};

struct MaterialParameter {
    std::pmr::string       name;
    MaterialParameterValue value;

    inline bool operator==(const MaterialParameter& rhs) const noexcept { return name == rhs.name && value == rhs.value; }
};

using MaterialParameters = std::pmr::vector<MaterialParameter>;

// The required, typed default is the binding's type authority. There is no
// name-only constructor and no separately maintained type tag.
struct MaterialBinding {
    MaterialBinding(std::string_view name, MaterialParameterValue default_value)
        : name(name), default_value(std::move(default_value)) {}

    std::pmr::string       name;
    MaterialParameterValue default_value;
};

inline auto MaterialParameterTypeName(const MaterialParameterValue& value) -> std::string_view {
    return std::visit([]<typename T>(const T&) -> std::string_view {
        if constexpr (std::is_same_v<T, std::shared_ptr<Texture>>)
            return "Texture2D";
        else if constexpr (std::is_same_v<T, math::Color>)
            return "Color";
        else
            return gfx::ShaderFieldType<T>();
    },
                      value);
}

inline auto MaterialDataFields(std::span<const MaterialBinding> bindings) -> std::pmr::vector<gfx::ShaderField> {
    std::pmr::vector<gfx::ShaderField> fields;
    std::size_t                        offset = 0;
    for (const MaterialBinding& binding : bindings) {
        fields.emplace_back(std::visit([&]<typename T>(const T&) -> gfx::ShaderField {
            if constexpr (std::is_same_v<T, std::shared_ptr<Texture>>)
                return gfx::MakeShaderResourceField(binding.name, offset, gfx::BindlessHandleType::Texture);
            else
                return gfx::MakeShaderField<T>(binding.name, offset);
        },
                                    binding.default_value));
        offset += fields.back().size;
    }
    return fields;
}

struct MaterialPass {
    std::pmr::string                  pass_contract;
    std::shared_ptr<RenderPipeline>   pipeline;
    std::pmr::vector<MaterialBinding> bindings;
    core::Buffer                      material_data;
};

struct MaterialPassStage {
    gfx::ShaderType  type;
    std::pmr::string entry;
};

// A renderer-owned pass that material compilers specialize. The shell is a
// Slang file module declaring `Surface`, `SurfaceInput`, `MaterialContext` and
// `IMaterial`; every stage entry is generic over one `IMaterial` type.
struct MaterialPassTemplate {
    std::pmr::string                    contract;
    gfx::ShaderModuleDesc               shell;
    std::pmr::vector<MaterialPassStage> stages;
    gfx::RenderPipelineDesc             pipeline;
};

class Material : public Resource {
public:
    Material(
        MaterialParameters             parameters = {},
        std::pmr::vector<MaterialPass> passes     = {},
        std::string_view               name       = "");

    void Load(const ResourceLoadContext& context) final;
    void Unload() final;

    // Strict authoring/cook validation; runtime uses the same checks as warnings.
    auto ValidateBindings() const -> std::pmr::vector<std::pmr::string>;

    inline auto GetParameters() const noexcept -> std::span<const MaterialParameter> { return m_Parameters; }
    inline auto GetPasses() const noexcept -> std::span<const MaterialPass> { return m_Passes; }

    auto FindPass(std::string_view pass_contract) const noexcept -> const MaterialPass*;

    // Clone parameter state and pass definitions, sharing shader/pipeline/texture
    // dependencies. The clone has a new identity and no derived resident data.
    auto Clone(std::string_view name) const -> std::shared_ptr<Material>;

    // Definition fixes the parameter type. Duplicate definitions and invalid
    // assignments throw std::invalid_argument without changing the material.
    template <MaterialParametric T>
    void SetParameter(std::string_view name, T value);
    template <MaterialParametric T>
    auto GetParameter(std::string_view name) const noexcept -> std::optional<T>;

private:
    void InvalidatePassData() noexcept;
    auto GenerateMaterialData(const MaterialPass& pass) -> core::Buffer;

    MaterialParameters             m_Parameters;
    std::pmr::vector<MaterialPass> m_Passes;
    // True while any texture parameter is still decoding asynchronously; Load()
    // keeps repacking material_data (with placeholder handles) until all settle.
    bool m_HasPendingTextures      = false;
    bool m_ReportedBindingWarnings = false;
};

template <MaterialParametric T>
void Material::SetParameter(std::string_view name, T value) {
    const auto iter = std::ranges::find_if(m_Parameters, [&](const auto& current) { return current.name == name; });
    if (iter == m_Parameters.end()) {
        throw std::invalid_argument(std::format("Material parameter '{}' is not defined", name));
    }
    if (!std::holds_alternative<T>(iter->value)) {
        throw std::invalid_argument(std::format("Material parameter '{}' has a different type", name));
    }
    iter->value = std::move(value);
    InvalidatePassData();
}

template <MaterialParametric T>
auto Material::GetParameter(std::string_view name) const noexcept -> std::optional<T> {
    const auto iter = std::ranges::find_if(m_Parameters, [name](const auto& parameter) {
        return parameter.name == name;
    });
    if (iter == m_Parameters.end() || !std::holds_alternative<T>(iter->value)) return std::nullopt;
    return std::get<T>(iter->value);
}

}  // namespace hitagi::asset

namespace hitagi::asset {

Material::Material(MaterialParameters parameters, std::pmr::vector<MaterialPass> passes, std::string_view name)
    : Resource(Type::Material, name), m_Passes(std::move(passes)) {
    m_Parameters.reserve(parameters.size());
    for (auto& parameter : parameters) {
        if (std::ranges::any_of(m_Parameters, [&](const auto& existing) { return existing.name == parameter.name; }))
            throw std::invalid_argument("Duplicate material parameter: " + std::string(parameter.name));
        m_Parameters.emplace_back(std::move(parameter));
    }
}

auto Material::Clone(std::string_view name) const -> std::shared_ptr<Material> {
    std::pmr::vector<MaterialPass> passes;
    for (const auto& pass : m_Passes)
        passes.push_back({.pass_contract = pass.pass_contract, .pipeline = pass.pipeline, .bindings = pass.bindings});
    return std::make_shared<Material>(m_Parameters, std::move(passes), name);
}

auto Material::FindPass(std::string_view pass_contract) const noexcept -> const MaterialPass* {
    const auto iter = std::ranges::find_if(m_Passes, [pass_contract](const auto& pass) { return pass.pass_contract == pass_contract; });
    return iter == m_Passes.end() ? nullptr : std::addressof(*iter);
}

auto Material::ValidateBindings() const -> std::pmr::vector<std::pmr::string> {
    std::pmr::vector<std::pmr::string> errors;
    for (const MaterialPass& pass : m_Passes) {
        for (const MaterialBinding& binding : pass.bindings) {
            const auto parameter = std::ranges::find(m_Parameters, binding.name, &MaterialParameter::name);
            if (parameter == m_Parameters.end() || parameter->value.index() != binding.default_value.index()) {
                errors.emplace_back(std::format(
                    "Material '{}', pass '{}', binding '{}': expected {}, got {}; contract default will be used at runtime",
                    GetName(), pass.pass_contract, binding.name,
                    MaterialParameterTypeName(binding.default_value),
                    parameter == m_Parameters.end() ? "missing parameter" : MaterialParameterTypeName(parameter->value)));
            }
        }
    }
    return errors;
}

auto Material::GenerateMaterialData(const MaterialPass& pass) -> core::Buffer {
    std::pmr::vector<MaterialDataValue> values;
    values.reserve(pass.bindings.size());
    for (const auto& binding : pass.bindings) {
        const auto                    parameter = std::ranges::find(m_Parameters, binding.name, &MaterialParameter::name);
        const MaterialParameterValue& value =
            parameter != m_Parameters.end() && parameter->value.index() == binding.default_value.index()
                ? parameter->value
                : binding.default_value;
        std::visit(utils::Overloaded{
                       [&](const std::shared_ptr<Texture>& texture) {
                           auto view = texture ? texture->GetGPUView() : nullptr;
                           if (texture && !texture->Empty() && !view) {
                               if (const auto placeholder = Texture::DefaultTexture()) view = placeholder->GetGPUView();
                           }
                           values.emplace_back(view ? view->GetBindlessHandle() : gfx::BindlessHandle{});
                       },
                       [&](const auto& value) { values.emplace_back(value); },
                   },
                   value);
    }
    return EncodeMaterialData(values);
}

void Material::InvalidatePassData() noexcept {
    m_ReportedBindingWarnings = false;
    for (auto& pass : m_Passes) {
        pass.material_data = {};
    }
    SetLoadState(ResourceLoadState::Unloaded);
}

void Material::Load(const ResourceLoadContext& context) {
    if (GetLoadState() == ResourceLoadState::Loaded && !m_HasPendingTextures) return;

    if (!m_ReportedBindingWarnings) {
        for (const auto& warning : ValidateBindings()) spdlog::warn("{}", warning);
        m_ReportedBindingWarnings = true;
    }
    bool       pending           = false;
    bool       needs_placeholder = false;
    const auto load_texture      = [&](const MaterialParameterValue& value) {
        const auto* texture = std::get_if<std::shared_ptr<Texture>>(&value);
        if (texture && *texture && !(*texture)->Empty()) {
            (*texture)->Load(context);
            if (!(*texture)->IsSettled()) pending = true;
            if (!(*texture)->GetGPUView()) needs_placeholder = true;
        }
    };
    for (const MaterialParameter& parameter : m_Parameters) load_texture(parameter.value);
    for (const MaterialPass& pass : m_Passes)
        for (const MaterialBinding& binding : pass.bindings) load_texture(binding.default_value);
    // Make sure the placeholder is resident before its handle gets packed below.
    if (needs_placeholder) Texture::DefaultTexture()->Load(context);

    for (auto& pass : m_Passes) {
        if (pass.pipeline) pass.pipeline->Load(context);
        pass.material_data = GenerateMaterialData(pass);
    }
    m_HasPendingTextures = pending;
    SetLoadState(ResourceLoadState::Loaded);
}

void Material::Unload() {
    if (GetLoadState() != ResourceLoadState::Loaded) return;
    for (auto& pass : m_Passes) {
        pass.material_data = {};
        if (pass.pipeline) pass.pipeline->Unload();
    }
    m_HasPendingTextures = false;
    SetLoadState(ResourceLoadState::Unloaded);
}

}  // namespace hitagi::asset
