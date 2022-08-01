#pragma once
#include "d3d_pch.hpp"

#include <hitagi/graphics/gpu_resource.hpp>

#include <magic_enum.hpp>

namespace hitagi::graphics::backend::DX12 {
class PSO : public backend::Resource {
public:
    PSO(std::string_view name, ID3D12Device* device) : m_Name(name), m_Device(device) {}
    PSO(const PSO&)            = delete;
    PSO& operator=(const PSO&) = delete;
    PSO(PSO&&)                 = default;
    PSO& operator=(PSO&&)      = default;

    inline ID3D12PipelineState*        GetPSO() const { return m_PSO.Get(); }
    inline ComPtr<ID3D12RootSignature> GetRootSignature() const noexcept { return m_RootSignature; }
    inline const auto&                 GetRootSignatureDesc() const noexcept { return m_RootSignatureDesc; }

protected:
    void CreateRootSignature(const std::shared_ptr<resource::Shader>& shader);

    std::string                                      m_Name;
    ID3D12Device*                                    m_Device;
    ComPtr<ID3D12VersionedRootSignatureDeserializer> m_RootSignatureDeserializer;
    ComPtr<ID3D12RootSignature>                      m_RootSignature;
    D3D12_ROOT_SIGNATURE_DESC1                       m_RootSignatureDesc;
    ComPtr<ID3D12PipelineState>                      m_PSO = nullptr;
};

class GraphicsPSO : public PSO {
public:
    using PSO::PSO;

    GraphicsPSO& SetInputLayout(std::pmr::vector<D3D12_INPUT_ELEMENT_DESC> input_layout);
    GraphicsPSO& SetShader(const std::shared_ptr<resource::Shader>& shader);
    GraphicsPSO& SetBlendState(const D3D12_BLEND_DESC& desc);
    GraphicsPSO& SetRasterizerState(const D3D12_RASTERIZER_DESC& desc);
    GraphicsPSO& SetDepthStencilState(const D3D12_DEPTH_STENCIL_DESC& desc);
    GraphicsPSO& SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE type);
    GraphicsPSO& SetSampleMask(unsigned mask);
    GraphicsPSO& SetRenderTargetFormats(const std::vector<DXGI_FORMAT>& rtv_formats, DXGI_FORMAT dsv_format, unsigned msaa_count = 1, unsigned msaa_quality = 0);

    void Finalize();

    int GetAttributeSlot(resource::VertexAttribute attr) const;

private:
    D3D12_GRAPHICS_PIPELINE_STATE_DESC         m_PSODesc{};
    std::pmr::vector<D3D12_INPUT_ELEMENT_DESC> m_InputLayouts;
};

class ComputePSO : public PSO {
public:
    using PSO::PSO;

    ComputePSO& SetComputeShader(const std::shared_ptr<resource::Shader>& shader);
    void        Finalize(ID3D12Device* device);

private:
    D3D12_COMPUTE_PIPELINE_STATE_DESC m_PSODesc{};
};

}  // namespace hitagi::graphics::backend::DX12