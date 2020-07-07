#include "PipelineState.hpp"
#include "DriverAPI.hpp"

namespace Hitagi::Graphics {
size_t RootSignature::GetNewId() {
    static size_t idGenerator = 0;
    return idGenerator++;
}

RootSignature::RootSignature() : m_Id(GetNewId()) {}

RootSignature::RootSignature(const RootSignature& rhs)
    : m_Id(GetNewId()), m_ParameterTable(rhs.m_ParameterTable) {}

RootSignature::RootSignature(RootSignature&& rhs)
    : m_Driver(rhs.m_Driver),
      m_Created(rhs.m_Created),
      m_Id(rhs.m_Id),
      m_ParameterTable(std::move(rhs.m_ParameterTable)) {
    rhs.m_Created = false;
}

RootSignature::~RootSignature() {
    if (m_Created && m_Driver) m_Driver->DeleteRootSignature(*this);
}

RootSignature& RootSignature::Add(std::string_view name, ShaderVariableType type, unsigned registerIndex, unsigned space, ShaderVisibility visibility) {
    if (m_Created) throw std::logic_error("RootSignature has been created.");
    m_ParameterTable.emplace(Parameter{std::string{name}, visibility, type, registerIndex, space});
    return *this;
}

void RootSignature::Create(backend::DriverAPI& driver) {
    if (m_Created) throw std::logic_error("RootSignature has been created.");
    m_Driver = &driver;
    // Call backend API to create root signature
    m_Driver->CreateRootSignature(*this);
    m_Created = true;
}

std::set<std::string> PipelineState::m_PSOName;

PipelineState::~PipelineState() {
    if (m_Created && m_Driver) m_Driver->DeletePipelineState(*this);
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
PipelineState& PipelineState::SetInputLayout(const std::vector<InputLayout>& inputLayout) {
    if (m_Created) throw std::logic_error("PSO has been created.");
    m_InputLayout = inputLayout;
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

void PipelineState::Create(backend::DriverAPI& driver) {
    if (m_Created) throw std::logic_error("PSO has been created.");
    m_Driver = &driver;
    if (!(m_Vs && m_Ps && m_RootSignature)) throw std::logic_error("RootSignature is incompleted.");
    m_Driver->CreatePipelineState(*this);
    m_Created = true;
}

}  // namespace Hitagi::Graphics