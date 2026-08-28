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

struct MaterialPass {
    std::pmr::string                   pass_contract;
    std::shared_ptr<RenderPipeline>    pipeline;
    std::pmr::vector<std::pmr::string> bindings;
    core::Buffer                       material_data;
};

class Material : public Resource {
public:
    Material(
        MaterialParameters             parameters = {},
        std::pmr::vector<MaterialPass> passes     = {},
        std::string_view               name       = "");

    void Load(const ResourceLoadContext& context) final;
    void Unload() final;

    inline auto GetParameters() const noexcept -> std::span<const MaterialParameter> { return m_Parameters; }
    inline auto GetPasses() const noexcept -> std::span<const MaterialPass> { return m_Passes; }

    auto FindPass(std::string_view pass_contract) const noexcept -> const MaterialPass*;

    template <MaterialParametric T>
    void SetParameter(std::string_view name, T value) noexcept;
    template <MaterialParametric T>
    auto GetParameter(std::string_view name) const noexcept -> std::optional<T>;

private:
    void InvalidatePassData() noexcept;
    auto GenerateMaterialData(const MaterialPass& pass, bool enable_16_bytes_packing) noexcept -> core::Buffer;

    MaterialParameters             m_Parameters;
    std::pmr::vector<MaterialPass> m_Passes;
    // True while any texture parameter is still decoding asynchronously; Load()
    // keeps repacking material_data (with placeholder handles) until all settle.
    bool m_HasPendingTextures = false;
};

template <MaterialParametric T>
void Material::SetParameter(std::string_view name, T value) noexcept {
    const auto iter = std::ranges::find_if(m_Parameters, [&](const auto& current) { return current.name == name; });
    if (iter != m_Parameters.end()) {
        iter->value = std::move(value);
    } else {
        m_Parameters.emplace_back(MaterialParameter{.name = std::pmr::string(name), .value = std::move(value)});
    }
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
    : Resource(Type::Material, name) {
    MaterialParameters unique;
    unique.reserve(parameters.size());
    for (const auto& parameter : parameters) {
        if (std::ranges::none_of(unique, [&](const auto& existing) { return existing.name == parameter.name; })) {
            unique.emplace_back(parameter);
        }
    }
    m_Parameters = std::move(unique);

    m_Passes.reserve(passes.size());
    for (const auto& source_pass : passes) {
        auto& pass         = m_Passes.emplace_back();
        pass.pass_contract = std::pmr::string(source_pass.pass_contract);
        pass.pipeline      = source_pass.pipeline;
        pass.bindings.reserve(source_pass.bindings.size());
        for (const auto& binding : source_pass.bindings) pass.bindings.emplace_back(binding);
        pass.material_data = source_pass.material_data;
    }
}

auto Material::FindPass(std::string_view pass_contract) const noexcept -> const MaterialPass* {
    const auto iter = std::ranges::find_if(m_Passes, [pass_contract](const auto& pass) { return pass.pass_contract == pass_contract; });
    return iter == m_Passes.end() ? nullptr : std::addressof(*iter);
}

auto Material::GenerateMaterialData(const MaterialPass& pass, bool enable_16_bytes_packing) noexcept -> core::Buffer {
    const auto get_parameter_size = [](const MaterialParameterValue& parameter) noexcept -> std::size_t {
        return std::visit(
            utils::Overloaded{
                [](const std::shared_ptr<Texture>&) -> std::size_t { return sizeof(gfx::BindlessHandle); },
                [](const auto& value) -> std::size_t { return sizeof(value); },
            },
            parameter);
    };

    const auto find_parameter = [this](std::string_view name) noexcept -> utils::optional_ref<const MaterialParameter> {
        const auto iter = std::ranges::find_if(m_Parameters, [name](const auto& parameter) {
            return parameter.name == name;
        });
        if (iter == m_Parameters.end()) return std::nullopt;
        return utils::make_optional_ref(*iter);
    };

    const auto calculate_material_data_size = [&]() noexcept -> std::size_t {
        std::size_t offset = 0;
        for (const auto& binding : pass.bindings) {
            const auto parameter = find_parameter(binding);
            if (!parameter) continue;

            const auto size = get_parameter_size(parameter->get().value);

            if (enable_16_bytes_packing) {
                const std::size_t remaining = (~(offset & 0xf) & 0xf) + 0x1;
                offset += remaining >= size ? 0 : remaining;
            }
            offset += size;
        }
        return std::max<std::size_t>(16, utils::align(offset, 16));
    };

    const auto buffer_size = pass.bindings.empty() ? 0 : calculate_material_data_size();

    core::Buffer result(buffer_size);
    if (!result.Empty()) std::memset(result.GetData(), 0, result.GetDataSize());

    std::size_t offset = 0;
    for (const auto& binding : pass.bindings) {
        const auto parameter = find_parameter(binding);
        if (!parameter) continue;

        const auto& value = parameter->get().value;
        const auto  size  = get_parameter_size(value);
        if (enable_16_bytes_packing) {
            const std::size_t remaining = (~(offset & 0xf) & 0xf) + 0x1;
            offset += remaining >= size ? 0 : remaining;
        }

        std::visit(
            utils::Overloaded{
                [&](const std::shared_ptr<Texture>& texture) {
                    auto view = texture ? texture->GetGPUView() : nullptr;
                    if (texture && !texture->Empty() && view == nullptr) {
                        // Async decode still in flight: sample the placeholder until
                        // Load() repacks this buffer with the real handle.
                        if (const auto placeholder = Texture::DefaultTexture()) view = placeholder->GetGPUView();
                    }
                    const auto handle = view ? view->GetBindlessHandle() : gfx::BindlessHandle{};
                    if (size == sizeof(handle)) std::memcpy(result.GetData() + offset, std::addressof(handle), size);
                },
                [&](const auto& data) {
                    using T = std::decay_t<decltype(data)>;
                    if (size == sizeof(T)) std::memcpy(result.GetData() + offset, std::addressof(data), size);
                },
            },
            value);
        offset += size;
    }
    return result;
}

void Material::InvalidatePassData() noexcept {
    for (auto& pass : m_Passes) {
        pass.material_data = {};
    }
    SetLoadState(ResourceLoadState::Unloaded);
}

void Material::Load(const ResourceLoadContext& context) {
    if (GetLoadState() == ResourceLoadState::Loaded && !m_HasPendingTextures) return;

    bool pending = false;
    for (auto& parameter : m_Parameters) {
        const auto* texture = std::get_if<std::shared_ptr<Texture>>(std::addressof(parameter.value));
        if (texture && *texture && !(*texture)->Empty()) {
            (*texture)->Load(context);
            if (!(*texture)->IsSettled()) pending = true;
        }
    }
    // Make sure the placeholder is resident before its handle gets packed below.
    if (pending) Texture::DefaultTexture()->Load(context);

    for (auto& pass : m_Passes) {
        if (pass.pipeline) pass.pipeline->Load(context);
        pass.material_data = GenerateMaterialData(pass, context.device.device_type == gfx::Device::Type::DX12);
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
