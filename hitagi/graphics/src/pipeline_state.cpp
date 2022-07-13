#include <hitagi/graphics/pipeline_state.hpp>
#include <hitagi/graphics/driver_api.hpp>

#include <spdlog/spdlog.h>

#include <string_view>

namespace hitagi::graphics {

using namespace hitagi::resource;

auto RootSignature::Builder::SetName(std::string_view _name) -> Builder& {
    name = _name;
    return *this;
}

auto RootSignature::Builder::Add(std::string_view name, ShaderVariableType type, unsigned register_index, unsigned space, ShaderVisibility visibility) -> Builder& {
    parameter_table.emplace_back(Parameter{std::pmr::string(name), type, register_index, space, visibility});
    return *this;
}

auto RootSignature::Builder::AddStaticSampler(std::string_view name, SamplerDesc desc, unsigned register_index, unsigned space, ShaderVisibility visibility) -> Builder& {
    static_sampler_descs.emplace_back(StaticSamplerDescription{std::pmr::string(name), desc, register_index, space, visibility});
    return *this;
}

auto RootSignature::Builder::Create(DriverAPI& driver) -> std::shared_ptr<RootSignature> {
    auto root_signature = RootSignature::Create(*this);
    driver.CreateRootSignature(root_signature);

    return root_signature;
}

auto PipelineState::Builder::SetName(std::string_view name) -> Builder& {
    this->name = name;
    return *this;
}

auto PipelineState::Builder::SetVertexShader(core::Buffer vs) -> Builder& {
    vertex_shader = std::move(vs);
    return *this;
}
auto PipelineState::Builder::SetPixelShader(core::Buffer ps) -> Builder& {
    pixel_shader = std::move(ps);
    return *this;
}
auto PipelineState::Builder::SetRootSignautre(std::shared_ptr<RootSignature> sig) -> Builder& {
    root_signature = std::move(sig);
    return *this;
}
auto PipelineState::Builder::SetRenderFormat(Format format) -> Builder& {
    render_format = format;
    return *this;
}
auto PipelineState::Builder::SetDepthBufferFormat(Format format) -> Builder& {
    depth_buffer_format = format;
    return *this;
}

auto PipelineState::Builder::SetPrimitiveType(PrimitiveType type) -> Builder& {
    primitive_type = type;
    return *this;
}

auto PipelineState::Builder::SetBlendState(BlendDescription desc) -> Builder& {
    blend_state = desc;
    return *this;
}

auto PipelineState::Builder::SetRasterizerState(RasterizerDescription desc) -> Builder& {
    rasterizer_state = desc;
    return *this;
}

auto PipelineState::Builder::Build(DriverAPI& driver) -> std::shared_ptr<PipelineState> {
    if (vertex_shader.Empty() || pixel_shader.Empty() || root_signature == nullptr) {
        if (auto logger = spdlog::get("GraphicsManager"); logger) {
            logger->error("RootSignature is incompleted.");
        }
        throw std::logic_error("RootSignature is incompleted.");
    }
    auto result = PipelineState::Create(*this);
    driver.CreatePipelineState(result);
    return result;
}

RootSignature::RootSignature(const Builder& builder)
    : Resource(builder.name, nullptr), RootSignatureDetial(builder) {}

PipelineState::PipelineState(const Builder& builder)
    : Resource(builder.name, nullptr), PipelineStateDetial(builder) {}

}  // namespace hitagi::graphics