#include "resource_manager.hpp"

using namespace hitagi::resource;

namespace hitagi::graphics {
void ResourceManager::PrepareVertexBuffer(const std::shared_ptr<VertexArray>& vertices) {
    auto id = vertices->GetGuid();
    if (m_VertexBuffer.count(id) == 0) {
        m_VertexBuffer.emplace(vertices->GetGuid(), m_Driver.CreateVertexBuffer(vertices));

    } else if (auto vb = m_VertexBuffer.at(id);
               vertices->Version() > vb->Version()) {
        m_VertexBuffer.at(id) = m_Driver.CreateVertexBuffer(vertices);
    } else {
        assert(vertices->Version() == vb->Version());
    }
}

void ResourceManager::PrepareIndexBuffer(const std::shared_ptr<IndexArray>& indices) {
    auto id = indices->GetGuid();
    if (m_IndexBuffer.count(id) == 0) {
        m_IndexBuffer.emplace(indices->GetGuid(), m_Driver.CreateIndexBuffer(indices));

    } else if (auto ib = m_IndexBuffer.at(id);
               indices->Version() > ib->Version()) {
        m_IndexBuffer.at(id) = m_Driver.CreateIndexBuffer(indices);

    } else {
        assert(indices->Version() == ib->Version());
    }
}

void ResourceManager::PrepareTextureBuffer(const std::shared_ptr<Texture>& texture) {
    auto id = texture->GetGuid();
    if (m_TextureBuffer.count(id) == 0) {
        m_TextureBuffer.emplace(texture->GetGuid(), m_Driver.CreateTextureBuffer(texture));

    } else if (auto tb = m_TextureBuffer.at(id);
               texture->Version() > tb->Version()) {
        m_TextureBuffer.at(id) = m_Driver.CreateTextureBuffer(texture);
    } else {
        assert(texture->Version() == tb->Version());
    }
}

void ResourceManager::PrepareMaterialParameterBuffer(const std::shared_ptr<Material>& material) {
    auto id = material->GetGuid();
    if (m_MaterialParameterBuffer.count(id) == 0) {
        m_MaterialParameterBuffer.emplace(
            material->GetGuid(),
            m_Driver.CreateConstantBuffer(material->GetUniqueName(), material->GetNumInstances(), material->GetParametersSize()));

    } else if (auto cb = m_VertexBuffer.at(id);
               material->Version() > cb->Version()) {
        m_VertexBuffer.at(id) = m_Driver.CreateConstantBuffer(material->GetUniqueName(), material->GetNumInstances(), material->GetParametersSize());
    } else {
        assert(material->Version() == cb->Version());
    }
}

void ResourceManager::PreparePipeline(const std::shared_ptr<Material>& material) {
    auto id = material->GetGuid();
    if (m_MaterialParameterBuffer.count(id) == 0) {
        m_MaterialParameterBuffer.emplace(
            material->GetGuid(),
            m_Driver.CreateConstantBuffer(material->GetUniqueName(), material->GetNumInstances(), material->GetParametersSize()));

    } else if (auto cb = m_VertexBuffer.at(id);
               material->Version() > cb->Version()) {
        m_VertexBuffer.at(id) = m_Driver.CreateConstantBuffer(material->GetUniqueName(), material->GetNumInstances(), material->GetParametersSize());
    } else {
        assert(material->Version() == cb->Version());
    }
}
}  // namespace hitagi::graphics