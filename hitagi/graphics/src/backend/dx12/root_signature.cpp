#include "root_signature.hpp"

#include <spdlog/spdlog.h>

namespace hitagi::graphics::backend::DX12 {
RootSignature::RootSignature(std::string_view name, uint32_t num_root_params, uint32_t num_static_samplers)
    : m_Name(name),
      m_RootSignature(nullptr),
      m_NumParameters(num_root_params),
      m_NumStaticSamplers(num_static_samplers),
      m_NumInitializedStaticSamplers(num_static_samplers) {
    Reset(num_root_params, num_static_samplers);
};

void RootSignature::Reset(uint32_t num_root_params, uint32_t num_static_samplers) {
    m_ParamArray.resize(num_root_params, RootParameter{});
    m_NumParameters = num_root_params;

    m_SamplerArray.resize(num_root_params, {});
    m_NumStaticSamplers            = num_static_samplers;
    m_NumInitializedStaticSamplers = 0;
}

void RootSignature::InitStaticSampler(UINT register_index, UINT space, const D3D12_SAMPLER_DESC& sampler_desc,
                                      D3D12_SHADER_VISIBILITY visibility) {
    assert(m_NumInitializedStaticSamplers < m_NumStaticSamplers);
    auto& desc = m_SamplerArray[m_NumInitializedStaticSamplers++];

    desc.Filter           = sampler_desc.Filter;
    desc.AddressU         = sampler_desc.AddressU;
    desc.AddressV         = sampler_desc.AddressV;
    desc.AddressW         = sampler_desc.AddressW;
    desc.MipLODBias       = sampler_desc.MipLODBias;
    desc.MaxAnisotropy    = sampler_desc.MaxAnisotropy;
    desc.ComparisonFunc   = sampler_desc.ComparisonFunc;
    desc.BorderColor      = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
    desc.MinLOD           = sampler_desc.MinLOD;
    desc.MaxLOD           = sampler_desc.MaxLOD;
    desc.ShaderRegister   = register_index;
    desc.RegisterSpace    = space,
    desc.ShaderVisibility = visibility;

    // Transform sampler border color to limited border color
    if (desc.AddressU == D3D12_TEXTURE_ADDRESS_MODE_BORDER ||
        desc.AddressV == D3D12_TEXTURE_ADDRESS_MODE_BORDER ||
        desc.AddressW == D3D12_TEXTURE_ADDRESS_MODE_BORDER) {
        if (
            (sampler_desc.BorderColor[0] == 0.0f &&
             sampler_desc.BorderColor[1] == 0.0f &&
             sampler_desc.BorderColor[2] == 0.0f &&
             sampler_desc.BorderColor[3] == 0.0f) ||
            // Opaque Black
            (sampler_desc.BorderColor[0] == 0.0f &&
             sampler_desc.BorderColor[1] == 0.0f &&
             sampler_desc.BorderColor[2] == 0.0f &&
             sampler_desc.BorderColor[3] == 1.0f) ||
            // Opaque White
            (sampler_desc.BorderColor[0] == 1.0f &&
             sampler_desc.BorderColor[1] == 1.0f &&
             sampler_desc.BorderColor[2] == 1.0f &&
             sampler_desc.BorderColor[3] == 1.0f)
            //
        ) {
            auto graphics_logger = spdlog::get("GraphicsManager");
            if (graphics_logger) {
                graphics_logger->warn(
                    "Sampler border color ({}, {}, {}, {}) does not match static sampler limitations",
                    sampler_desc.BorderColor[0], sampler_desc.BorderColor[1],
                    sampler_desc.BorderColor[2], sampler_desc.BorderColor[3]);
            }
        }

        if (sampler_desc.BorderColor[3] == 1.0f) {
            if (sampler_desc.BorderColor[0] == 1.0f)
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
    m_RootSignatureDesc.NumStaticSamplers = m_NumStaticSamplers;
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

}  // namespace hitagi::graphics::backend::DX12