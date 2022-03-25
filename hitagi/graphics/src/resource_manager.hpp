#pragma once
#include <hitagi/graphics/driver_api.hpp>
#include <hitagi/resource/geometry.hpp>
#include <hitagi/gui/gui_manager.hpp>

namespace hitagi::graphics {
class ResourceManager {
public:
    ResourceManager(DriverAPI& driver) : m_Driver(driver) {}

    // Get mesh buffer from gpu. It will create new buffer in gpu if the mesh is not in gpu.
    std::shared_ptr<MeshBuffer>    GetMeshBuffer(const asset::Mesh& mesh);
    std::shared_ptr<TextureBuffer> GetTextureBuffer(std::shared_ptr<asset::Image> image);
    std::shared_ptr<Sampler>       GetSampler(std::string_view name);
    std::shared_ptr<TextureBuffer> GetDefaultTextureBuffer(Format format);

    using ImGuiMeshBuilder = std::function<void(std::shared_ptr<hitagi::graphics::MeshBuffer>, ImDrawList*, const ImDrawCmd&)>;
    void MakeImGuiMesh(ImDrawData* data, ImGuiMeshBuilder&& builder);

private:
    DriverAPI&                                                   m_Driver;
    std::unordered_map<xg::Guid, std::shared_ptr<MeshBuffer>>    m_MeshBuffer;
    std::unordered_map<xg::Guid, std::shared_ptr<TextureBuffer>> m_TextureBuffer;
    std::unordered_map<std::string, std::shared_ptr<Sampler>>    m_Samplers;

    std::unordered_map<Format, std::shared_ptr<TextureBuffer>> m_DefaultTextureBuffer;

    std::shared_ptr<VertexBuffer> m_ImGuiVertexBuffer;
    std::shared_ptr<IndexBuffer>  m_ImGuiIndexBuffer;
};
}  // namespace hitagi::graphics