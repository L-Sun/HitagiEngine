#pragma once
#include "ShaderManager.hpp"
#include "RootSignature.hpp"
#include "Buffer.hpp"

namespace Hitagi::Graphics::DX12 {
class PipeLineState {
public:
    PipeLineState() = default;

    void                 SetRootSignature(const RootSignature& rootSignature) { m_RootSignature = &rootSignature; }
    ID3D12PipelineState* GetPSO() const { return m_PSO.Get(); }

protected:
    const RootSignature*                        m_RootSignature = nullptr;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_PSO;
};

class GraphicsPSO : public PipeLineState {
public:
    using PipeLineState::PipeLineState;
    void SetInputLayout(const std::vector<D3D12_INPUT_ELEMENT_DESC>& inputLayout);
    void SetVertexShader(const VertexShader& shader);
    void SetPixelShader(const PixelShader& shader);
    void SetBlendState(const D3D12_BLEND_DESC& desc);
    void SetRasterizerState(const D3D12_RASTERIZER_DESC& desc);
    void SetDepthStencilState(const D3D12_DEPTH_STENCIL_DESC& desc);
    void SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE type);
    void SetSampleMask(UINT mask);
    void SetRenderTargetFormats(const std::vector<DXGI_FORMAT>& RTVFormats, DXGI_FORMAT DSVFormat);

    void Finalize();

private:
    D3D12_GRAPHICS_PIPELINE_STATE_DESC    m_PSODesc;
    std::vector<D3D12_INPUT_ELEMENT_DESC> m_InputLayouts;
};

class ComputePSO : public PipeLineState {
public:
    ComputePSO();
    void SetComputeShader(const ComputeShader& shader);
    void Finalize();

private:
    D3D12_COMPUTE_PIPELINE_STATE_DESC m_PSODesc;
};

}  // namespace Hitagi::Graphics::DX12