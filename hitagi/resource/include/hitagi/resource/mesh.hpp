#pragma once
#include "scene_object.hpp"
#include "enums.hpp"

#include <hitagi/core/buffer.hpp>
#include <hitagi/utils/private_build.hpp>

#include <memory>

namespace hitagi::resource {

class VertexArray : utils::enable_private_allocate_shared_build<VertexArray> {
public:
    struct AttributeInfo {
        bool         enabled      = false;
        std::uint8_t buffer_index = 0;
        std::size_t  offset       = 0;
    };
    class Builder {
        friend class VertexArray;

    public:
        Builder&                     BufferCount(std::uint8_t count) noexcept;
        Builder&                     VertexCount(std::size_t count) noexcept;
        Builder&                     AppendAttributeAt(std::uint8_t buffer_index, VertexAttribute attribute) noexcept;
        std::shared_ptr<VertexArray> Build();

    private:
        std::uint8_t                                                                       buffer_count;
        std::size_t                                                                        vertex_count;
        std::array<AttributeInfo, static_cast<std::uint8_t>(VertexAttribute::Num_Support)> attributes{};
        std::array<std::size_t, static_cast<std::uint8_t>(VertexAttribute::Num_Support)>   buffer_strides;
    };

    inline std::size_t VertexCount() const noexcept { return m_VertexCount; }
    auto&              GetBuffer(std::uint8_t index) { return m_Buffers.at(index); }
    std::size_t        GetStride(std::uint8_t index) const { return m_BufferStrides.at(index); }

protected:
    // VertexArray only can be create by Builder
    VertexArray(const Builder&);

private:
    std::size_t                                                                        m_VertexCount = 0;
    std::array<AttributeInfo, static_cast<std::uint8_t>(VertexAttribute::Num_Support)> m_AttributeInfo{};
    std::pmr::vector<core::Buffer>                                                     m_Buffers;
    std::pmr::vector<std::size_t>                                                      m_BufferStrides;
};

class IndexArray : utils::enable_private_allocate_shared_build<IndexArray> {
public:
    class Builder {
        friend class IndexArray;

    public:
        Builder&                    Count(std::size_t count) noexcept;
        Builder&                    Type(IndexType type) noexcept;
        std::shared_ptr<IndexArray> Build();

    private:
        IndexType   index_type  = IndexType::UINT32;
        std::size_t index_count = 0;
    };

    auto  IndexSize() const noexcept { return get_index_type_size(m_IndexType); }
    auto  IndexCount() const noexcept { return m_IndexCount; }
    auto& Buffer() const noexcept { return m_Buffer; }

protected:
    // IndexArray only can be create by Builder
    IndexArray(const Builder& builder);

private:
    IndexType    m_IndexType  = IndexType::UINT32;
    std::size_t  m_IndexCount = 0;
    core::Buffer m_Buffer;
};

class Mesh : public SceneObject {
public:
    Mesh(
        std::shared_ptr<VertexArray> vertices,
        std::shared_ptr<IndexArray>  indices,
        PrimitiveType                type         = PrimitiveType::TriangleList,
        std::size_t                  index_count  = 0,
        std::size_t                  index_start  = 0,
        std::size_t                  vertex_start = 0);

    inline auto Vertices() const noexcept { return *m_Vertices; }
    inline auto Indices() const noexcept { return *m_Indices; }
    inline auto Primitive() const noexcept { return m_PrimitiveType; }

    // void SetMaterial(std::shared_ptr<Material> material) { m_Material = material; }

private:
    std::shared_ptr<VertexArray> m_Vertices;
    std::shared_ptr<IndexArray>  m_Indices;
    PrimitiveType                m_PrimitiveType;
    std::size_t                  m_IndexCount;
    std::size_t                  m_IndexStart;
    std::size_t                  m_VertexStart;
    // std::shared_ptr<Material>    m_Material;
};

}  // namespace hitagi::resource