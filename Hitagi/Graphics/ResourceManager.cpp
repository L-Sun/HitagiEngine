#include "ResourceManager.hpp"

#include <spdlog/spdlog.h>
#include <fmt/format.h>

namespace Hitagi::Graphics {
std::shared_ptr<MeshBuffer> ResourceManager::GetMeshBuffer(const Asset::SceneObjectMesh& mesh) {
    auto id = mesh.GetGuid();
    if (m_MeshBuffer.count(id) != 0)
        return m_MeshBuffer.at(id);

    auto result = std::make_shared<MeshBuffer>();

    // Create new vertex array
    for (auto&& vertex : mesh.GetVertexArrays()) {
        std::string_view name = vertex.GetAttributeName();
        result->vertices.emplace(
            name,                         // attribut name
            m_Driver.CreateVertexBuffer(  // backend vertex buffer
                fmt::format("{}-{}", name, id.str()),
                vertex.GetVertexCount(),
                vertex.GetVertexSize(),
                vertex.GetData()));
    }
    // Create Index array
    auto& index_array = mesh.GetIndexArray();
    result->indices   = m_Driver.CreateIndexBuffer(
          fmt::format("index-{}", id.str()),
          index_array.GetIndexCount(),
          index_array.GetIndexSize(),
          index_array.GetData());

    result->primitive = mesh.GetPrimitiveType();

    m_MeshBuffer.emplace(id, result);

    return result;
}

std::shared_ptr<TextureBuffer> ResourceManager::GetTextureBuffer(const Asset::SceneObjectTexture& texture) {
    auto id = texture.GetGuid();
    if (m_TextureBuffer.count(id) != 0)
        return m_TextureBuffer.at(id);

    auto image = texture.GetTextureImage();
    if (!image) return GetDefaultTextureBuffer(Format::R8G8B8A8_UNORM);

    Format format;
    if (auto bit_count = image->GetBitcount(); bit_count == 8)
        format = Format::R8_UNORM;
    else if (bit_count == 16)
        format = Format::R8G8_UNORM;
    else if (bit_count == 24)
        format = Format::R8G8B8A8_UNORM;
    else if (bit_count == 32)
        format = Format::R8G8B8A8_UNORM;
    else
        format = Format::UNKNOWN;

    // Create new texture buffer
    TextureBuffer::Description desc = {};
    desc.format                     = format;
    desc.width                      = image->GetWidth();
    desc.height                     = image->GetHeight();
    desc.pitch                      = image->GetPitch();
    desc.initial_data               = image->GetData();
    desc.initial_data_size          = image->GetDataSize();

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
    desc.pitch                      = get_format_bit_size(format);
    desc.sample_count               = 1;
    desc.sample_quality             = 0;
    m_DefaultTextureBuffer.emplace(format, m_Driver.CreateTextureBuffer("Default Texture", desc));
    return m_DefaultTextureBuffer.at(format);
}

std::shared_ptr<Sampler> ResourceManager::GetSampler(std::string_view name) {
    const std::string search_name(name);
    if (m_Samplers.count(search_name) != 0) return m_Samplers.at(search_name);

    // TODO should throw error
    auto&& [iter, success] = m_Samplers.emplace(search_name, m_Driver.CreateSampler(name, {}));
    return iter->second;
}

}  // namespace Hitagi::Graphics