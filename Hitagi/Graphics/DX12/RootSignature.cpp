#include "RootSignature.hpp"

#include <memory>

#include "D3DCore.hpp"

namespace Hitagi::Graphics::DX12 {
RootSignature::RootSignature(uint32_t numRootParams, uint32_t numStaticSamplers)
    : m_RootSignature(nullptr),
      m_NumParameters(numRootParams),
      m_NumSamplers(numStaticSamplers),
      m_NumInitializedStaticSamplers(numStaticSamplers) {
    Reset(numRootParams, numStaticSamplers);
};

void RootSignature::Reset(uint32_t numRootParams, uint32_t numStaticSamplers) {
    m_ParamArray.resize(numRootParams, RootParameter{});
    m_NumParameters = numRootParams;

    m_SamplerArray.resize(numRootParams, {});
    m_NumSamplers                  = numStaticSamplers;
    m_NumInitializedStaticSamplers = 0;
}

void RootSignature::InitStaticSampler(UINT shaderRegister, const D3D12_SAMPLER_DESC& nonStaticSamplerDesc,
                                      D3D12_SHADER_VISIBILITY visibility) {
    assert(m_NumInitializedStaticSamplers < m_NumSamplers);
    auto& StaticSamplerDesc = m_SamplerArray[m_NumInitializedStaticSamplers++];

    StaticSamplerDesc.Filter           = nonStaticSamplerDesc.Filter;
    StaticSamplerDesc.AddressU         = nonStaticSamplerDesc.AddressU;
    StaticSamplerDesc.AddressV         = nonStaticSamplerDesc.AddressV;
    StaticSamplerDesc.AddressW         = nonStaticSamplerDesc.AddressW;
    StaticSamplerDesc.MipLODBias       = nonStaticSamplerDesc.MipLODBias;
    StaticSamplerDesc.MaxAnisotropy    = nonStaticSamplerDesc.MaxAnisotropy;
    StaticSamplerDesc.ComparisonFunc   = nonStaticSamplerDesc.ComparisonFunc;
    StaticSamplerDesc.BorderColor      = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
    StaticSamplerDesc.MinLOD           = nonStaticSamplerDesc.MinLOD;
    StaticSamplerDesc.MaxLOD           = nonStaticSamplerDesc.MaxLOD;
    StaticSamplerDesc.ShaderRegister   = shaderRegister;
    StaticSamplerDesc.RegisterSpace    = 0;
    StaticSamplerDesc.ShaderVisibility = visibility;

    if (StaticSamplerDesc.AddressU == D3D12_TEXTURE_ADDRESS_MODE_BORDER ||
        StaticSamplerDesc.AddressV == D3D12_TEXTURE_ADDRESS_MODE_BORDER ||
        StaticSamplerDesc.AddressW == D3D12_TEXTURE_ADDRESS_MODE_BORDER) {
        bool noMatch = false;

        do {
            if (nonStaticSamplerDesc.BorderColor[0] == 0.0f && nonStaticSamplerDesc.BorderColor[1] == 0.0f &&
                nonStaticSamplerDesc.BorderColor[2] == 0.0f && nonStaticSamplerDesc.BorderColor[3] == 0.0f)
                break;
            // Opaque Black
            if (nonStaticSamplerDesc.BorderColor[0] == 0.0f && nonStaticSamplerDesc.BorderColor[1] == 0.0f &&
                nonStaticSamplerDesc.BorderColor[2] == 0.0f && nonStaticSamplerDesc.BorderColor[3] == 1.0f)
                break;
            // Opaque White
            if (nonStaticSamplerDesc.BorderColor[0] == 1.0f && nonStaticSamplerDesc.BorderColor[1] == 1.0f &&
                nonStaticSamplerDesc.BorderColor[2] == 1.0f && nonStaticSamplerDesc.BorderColor[3] == 1.0f)
                break;
        } while (false);

        if (noMatch) {
            std::cout << "[Warning] Sampler border color does not match static sampler limitations" << std::endl;
        }

        if (nonStaticSamplerDesc.BorderColor[3] == 1.0f) {
            if (nonStaticSamplerDesc.BorderColor[0] == 1.0f)
                StaticSamplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
            else
                StaticSamplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
        } else
            StaticSamplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
    }
}

void RootSignature::Finalize(D3D12_ROOT_SIGNATURE_FLAGS flags,
                             D3D_ROOT_SIGNATURE_VERSION version) {
    if (m_Finalized) return;

    for (size_t i = 0; i < m_NumParameters; i++) {
        switch (m_ParamArray[i].ParameterType) {
            case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE:
                m_NumDescriptorTables++;
                break;
            case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS:
                m_NumRootConstants++;
                break;
            default:
                m_NumRootDescriptors++;
                break;
        }
    }

    m_RootSignatureDesc.NumParameters     = m_NumParameters;
    m_RootSignatureDesc.pParameters       = m_ParamArray.data();
    m_RootSignatureDesc.NumStaticSamplers = m_NumSamplers;
    m_RootSignatureDesc.pStaticSamplers   = m_SamplerArray.data();
    m_RootSignatureDesc.Flags             = flags;

    for (uint32_t rootIndex = 0; rootIndex < m_NumParameters; rootIndex++) {
        const auto& rootParam               = m_RootSignatureDesc.pParameters[rootIndex];
        m_NumDescriptorsPerTable[rootIndex] = 0;
        if (rootParam.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE) {
            assert(rootParam.DescriptorTable.pDescriptorRanges != nullptr);
            if (rootParam.DescriptorTable.pDescriptorRanges->RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER)
                m_SamplerTableBitMask |= (1 << rootIndex);
            else
                m_DescriptorTableBitMask |= (1 << rootIndex);

            for (UINT rangeIndex = 0; rangeIndex < rootParam.DescriptorTable.NumDescriptorRanges; rangeIndex++)
                m_NumDescriptorsPerTable[rootIndex] +=
                    rootParam.DescriptorTable.pDescriptorRanges[rangeIndex].NumDescriptors;
        }
    }

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC versionRootSignatureDesc(m_RootSignatureDesc);

    // Serialize the root signature.
    Microsoft::WRL::ComPtr<ID3DBlob> rootSignatureBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

    ThrowIfFailed(
        D3DX12SerializeVersionedRootSignature(&versionRootSignatureDesc, version, &rootSignatureBlob, &errorBlob));

    ThrowIfFailed(g_Device->CreateRootSignature(0, rootSignatureBlob->GetBufferPointer(),
                                                rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&m_RootSignature)));

    m_Finalized = true;
}

}  // namespace Hitagi::Graphics::DX12