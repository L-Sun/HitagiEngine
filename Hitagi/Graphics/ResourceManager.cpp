#include "ResourceManager.hpp"

#include <spdlog/spdlog.h>
#include <fmt/format.h>

namespace Hitagi::Graphics {
std::shared_ptr<MeshBuffer> ResourceManager::GetMeshBuffer(const Asset::SceneObjectMesh& mesh) {
    auto id = mesh.GetGuid();
    if (m_MeshBuffer.count(id) != 0)
        return m_MeshBuffer.at(id);

    decltype(MeshBuffer::vertices) vertices;

    // Create new vertex array
    for (auto&& vertex : mesh.GetVertexArrays()) {
        std::string_view name = vertex.GetAttributeName();
        vertices.emplace(
            name,                         // attribut name
            m_Driver.CreateVertexBuffer(  // backend vertex buffer
                fmt::format("{}-{}", name, id.str()),
                vertex.GetVertexCount(),
                vertex.GetVertexSize(),
                vertex.GetData()));
    }
    // Create Index array
    auto& indexArray = mesh.GetIndexArray();
    auto  indices    = m_Driver.CreateIndexBuffer(
        fmt::format("index-{}", id.str()),
        indexArray.GetIndexCount(),
        indexArray.GetIndexSize(),
        indexArray.GetData());

    m_MeshBuffer.emplace(id, std::make_shared<MeshBuffer>(vertices, indices, mesh.GetPrimitiveType()));

    return m_MeshBuffer.at(id);
}

std::shared_ptr<TextureBuffer> ResourceManager::GetTextureBuffer(const Asset::SceneObjectTexture& texture) {
    auto id = texture.GetGuid();
    if (m_TextureBuffer.count(id) != 0)
        return m_TextureBuffer.at(id);

    auto& image = texture.GetTextureImage();
    if (image.Empty()) {
        spdlog::get("GraphicsManager")->warn("Texture({}) is empty. There is nothing return!");
        return nullptr;
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

std::shared_ptr<TextureBuffer> ResourceManager::GetDefaultTextureBuffer(Format format) {
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

std::shared_ptr<Sampler> ResourceManager::GetSampler(std::string_view name) {
    const std::string _name(name);
    if (m_Samplers.count(_name) != 0) return m_Samplers.at(_name);

    // TODO should throw error
    m_Samplers.emplace(name, m_Driver.CreateSampler(name, {}));
    return m_Samplers.at(_name);
}

}  // namespace Hitagi::Graphics