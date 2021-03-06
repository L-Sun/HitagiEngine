#include "PSO.hpp"

namespace Hitagi::Graphics::backend::DX12 {
void GraphicsPSO::SetInputLayout(const std::vector<D3D12_INPUT_ELEMENT_DESC>& inputLayout) {
    m_InputLayouts = inputLayout;
}

void GraphicsPSO::SetBlendState(const D3D12_BLEND_DESC& desc) { m_PSODesc.BlendState = desc; }
void GraphicsPSO::SetRasterizerState(const D3D12_RASTERIZER_DESC& desc) { m_PSODesc.RasterizerState = desc; }
void GraphicsPSO::SetDepthStencilState(const D3D12_DEPTH_STENCIL_DESC& desc) { m_PSODesc.DepthStencilState = desc; }
void GraphicsPSO::SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE type) {
    m_PSODesc.PrimitiveTopologyType = type;
}
void GraphicsPSO::SetSampleMask(unsigned mask) { m_PSODesc.SampleMask = mask; }

void GraphicsPSO::SetVertexShader(CD3DX12_SHADER_BYTECODE code) {
    m_PSODesc.VS = code;
}

void GraphicsPSO::SetPixelShader(CD3DX12_SHADER_BYTECODE code) {
    m_PSODesc.PS = code;
}

void GraphicsPSO::SetRenderTargetFormats(
    const std::vector<DXGI_FORMAT>& RTVFormats, DXGI_FORMAT DSVFormat, unsigned MsaaCount, unsigned MsaaQuality) {
    for (int i = 0; i < RTVFormats.size(); i++) m_PSODesc.RTVFormats[i] = RTVFormats[i];
    for (int i = RTVFormats.size(); i < m_PSODesc.NumRenderTargets; i++) m_PSODesc.RTVFormats[i] = DXGI_FORMAT_UNKNOWN;
    m_PSODesc.NumRenderTargets   = RTVFormats.size();
    m_PSODesc.DSVFormat          = DSVFormat;
    m_PSODesc.SampleDesc.Count   = MsaaCount;
    m_PSODesc.SampleDesc.Quality = MsaaQuality;
}

void GraphicsPSO::Finalize(ID3D12Device* device) {
    assert(device);
    m_PSODesc.InputLayout    = {m_InputLayouts.data(), static_cast<UINT>(m_InputLayouts.size())};
    m_PSODesc.pRootSignature = m_RootSignature->GetRootSignature();
    assert(m_PSODesc.pRootSignature != nullptr);
    ThrowIfFailed(device->CreateGraphicsPipelineState(&m_PSODesc, IID_PPV_ARGS(&m_PSO)));
}

GraphicsPSO GraphicsPSO::Copy() const {
    GraphicsPSO ret;
    ret.m_RootSignature = m_RootSignature;
    ret.m_PSODesc       = m_PSODesc;
    ret.m_InputLayouts  = m_InputLayouts;
    return ret;
}

ComputePSO::ComputePSO() {
    ZeroMemory(&m_PSODesc, sizeof(m_PSODesc));
    m_PSODesc.NodeMask = 1;
}

void ComputePSO::SetComputeShader(CD3DX12_SHADER_BYTECODE code) {
    m_PSODesc.CS = code;
}

void ComputePSO::Finalize(ID3D12Device* device) {
    // Make sure the root signature is finalized first
    m_PSODesc.pRootSignature = m_RootSignature->GetRootSignature();
    assert(m_PSODesc.pRootSignature != nullptr);
    ThrowIfFailed(device->CreateComputePipelineState(&m_PSODesc, IID_PPV_ARGS(&m_PSO)));
}

}  // namespace Hitagi::Graphics::backend::DX12