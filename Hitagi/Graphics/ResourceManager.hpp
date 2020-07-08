#pragma once
#include "DriverAPI.hpp"
#include "SceneObject.hpp"

namespace Hitagi::Graphics {
class ResourceManager {
public:
    ResourceManager(backend::DriverAPI& driver) : m_Driver(driver) {}

    const MeshBuffer&     GetMeshBuffer(Asset::SceneObjectMesh& mesh);
    const TextureBuffer&  GetTextureBuffer(Asset::SceneObjectTexture& texture);
    const TextureBuffer&  GetDefaultTextureBuffer(Format format);
    const TextureSampler& GetSampler(std::string_view name);

private:
    backend::DriverAPI&                             m_Driver;
    std::unordered_map<xg::Guid, MeshBuffer>        m_MeshBuffer;
    std::unordered_map<xg::Guid, TextureBuffer>     m_TextureBuffer;
    std::unordered_map<std::string, TextureSampler> m_Samplers;

    std::unordered_map<Format, TextureBuffer> m_DefaultTextureBuffer;
};
}  // namespace Hitagi::Graphics