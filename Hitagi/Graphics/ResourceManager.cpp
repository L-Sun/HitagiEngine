#include "ResourceManager.hpp"

namespace Hitagi::Graphics {
const MeshBuffer& ResourceManager::GetMeshBuffer(Asset::SceneObjectMesh& mesh) {
    auto id = mesh.GetGuid();
    if (m_MeshBuffer.count(id) != 0)
        return m_MeshBuffer.at(id);

    // Create new vertex array
    for (auto&& vertex : mesh.GetVertexArrays()) {
        m_MeshBuffer[id].vertices.emplace(
            vertex.GetAttributeName(),    // attribut name
            m_Driver.CreateVertexBuffer(  // backend vertex buffer
                vertex.GetVertexCount(),
                vertex.GetVertexSize(),
                vertex.GetData()));
    }
    // Create Index array
    auto& indexArray         = mesh.GetIndexArray();
    m_MeshBuffer[id].indices = m_Driver.CreateIndexBuffer(
        indexArray.GetIndexCount(),
        indexArray.GetIndexSize(),
        indexArray.GetData());
    m_MeshBuffer[id].primitive = mesh.GetPrimitiveType();

    return m_MeshBuffer[id];
}

const TextureBuffer& ResourceManager::GetTextureBuffer(Asset::SceneObjectTexture& texture) {
    auto id = texture.GetGuid();
    if (m_TextureBuffer.count(id) != 0)
        return m_TextureBuffer.at(id);

    auto&  image = texture.GetTextureImage();
    Format format;
    if (auto bitCount = image.GetBitcount(); bitCount == 8)
        format = Format::R8_UNORM;
    else if (bitCount == 16)
        format = Format::R8G8_UNORM;
    else if (bitCount == 24)
        format = Format::R8G8B8A8_UNORM;
    else if (bitCount == 32)
        format = Format::R8G8B8A8_UNORM;
    else
        format = Format::UNKNOWN;

    // Create new texture buffer
    TextureBuffer::Description desc = {};
    desc.format                     = format;
    desc.width                      = image.GetWidth();
    desc.height                     = image.GetHeight();
    desc.pitch                      = image.GetPitch();
    desc.initialData                = image.GetData();
    desc.initialDataSize            = image.GetDataSize();

    m_TextureBuffer[id] = m_Driver.CreateTextureBuffer(texture.GetName(), desc);
    return m_TextureBuffer.at(id);
}

}  // namespace Hitagi::Graphics