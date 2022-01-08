#pragma once
#include "DriverAPI.hpp"
#include "SceneObject.hpp"

namespace Hitagi::Graphics {
class ResourceManager {
public:
    ResourceManager(DriverAPI& driver) : m_Driver(driver) {}

    // Get mesh buffer from gpu. It will create new buffer in gpu if the mesh is not in gpu.
    std::shared_ptr<MeshBuffer>    GetMeshBuffer(const Asset::Mesh& mesh);
    std::shared_ptr<TextureBuffer> GetTextureBuffer(std::shared_ptr<Asset::Image> image);
    std::shared_ptr<Sampler>       GetSampler(std::string_view name);
    std::shared_ptr<TextureBuffer> GetDefaultTextureBuffer(Format format);

private:
    DriverAPI&                                                   m_Driver;
    std::unordered_map<xg::Guid, std::shared_ptr<MeshBuffer>>    m_MeshBuffer;
    std::unordered_map<xg::Guid, std::shared_ptr<TextureBuffer>> m_TextureBuffer;
    std::unordered_map<std::string, std::shared_ptr<Sampler>>    m_Samplers;

    std::unordered_map<Format, std::shared_ptr<TextureBuffer>> m_DefaultTextureBuffer;
};
}  // namespace Hitagi::Graphics