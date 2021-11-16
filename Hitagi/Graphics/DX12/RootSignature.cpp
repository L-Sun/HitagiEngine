#include "RootSignature.hpp"

namespace Hitagi::Graphics::backend::DX12 {
RootSignature::RootSignature(std::string_view name, uint32_t num_root_params, uint32_t num_static_samplers)
    : m_Name(name),
      m_RootSignature(nullptr),
      m_NumParameters(num_root_params),
      m_NumSamplers(num_static_samplers),
      m_NumInitializedStaticSamplers(num_static_samplers) {
    Reset(num_root_params, num_static_samplers);
};

void RootSignature::Reset(uint32_t num_root_params, uint32_t num_static_samplers) {
    m_ParamArray.resize(num_root_params, RootParameter{});
    m_NumParameters = num_root_params;

    m_SamplerArray.resize(num_root_params, {});
    m_NumSamplers                  = num_static_samplers;
    m_NumInitializedStaticSamplers = 0;
}

void RootSignature::InitStaticSampler(UINT shader_register, const D3D12_SAMPLER_DESC& non_static_sampler_desc,
                                      D3D12_SHADER_VISIBILITY visibility) {
    assert(m_NumInitializedStaticSamplers < m_NumSamplers);
    auto& desc = m_SamplerArray[m_NumInitializedStaticSamplers++];

    desc.Filter           = non_static_sampler_desc.Filter;
    desc.AddressU         = non_static_sampler_desc.AddressU;
    desc.AddressV         = non_static_sampler_desc.AddressV;
    desc.AddressW         = non_static_sampler_desc.AddressW;
    desc.MipLODBias       = non_static_sampler_desc.MipLODBias;
    desc.MaxAnisotropy    = non_static_sampler_desc.MaxAnisotropy;
    desc.ComparisonFunc   = non_static_sampler_desc.ComparisonFunc;
    desc.BorderColor      = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
    desc.MinLOD           = non_static_sampler_desc.MinLOD;
    desc.MaxLOD           = non_static_sampler_desc.MaxLOD;
    desc.ShaderRegister   = shader_register;
    desc.RegisterSpace    = 0;
    desc.ShaderVisibility = visibility;

    if (desc.AddressU == D3D12_TEXTURE_ADDRESS_MODE_BORDER ||
        desc.AddressV == D3D12_TEXTURE_ADDRESS_MODE_BORDER ||
        desc.AddressW == D3D12_TEXTURE_ADDRESS_MODE_BORDER) {
        bool no_match = false;

        do {
            if (non_static_sampler_desc.BorderColor[0] == 0.0f && non_static_sampler_desc.BorderColor[1] == 0.0f &&
                non_static_sampler_desc.BorderColor[2] == 0.0f && non_static_sampler_desc.BorderColor[3] == 0.0f)
                break;
            // Opaque Black
            if (non_static_sampler_desc.BorderColor[0] == 0.0f && non_static_sampler_desc.BorderColor[1] == 0.0f &&
                non_static_sampler_desc.BorderColor[2] == 0.0f && non_static_sampler_desc.BorderColor[3] == 1.0f)
                break;
            // Opaque White
            if (non_static_sampler_desc.BorderColor[0] == 1.0f && non_static_sampler_desc.BorderColor[1] == 1.0f &&
                non_static_sampler_desc.BorderColor[2] == 1.0f && non_static_sampler_desc.BorderColor[3] == 1.0f)
                break;
        } while (false);

        if (no_match) {
            std::cout << "[Warning] Sampler border color does not match static sampler limitations" << std::endl;
        }

        if (non_static_sampler_desc.BorderColor[3] == 1.0f) {
            if (non_static_sampler_desc.BorderColor[0] == 1.0f)
                desc.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
            else
                desc.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
        } else
            desc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
    }
}

void RootSignature::Finalize(
    ID3D12Device*              device,
    D3D12_ROOT_SIGNATURE_FLAGS flags,
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

    for (uint32_t root_index = 0; root_index < m_NumParameters; root_index++) {
        const auto& root_param               = m_RootSignatureDesc.pParameters[root_index];
        m_NumDescriptorsPerTable[root_index] = 0;
        if (root_param.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE) {
            assert(root_param.DescriptorTable.pDescriptorRanges != nullptr);
            // sampler descriptor and cbv_srv_uav descriptor can not be in the same descriptor table.
            if (root_param.DescriptorTable.pDescriptorRanges->RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER)
                m_SamplerTableBitMask |= (1 << root_index);
            else
                m_DescriptorTableBitMask |= (1 << root_index);

            for (UINT range_index = 0; range_index < root_param.DescriptorTable.NumDescriptorRanges; range_index++)
                m_NumDescriptorsPerTable[root_index] +=
                    root_param.DescriptorTable.pDescriptorRanges[range_index].NumDescriptors;
        }
    }

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC version_root_signature_desc(m_RootSignatureDesc);

    // Serialize the root signature.
    Microsoft::WRL::ComPtr<ID3DBlob> root_signature_blob;
    Microsoft::WRL::ComPtr<ID3DBlob> error_blob;

    if (FAILED(D3DX12SerializeVersionedRootSignature(&version_root_signature_desc, version, &root_signature_blob, &error_blob))) {
        auto        buffer = reinterpret_cast<const char*>(error_blob->GetBufferPointer());
        auto        size   = error_blob->GetBufferSize();
        std::string info(buffer, size);
        std::cout << info << std::endl;
        throw std::runtime_error("Serialize Root RootSignature failed.");
    }

    ThrowIfFailed(device->CreateRootSignature(0, root_signature_blob->GetBufferPointer(),
                                              root_signature_blob->GetBufferSize(), IID_PPV_ARGS(&m_RootSignature)));

    m_RootSignature->SetName(std::wstring(m_Name.begin(), m_Name.end()).data());

    m_Finalized = true;
}

}  // namespace Hitagi::Graphics::backend::DX12