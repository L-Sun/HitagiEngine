#include "resource_manager.hpp"

#include <hitagi/core/file_io_manager.hpp>

#include <magic_enum.hpp>

using namespace hitagi::resource;

namespace hitagi::graphics {

ResourceManager::ResourceManager(DriverAPI& driver)
    : m_Driver(driver) {}

void ResourceManager::PrepareVertexBuffer(const std::shared_ptr<VertexArray>& vertices) {
    auto id = vertices->GetGuid();
    if (m_VertexBuffer.count(id) == 0) {
        m_VertexBuffer.emplace(vertices->GetGuid(), m_Driver.CreateVertexBuffer(vertices));

    } else if (auto& vb = m_VertexBuffer.at(id);
               vertices->Version() > vb->Version()) {
        vb = m_Driver.CreateVertexBuffer(vertices);
    } else {
        assert(vertices->Version() == vb->Version());
    }
}

void ResourceManager::PrepareIndexBuffer(const std::shared_ptr<IndexArray>& indices) {
    auto id = indices->GetGuid();
    if (m_IndexBuffer.count(id) == 0) {
        m_IndexBuffer.emplace(indices->GetGuid(), m_Driver.CreateIndexBuffer(indices));

    } else if (auto& ib = m_IndexBuffer.at(id);
               indices->Version() > ib->Version()) {
        ib = m_Driver.CreateIndexBuffer(indices);

    } else {
        assert(indices->Version() == ib->Version());
    }
}

void ResourceManager::PrepareTextureBuffer(const std::shared_ptr<Texture>& texture) {
    auto id = texture->GetGuid();
    if (m_TextureBuffer.count(id) == 0) {
        m_TextureBuffer.emplace(texture->GetGuid(), m_Driver.CreateTextureBuffer(texture));

    } else if (auto& tb = m_TextureBuffer.at(id);
               texture->Version() > tb->Version()) {
        tb = m_Driver.CreateTextureBuffer(texture);
    } else {
        assert(texture->Version() == tb->Version());
    }
}

void ResourceManager::PrepareMaterial(const std::shared_ptr<resource::Material>& material) {
    PrepareMaterialParameterBuffer(material);
    PreparePipeline(material);
}

void ResourceManager::PrepareMaterialParameterBuffer(const std::shared_ptr<Material>& material) {
    auto id = material->GetGuid();
    if (m_MaterialParameterBuffer.count(id) == 0) {
        m_MaterialParameterBuffer.emplace(
            material->GetGuid(),
            m_Driver.CreateConstantBuffer(material->GetUniqueName(), {material->GetNumInstances(), material->GetParametersSize()}));

    } else if (auto& mb = m_MaterialParameterBuffer.at(id);
               material->Version() > mb->Version()) {
        mb = m_Driver.CreateConstantBuffer(material->GetUniqueName(), {material->GetNumInstances(), material->GetParametersSize()});
    } else {
        assert(material->Version() == mb->Version());
    }
}

void ResourceManager::PreparePipeline(const std::shared_ptr<Material>& material) {
    if (m_PipelineStates.count(material->GetGuid()) == 0) {
        RootSignature::Builder rootsig_builder;
        rootsig_builder
            .Add("FrameConstantBuffer", ShaderVariableType::CBV, 0, 0)
            .Add("ObjectConstantBuffer", ShaderVariableType::CBV, 1, 0)
            .Add("MaterialConstantBuffer", ShaderVariableType::CBV, 2, 0);  // TODO shader visibility

        {
            std::size_t i = 0;
            for (auto& texture_name : material->GetTextureNames()) {
                rootsig_builder.Add(texture_name, ShaderVariableType::SRV, i++, 0);
            }
        }

        auto shader_path = material->GetShaderPath();

        PipelineState::Builder pso_builder;
        // TODO more infomation
        pso_builder
            .SetName(material->GetName())
            .SetVertexShader(g_FileIoManager->SyncOpenAndReadBinary(shader_path.replace_extension("vs")))
            .SetPixelShader(g_FileIoManager->SyncOpenAndReadBinary(shader_path.replace_extension("ps")))
            .SetRootSignautre(rootsig_builder.Create(m_Driver))
            .SetPrimitiveType(material->GetPrimitiveType());

        magic_enum::enum_for_each<VertexAttribute>([&](auto slot) {
            if (material->IsSlotEnabled(slot)) {
                pso_builder.EnableVertexSlot(slot);
            }
        });
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