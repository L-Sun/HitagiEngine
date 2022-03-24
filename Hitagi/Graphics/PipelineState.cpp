#include "PipelineState.hpp"
#include "DriverAPI.hpp"

namespace hitagi::graphics {

RootSignature::RootSignature(RootSignature&& rhs)
    : Resource(std::move(rhs.m_Name), std::move(rhs.m_Resource)),
      m_Created(rhs.m_Created),
      m_ParameterTable(std::move(rhs.m_ParameterTable)) {
    rhs.m_Created = false;
}

RootSignature& RootSignature::operator=(RootSignature&& rhs) {
    if (this != &rhs) {
        m_Name           = std::move(rhs.m_Name);
        m_Created        = rhs.m_Created;
        m_ParameterTable = std::move(rhs.m_ParameterTable);
        m_Resource.reset(rhs.m_Resource.release());
        rhs.m_Created = false;
    }
    return *this;
}

RootSignature& RootSignature::Add(std::string_view name, ShaderVariableType type, unsigned register_index, unsigned space, ShaderVisibility visibility) {
    if (m_Created) throw std::logic_error("RootSignature has been created.");
    m_ParameterTable.emplace_back(Parameter{
        std::string{name},
        type,
        register_index,
        space,
        visibility,
    });
    return *this;
}

RootSignature& RootSignature::AddStaticSampler(
    std::string_view     name,
    Sampler::Description desc,
    unsigned             register_index,
    unsigned             sapce,
    ShaderVisibility     visibility) {
    m_StaticSamplerDescs.emplace_back(StaticSamplerDescription{
        std::string{name},
        desc,
        register_index,
        sapce,
        visibility,
    });

    return *this;
}

RootSignature& RootSignature::Create(DriverAPI& driver) {
    if (m_Created) throw std::logic_error("RootSignature has been created.");
    // Call backend API to create root signature
    m_Resource = driver.CreateRootSignature(*this);
    m_Created  = true;
    return *this;
}

PipelineState& PipelineState::SetVertexShader(std::shared_ptr<VertexShader> vs) {
    if (m_Created) throw std::logic_error("PSO has been created.");
    m_Vs = vs;
    return *this;
}
PipelineState& PipelineState::SetPixelShader(std::shared_ptr<PixelShader> ps) {
    if (m_Created) throw std::logic_error("PSO has been created.");
    m_Ps = ps;
    return *this;
}
PipelineState& PipelineState::SetInputLayout(const std::vector<InputLayout>& input_layout) {
    if (m_Created) throw std::logic_error("PSO has been created.");
    m_InputLayout = input_layout;
    return *this;
}
PipelineState& PipelineState::SetRootSignautre(std::shared_ptr<RootSignature> sig) {
    if (m_Created) throw std::logic_error("PSO has been created.");
    m_RootSignature = std::move(sig);
    return *this;
}
PipelineState& PipelineState::SetRenderFormat(Format format) {
    if (m_Created) throw std::logic_error("PSO has been created.");
    m_RenderFormat = format;
    return *this;
}
PipelineState& PipelineState::SetDepthBufferFormat(Format format) {
    if (m_Created) throw std::logic_error("PSO has been created.");
    m_DepthBufferFormat = format;
    return *this;
}

PipelineState& PipelineState::SetPrimitiveType(PrimitiveType type) {
    if (m_Created) throw std::logic_error("PSO has been created.");
    m_PrimitiveType = type;
    return *this;
}

PipelineState& PipelineState::SetBlendState(BlendDescription desc) {
    if (m_Created) throw std::logic_error("PSO has been created.");
    m_BlendState = desc;
    return *this;
}

PipelineState& PipelineState::SetRasterizerState(RasterizerDescription desc) {
    if (m_Created) throw std::logic_error("PSO has been created.");
    m_RasterizerState = desc;
    return *this;
}

void PipelineState::Create(DriverAPI& driver) {
    if (m_Created) throw std::logic_error("PSO has been created.");
    if (!(m_Vs && m_Ps && m_RootSignature)) throw std::logic_error("RootSignature is incompleted.");
    m_Resource = driver.CreatePipelineState(*this);
    m_Created  = true;
}

}  // namespace hitagi::graphics