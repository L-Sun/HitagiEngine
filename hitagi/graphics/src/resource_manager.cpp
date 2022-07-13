#include <hitagi/graphics/resource_manager.hpp>

#include <hitagi/core/file_io_manager.hpp>

#include <magic_enum.hpp>

using namespace hitagi::resource;
using namespace hitagi::math;

namespace hitagi::graphics {

ResourceManager::ResourceManager(DriverAPI& driver)
    : m_Driver(driver) {}

void ResourceManager::PrepareVertexBuffer(const std::shared_ptr<VertexArray>& vertices) {
    auto id = vertices->GetGuid();
    if (m_VertexBuffer.count(id) == 0) {
        m_VertexBuffer.emplace(id, m_Driver.CreateVertexBuffer(vertices));
        m_VersionsInfo[id] = vertices->Version();
    } else if (auto& vb = m_VertexBuffer.at(id); vertices->Version() > m_VersionsInfo.at(id)) {
        if (vb->desc.vertex_count < vertices->VertexCount() || vb->desc.attr_mask != vertices->GetAttributeMask()) {
            vb = m_Driver.CreateVertexBuffer(vertices);

        } else {
            auto context = m_Driver.GetGraphicsCommandContext();
            magic_enum::enum_for_each<resource::VertexAttribute>([&](auto attr) {
                std::size_t i = magic_enum::enum_integer(attr());
                if (vb->desc.attr_mask.test(i)) {
                    const auto& buffer = vertices->GetBuffer(attr());

                    context->UpdateBuffer(
                        vb,
                        vb->desc.attr_offset[i],
                        buffer.GetData(),
                        buffer.GetDataSize());

                    vb->desc.vertex_count = vertices->VertexCount();
                }
            });

            context->Finish(true);
        }

        m_VersionsInfo[id] = vertices->Version();
    } else {
        assert(vertices->Version() == m_VersionsInfo.at(id));
    }
}

void ResourceManager::PrepareIndexBuffer(const std::shared_ptr<IndexArray>& indices) {
    auto id = indices->GetGuid();
    if (m_IndexBuffer.count(id) == 0) {
        m_IndexBuffer.emplace(id, m_Driver.CreateIndexBuffer(indices));
        m_VersionsInfo[id] = indices->Version();
    } else if (auto& ib = m_IndexBuffer.at(id);
               indices->Version() > m_VersionsInfo.at(id)) {
        if (ib->desc.index_count < indices->IndexCount() ||
            ib->desc.index_size != indices->IndexSize()) {
            ib = m_Driver.CreateIndexBuffer(indices);
        } else {
            auto context = m_Driver.GetGraphicsCommandContext();
            context->UpdateBuffer(ib, 0, indices->Buffer().GetData(), indices->Buffer().GetDataSize());
            context->Finish(true);
            ib->desc.index_count = indices->IndexCount();
        }
        m_VersionsInfo[id] = indices->Version();
    } else {
        assert(indices->Version() == m_VersionsInfo.at(id));
    }
}

void ResourceManager::PrepareTextureBuffer(const std::shared_ptr<Texture>& texture) {
    auto id = texture->GetGuid();
    if (m_TextureBuffer.count(id) == 0) {
        m_TextureBuffer.emplace(id, m_Driver.CreateTextureBuffer(texture));
        m_VersionsInfo[id] = texture->Version();
    } else if (auto& tb = m_TextureBuffer.at(id);
               texture->Version() > m_VersionsInfo.at(id)) {
        tb                 = m_Driver.CreateTextureBuffer(texture);
        m_VersionsInfo[id] = texture->Version();
    } else {
        assert(texture->Version() == m_VersionsInfo.at(id));
    }
}

void ResourceManager::PrepareMaterial(const std::shared_ptr<resource::Material>& material) {
    PrepareMaterialParameterBuffer(material);
    PreparePipeline(material);
}

void ResourceManager::PrepareMaterialParameterBuffer(const std::shared_ptr<Material>& material) {
    if (material->GetParametersSize() == 0) return;

    auto id = material->GetGuid();
    if (m_MaterialParameterBuffer.count(id) == 0) {
        m_MaterialParameterBuffer.emplace(
            id,
            m_Driver.CreateConstantBuffer(material->GetUniqueName(), {material->GetNumInstances(), material->GetParametersSize()}));
        m_VersionsInfo[id] = material->Version();

    } else if (auto& mb = m_MaterialParameterBuffer.at(id);
               material->Version() > m_VersionsInfo.at(id)) {
        mb                 = m_Driver.CreateConstantBuffer(material->GetUniqueName(), {material->GetNumInstances(), material->GetParametersSize()});
        m_VersionsInfo[id] = material->Version();

    } else {
        assert(material->Version() == m_VersionsInfo.at(id));
    }
}

void ResourceManager::PreparePipeline(const std::shared_ptr<Material>& material) {
    auto id = material->GetGuid();
    if (m_PipelineStates.count(id) == 0) {
        RootSignature::Builder rootsig_builder;
        rootsig_builder
            .Add(FRAME_CONSTANT_BUFFER, ShaderVariableType::CBV, 0, 0)
            .Add(OBJECT_CONSTANT_BUFFER, ShaderVariableType::CBV, 1, 0);

        if (material->GetParametersSize() != 0) {
            rootsig_builder.Add(MATERIAL_CONSTANT_BUFFER, ShaderVariableType::CBV, 2, 0);
        }

        {
            std::size_t i = 0;
            for (auto& texture_name : material->GetTextureNames()) {
                rootsig_builder.Add(texture_name, ShaderVariableType::SRV, i++, 0);
            }
        }

        auto shader_path = material->GetShaderPath();

        PipelineState::Builder pso_builder;
        // TODO more universe impletement
        if (material->GetName() == "imgui") {
            rootsig_builder.AddStaticSampler(
                material->GetName(),
                SamplerDesc{
                    .filter         = Filter::Min_Mag_Mip_Linear,
                    .address_u      = TextureAddressMode::Wrap,
                    .address_v      = TextureAddressMode::Wrap,
                    .address_w      = TextureAddressMode::Wrap,
                    .mip_lod_bias   = 0.0f,
                    .max_anisotropy = 0,
                    .comp_func      = ComparisonFunc::Always,
                    .border_color   = vec4f(0.0f, 0.0f, 0.0f, 1.0f),
                    .min_lod        = 0.0f,
                    .max_lod        = 0.0f,
                },
                0, 0, ShaderVisibility::Pixel);
            pso_builder
                .SetBlendState(BlendDescription{
                    .alpha_to_coverage_enable = false,
                    .enable_blend             = true,
                    .src_blend                = Blend::SrcAlpha,
                    .dest_blend               = Blend::InvSrcAlpha,
                    .blend_op                 = BlendOp::Add,
                    .src_blend_alpha          = Blend::One,
                    .dest_blend_alpha         = Blend::InvSrcAlpha,
                    .blend_op_alpha           = BlendOp::Add,
                });
        }

        // TODO more infomation
        pso_builder
            .SetName(material->GetName())
            .SetVertexShader(g_FileIoManager->SyncOpenAndReadBinary(shader_path.replace_extension("vs")))
            .SetPixelShader(g_FileIoManager->SyncOpenAndReadBinary(shader_path.replace_extension("ps")))
            .SetRootSignautre(rootsig_builder.Create(m_Driver))
            .SetRenderFormat(Format::R8G8B8A8_UNORM)
            .SetPrimitiveType(material->GetPrimitiveType());

        m_PipelineStates.emplace(id, pso_builder.Build(m_Driver));
    }
}

std::shared_ptr<VertexBuffer> ResourceManager::GetVertexBuffer(xg::Guid vertex_array_id) const noexcept {
    if (m_VertexBuffer.count(vertex_array_id) != 0) {
        return m_VertexBuffer.at(vertex_array_id);
    }
    return nullptr;
}
std::shared_ptr<IndexBuffer> ResourceManager::GetIndexBuffer(xg::Guid index_array_id) const noexcept {
    if (m_IndexBuffer.count(index_array_id) != 0) {
        return m_IndexBuffer.at(index_array_id);
    }
    return nullptr;
}
std::shared_ptr<TextureBuffer> ResourceManager::GetTextureBuffer(xg::Guid texture_id) const noexcept {
    if (m_TextureBuffer.count(texture_id) != 0) {
        return m_TextureBuffer.at(texture_id);
    }
    return nullptr;
}
std::shared_ptr<Sampler> ResourceManager::GetSampler(xg::Guid sampler_id) const noexcept {
    if (m_Samplers.count(sampler_id) != 0) {
        return m_Samplers.at(sampler_id);
    }
    return nullptr;
}
std::shared_ptr<ConstantBuffer> ResourceManager::GetMaterialParameterBuffer(xg::Guid material_id) const noexcept {
    if (m_MaterialParameterBuffer.count(material_id) != 0) {
        return m_MaterialParameterBuffer.at(material_id);
    }
    return nullptr;
}

std::shared_ptr<PipelineState> ResourceManager::GetPipelineState(xg::Guid material_id) const noexcept {
    if (m_PipelineStates.count(material_id) != 0) {
        return m_PipelineStates.at(material_id);
    }
    return nullptr;
}

}  // namespace hitagi::graphics