#include "pso.hpp"
#include "utils.hpp"

#include <spdlog/spdlog.h>
#include <magic_enum.hpp>

namespace hitagi::graphics::backend::DX12 {
void PSO::CreateRootSignature(const std::shared_ptr<resource::Shader>& shader) {
    auto hr = D3D12CreateVersionedRootSignatureDeserializer(
        shader->Program().GetData(),
        shader->Program().GetDataSize(),
        IID_PPV_ARGS(&m_RootSignatureDeserializer));

    if (SUCCEEDED(hr)) {
        const D3D12_VERSIONED_ROOT_SIGNATURE_DESC* p_desc = nullptr;
        m_RootSignatureDeserializer->GetRootSignatureDescAtVersion(D3D_ROOT_SIGNATURE_VERSION_1_1, &p_desc);
        assert(p_desc->Version == D3D_ROOT_SIGNATURE_VERSION_1_1);
        m_RootSignatureDesc = p_desc->Desc_1_1;

        hr = m_Device->CreateRootSignature(
            0,
            shader->Program().GetData(),
            shader->Program().GetDataSize(),
            IID_PPV_ARGS(&m_RootSignature));
        assert(SUCCEEDED(hr));
    }
}

GraphicsPSO& GraphicsPSO::SetInputLayout(std::pmr::vector<D3D12_INPUT_ELEMENT_DESC> input_layout) {
    m_InputLayouts = std::move(input_layout);
    return *this;
}

GraphicsPSO& GraphicsPSO::SetBlendState(const D3D12_BLEND_DESC& desc) {
    m_PSODesc.BlendState = desc;
    return *this;
}
GraphicsPSO& GraphicsPSO::SetRasterizerState(const D3D12_RASTERIZER_DESC& desc) {
    m_PSODesc.RasterizerState = desc;
    return *this;
}
GraphicsPSO& GraphicsPSO::SetDepthStencilState(const D3D12_DEPTH_STENCIL_DESC& desc) {
    m_PSODesc.DepthStencilState = desc;
    return *this;
}
GraphicsPSO& GraphicsPSO::SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE type) {
    m_PSODesc.PrimitiveTopologyType = type;
    return *this;
}
GraphicsPSO& GraphicsPSO::SetSampleMask(unsigned mask) {
    m_PSODesc.SampleMask = mask;
    return *this;
}

GraphicsPSO& GraphicsPSO::SetShader(const std::shared_ptr<resource::Shader>& shader) {
    if (shader == nullptr || shader->Program().Empty()) return *this;

    if (m_RootSignature == nullptr) {
        CreateRootSignature(shader);
    }

    D3D12_SHADER_BYTECODE code{
        .pShaderBytecode = shader->Program().GetData(),
        .BytecodeLength  = shader->Program().GetDataSize(),
    };
    switch (shader->GetType()) {
        case resource::Shader::Type::Vertex:
            m_PSODesc.VS = code;
            break;
        case resource::Shader::Type::Pixel:
            m_PSODesc.PS = code;
            break;
        case resource::Shader::Type::Geometry:
            m_PSODesc.GS = code;
            break;
        default: {
            auto logger = spdlog::get("GraphicsManager");
            logger->warn("Unsupport shader {}!", magic_enum::enum_name(shader->GetType()));
        }
    }

    return *this;
}

GraphicsPSO& GraphicsPSO::SetRenderTargetFormats(
    const std::pmr::vector<DXGI_FORMAT>& rtv_formats, DXGI_FORMAT dsv_format, unsigned msaa_count, unsigned msaa_quality) {
    for (int i = 0; i < rtv_formats.size(); i++) m_PSODesc.RTVFormats[i] = rtv_formats[i];
    for (int i = rtv_formats.size(); i < m_PSODesc.NumRenderTargets; i++) m_PSODesc.RTVFormats[i] = DXGI_FORMAT_UNKNOWN;
    m_PSODesc.NumRenderTargets   = rtv_formats.size();
    m_PSODesc.DSVFormat          = dsv_format;
    m_PSODesc.SampleDesc.Count   = msaa_count;
    m_PSODesc.SampleDesc.Quality = msaa_quality;

    return *this;
}

void GraphicsPSO::Finalize() {
    m_PSODesc.InputLayout    = {m_InputLayouts.data(), static_cast<UINT>(m_InputLayouts.size())};
    m_PSODesc.pRootSignature = m_RootSignature.Get();
    assert(m_PSODesc.pRootSignature != nullptr);
    ThrowIfFailed(m_Device->CreateGraphicsPipelineState(&m_PSODesc, IID_PPV_ARGS(&m_PSO)));
    m_PSO->SetName(std::wstring(m_Name.begin(), m_Name.end()).data());
}

int GraphicsPSO::GetAttributeSlot(resource::VertexAttribute attr) const {
    for (const auto& input_element : m_InputLayouts) {
        if (input_element.SemanticName == hlsl_semantic_name(attr) &&
            input_element.SemanticIndex == hlsl_semantic_index(attr) &&
            input_element.Format == hlsl_semantic_format(attr)) {
            return input_element.InputSlot;
        }
    }
    return -1;
}

ComputePSO& ComputePSO::SetComputeShader(const std::shared_ptr<resource::Shader>& shader) {
    if (shader == nullptr || shader->Program().Empty()) return *this;

    if (auto type = shader->GetType();
        type != resource::Shader::Type::Compute) {
        auto logger = spdlog::get("GraphicsManager");
        logger->error("ComputePipeline only accepts compute shader, but get {} shader", magic_enum::enum_name(type));
        return *this;
    }
    m_PSODesc.CS.pShaderBytecode = shader->Program().GetData();
    m_PSODesc.CS.BytecodeLength  = shader->Program().GetDataSize();

    return *this;
}

void ComputePSO::Finalize(ID3D12Device* device) {
    m_PSODesc.NodeMask = 1;

    // Make sure the root signature is finalized first
    assert(m_PSODesc.pRootSignature != nullptr);
    ThrowIfFailed(device->CreateComputePipelineState(&m_PSODesc, IID_PPV_ARGS(&m_PSO)));
}

}  // namespace hitagi::graphics::backend::DX12