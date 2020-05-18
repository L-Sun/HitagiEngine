#include "PipeLineState.hpp"
#include "D3DCore.hpp"

namespace Hitagi::Graphics::DX12 {
void GraphicsPSO::SetInputLayout(const std::vector<D3D12_INPUT_ELEMENT_DESC>& inputLayout) {
    m_InputLayouts = inputLayout;
}

void GraphicsPSO::SetBlendState(const D3D12_BLEND_DESC& desc) { m_PSODesc.BlendState = desc; }
void GraphicsPSO::SetRasterizerState(const D3D12_RASTERIZER_DESC& desc) { m_PSODesc.RasterizerState = desc; }
void GraphicsPSO::SetDepthStencilState(const D3D12_DEPTH_STENCIL_DESC& desc) { m_PSODesc.DepthStencilState = desc; }
void GraphicsPSO::SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE type) {
    m_PSODesc.PrimitiveTopologyType = type;
}
void GraphicsPSO::SetSampleMask(UINT mask) { m_PSODesc.SampleMask = mask; }

void GraphicsPSO::SetVertexShader(const VertexShader& shader) {
    const Core::Buffer& code = shader.GetShaderCode();
    m_PSODesc.VS             = CD3DX12_SHADER_BYTECODE(code.GetData(), code.GetDataSize());
}

void GraphicsPSO::SetPixelShader(const PixelShader& shader) {
    const Core::Buffer& code = shader.GetShaderCode();
    m_PSODesc.PS             = CD3DX12_SHADER_BYTECODE(code.GetData(), code.GetDataSize());
}

void GraphicsPSO::SetRenderTargetFormats(const std::vector<DXGI_FORMAT>& RTVFormats, DXGI_FORMAT DSVFormat) {
    for (int i = 0; i < RTVFormats.size(); i++) m_PSODesc.RTVFormats[i] = RTVFormats[i];
    for (int i = RTVFormats.size(); i < m_PSODesc.NumRenderTargets; i++) m_PSODesc.RTVFormats[i] = DXGI_FORMAT_UNKNOWN;
    m_PSODesc.NumRenderTargets   = RTVFormats.size();
    m_PSODesc.DSVFormat          = DSVFormat;
    m_PSODesc.SampleDesc.Count   = 1;
    m_PSODesc.SampleDesc.Quality = 0;
}

void GraphicsPSO::Finalize() {
    m_PSODesc.InputLayout    = {m_InputLayouts.data(), static_cast<UINT>(m_InputLayouts.size())};
    m_PSODesc.pRootSignature = m_RootSignature->GetRootSignature();
    assert(m_PSODesc.pRootSignature != nullptr);
    ThrowIfFailed(g_Device->CreateGraphicsPipelineState(&m_PSODesc, IID_PPV_ARGS(&m_PSO)));
}

ComputePSO::ComputePSO() {
    ZeroMemory(&m_PSODesc, sizeof(m_PSODesc));
    m_PSODesc.NodeMask = 1;
}

void ComputePSO::SetComputeShader(const ComputeShader& shader) {
    const Core::Buffer& code = shader.GetShaderCode();
    m_PSODesc.CS             = CD3DX12_SHADER_BYTECODE(code.GetData(), code.GetDataSize());
}

void ComputePSO::Finalize() {
    // Make sure the root signature is finalized first
    m_PSODesc.pRootSignature = m_RootSignature->GetRootSignature();
    assert(m_PSODesc.pRootSignature != nullptr);
    ThrowIfFailed(g_Device->CreateComputePipelineState(&m_PSODesc, IID_PPV_ARGS(&m_PSO)));
}

}  // namespace Hitagi::Graphics::DX12