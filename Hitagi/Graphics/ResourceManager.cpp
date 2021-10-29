#include "ResourceManager.hpp"

#include <spdlog/spdlog.h>

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

    auto& image = texture.GetTextureImage();
    if (image.Empty()) {
        spdlog::get("GraphicsManager")->warn("Texture({}) is empty. Program will use default texture instead.");
        return GetDefaultTextureBuffer(Format::R8G8B8A8_UNORM);
    }
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

    m_TextureBuffer.emplace(id, m_Driver.CreateTextureBuffer(texture.GetName(), desc));
    return m_TextureBuffer.at(id);
}

const TextureBuffer& ResourceManager::GetDefaultTextureBuffer(Format format) {
    if (m_DefaultTextureBuffer.count(format) != 0)
        return m_DefaultTextureBuffer.at(format);

    TextureBuffer::Description desc = {};
    desc.format                     = format;
    desc.width                      = 100;
    desc.height                     = 100;
    desc.pitch                      = GetFormatBitSize(format);
    desc.sampleCount                = 1;
    desc.sampleQuality              = 0;
    m_DefaultTextureBuffer.emplace(format, m_Driver.CreateTextureBuffer("Default Texture", desc));
    return m_DefaultTextureBuffer.at(format);
}

const TextureSampler& ResourceManager::GetSampler(std::string_view name) {
    const std::string _name(name);
    if (m_Samplers.count(_name) != 0) return m_Samplers.at(_name);

    // TODO should throw error
    m_Samplers.emplace(name, m_Driver.CreateSampler(name, {}));
    return m_Samplers.at(_name);
}

}  // namespace Hitagi::Graphics