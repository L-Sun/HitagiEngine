#pragma once
#include "D3Dpch.hpp"

namespace Hitagi::Graphics::DX12 {
struct RootParameter : public CD3DX12_ROOT_PARAMETER1 {
public:
    using CD3DX12_ROOT_PARAMETER1::CD3DX12_ROOT_PARAMETER1;
    ~RootParameter() {
        if (ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE) delete[] DescriptorTable.pDescriptorRanges;
    }

    void InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE type, UINT shaderRegister, UINT count,
                               D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL) {
        InitAsDescriptorTable(1, visibility);
        SetTableRange(0, type, shaderRegister, count);
    }

    void InitAsDescriptorTable(UINT rangeCount, D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL) {
        D3D12_DESCRIPTOR_RANGE1* range = new D3D12_DESCRIPTOR_RANGE1[rangeCount];
        CD3DX12_ROOT_PARAMETER1::InitAsDescriptorTable(rangeCount, range, visibility);
    }

    void SetTableRange(UINT rangeIndex, D3D12_DESCRIPTOR_RANGE_TYPE type, UINT shaderRegister, UINT count,
                       UINT space = 0, D3D12_DESCRIPTOR_RANGE_FLAGS flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE) {
        D3D12_DESCRIPTOR_RANGE1& range =
            *const_cast<D3D12_DESCRIPTOR_RANGE1*>(DescriptorTable.pDescriptorRanges + rangeIndex);
        range = CD3DX12_DESCRIPTOR_RANGE1(type, count, shaderRegister, space);
    }
};

class RootSignature {
public:
    RootSignature(uint32_t numRootParams = 0, uint32_t numStaticSamplers = 0);
    ~RootSignature(){};

    void Reset(uint32_t numRootParams, uint32_t numStaticSamplers);

    void Destroy();

    void Finalize(ID3D12Device6* device, D3D12_ROOT_SIGNATURE_FLAGS flags = D3D12_ROOT_SIGNATURE_FLAG_NONE,
                  D3D_ROOT_SIGNATURE_VERSION version = D3D_ROOT_SIGNATURE_VERSION_1_1);

    void InitStaticSampler(UINT shaderRegister, const D3D12_SAMPLER_DESC& nonStaticSamplerDesc,
                           D3D12_SHADER_VISIBILITY visibility);

    Microsoft::WRL::ComPtr<ID3D12RootSignature> GetRootSignature() const { return m_RootSignature; }
    const D3D12_ROOT_SIGNATURE_DESC1&           GetRootSignatureDesc() const { return m_RootSignatureDesc; }
    uint32_t                                    GetDescriptorTableBitMask(D3D12_DESCRIPTOR_HEAP_TYPE type) const {
        return type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER ? m_SamplerTableBitMask : m_DescriptorTableBitMask;
    }
    uint32_t GetNumDescriptors(uint32_t rootIndex) const {
        assert(rootIndex < 32);
        return m_NumDescriptorsPerTable[rootIndex];
    };

    RootParameter& operator[](size_t index) {
        assert(index < m_NumParameters);
        return m_ParamArray.get()[index];
    }

    const RootParameter& operator[](size_t index) const {
        assert(index < m_NumParameters);
        return m_ParamArray.get()[index];
    }

private:
    bool m_Finalized;

    ID3D12Device6*                              m_Device;
    D3D12_ROOT_SIGNATURE_DESC1                  m_RootSignatureDesc;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_RootSignature;

    uint32_t m_NumDescriptorsPerTable[32];
    unsigned m_NumParameters;
    unsigned m_NumSamplers;
    unsigned m_NumInitializedStaticSamplers;

    std::unique_ptr<RootParameter[]>             m_ParamArray;
    std::unique_ptr<D3D12_STATIC_SAMPLER_DESC[]> m_SamplerArray;

    uint32_t m_SamplerTableBitMask;
    uint32_t m_DescriptorTableBitMask;
};

}  // namespace Hitagi::Graphics::DX12