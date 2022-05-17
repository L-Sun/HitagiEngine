#pragma once
#include <hitagi/graphics/driver_api.hpp>

#include <hitagi/resource/mesh.hpp>
#include <hitagi/resource/texture.hpp>
#include <hitagi/resource/image.hpp>

#include <unordered_map>

namespace hitagi::graphics {
class ResourceManager {
public:
    ResourceManager(DriverAPI& driver);

    // prepare vertex buffer in gpu. It will create new buffer in gpu if the vertices is not in gpu.
    void PrepareVertexBuffer(const std::shared_ptr<resource::VertexArray>& vertices);

    // prepare index buffer in gpu. It will create new buffer in gpu if the indices is not in gpu.
    void PrepareIndexBuffer(const std::shared_ptr<resource::IndexArray>& indices);

    // prepare texture buffer in gpu. It will create new buffer in gpu if the textrue is not in gpu.
    void PrepareTextureBuffer(const std::shared_ptr<resource::Texture>& texture);

    // this function will prepare a constant buffer for storing all parameters from material instance
    void PrepareMaterialParameterBuffer(const std::shared_ptr<resource::Material>& material);

    // this function will prepare a pipeline state object in gpu.
    void PreparePipeline(const std::shared_ptr<resource::Material>& material);

    // TODO Sampler
    // void PrepareSampler();

    std::shared_ptr<VertexBuffer>   GetVertexBuffer(xg::Guid vertex_array_id) const noexcept;
    std::shared_ptr<IndexBuffer>    GetIndexBuffer(xg::Guid index_array_id) const noexcept;
    std::shared_ptr<TextureBuffer>  GetTextureBuffer(xg::Guid texture_id) const noexcept;
    std::shared_ptr<Sampler>        GetSampler(xg::Guid sampler_id) const noexcept;
    std::shared_ptr<ConstantBuffer> GetMaterialParameterBuffer(xg::Guid material_id) const noexcept;

    std::shared_ptr<PipelineState> GetPipelineState(xg::Guid material_id) const noexcept;

private:
    DriverAPI&                                                         m_Driver;
    std::pmr::unordered_map<xg::Guid, std::shared_ptr<VertexBuffer>>   m_VertexBuffer;
    std::pmr::unordered_map<xg::Guid, std::shared_ptr<IndexBuffer>>    m_IndexBuffer;
    std::pmr::unordered_map<xg::Guid, std::shared_ptr<TextureBuffer>>  m_TextureBuffer;
    std::pmr::unordered_map<xg::Guid, std::shared_ptr<Sampler>>        m_Samplers;
    std::pmr::unordered_map<xg::Guid, std::shared_ptr<ConstantBuffer>> m_MaterialParameterBuffer;
    std::pmr::unordered_map<xg::Guid, std::shared_ptr<PipelineState>>  m_PipelineStates;
};

}  // namespace hitagi::graphics