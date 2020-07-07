#pragma once
#include "DriverAPI.hpp"
#include "SceneObject.hpp"

namespace Hitagi::Graphics {
class ResourceManager {
public:
    ResourceManager(backend::DriverAPI& driver) : m_Driver(driver) {}

    const MeshBuffer&    GetMeshBuffer(Asset::SceneObjectMesh& mesh);
    const TextureBuffer& GetTextureBuffer(Asset::SceneObjectTexture& texture);

private:
    backend::DriverAPI&                         m_Driver;
    std::unordered_map<xg::Guid, MeshBuffer>    m_MeshBuffer;
    std::unordered_map<xg::Guid, TextureBuffer> m_TextureBuffer;
};
}  // namespace Hitagi::Graphics