#pragma once
#include "DriverAPI.hpp"
#include "SceneObject.hpp"

namespace Hitagi::Graphics {
class ResourceManager {
public:
    ResourceManager(DriverAPI& driver) : m_Driver(driver) {}

    // Get mesh buffer from gpu. It will create new buffer in gpu if the mesh is not in gpu.
    const MeshBuffer&    GetMeshBuffer(Asset::SceneObjectMesh& mesh);
    const TextureBuffer& GetTextureBuffer(Asset::SceneObjectTexture& texture);
    const TextureBuffer& GetDefaultTextureBuffer(Format format);
    const Sampler&       GetSampler(std::string_view name);

private:
    DriverAPI&                                  m_Driver;
    std::unordered_map<xg::Guid, MeshBuffer>    m_MeshBuffer;
    std::unordered_map<xg::Guid, TextureBuffer> m_TextureBuffer;
    std::unordered_map<std::string, Sampler>    m_Samplers;

    std::unordered_map<Format, TextureBuffer> m_DefaultTextureBuffer;
};
}  // namespace Hitagi::Graphics