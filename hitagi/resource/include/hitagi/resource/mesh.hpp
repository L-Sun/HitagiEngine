#pragma once
#include "crossguid/guid.hpp"
#include "scene_object.hpp"
#include "enums.hpp"

#include <bitset>
#include <cstdint>
#include <hitagi/core/buffer.hpp>

#include <magic_enum.hpp>
#include <crossguid/guid.hpp>
#include <memory>
#include <spdlog/spdlog.h>

namespace hitagi::resource {

class VertexArray {
public:
    struct AttributeInfo {
        bool         enabled      = false;
        std::uint8_t buffer_index = 0;
        std::size_t  offset       = 0;
    };
    class Builder {
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

private:
    std::size_t                                                                        m_VertexCount = 0;
    std::array<AttributeInfo, static_cast<std::uint8_t>(VertexAttribute::Num_Support)> m_AttributeInfo{};
    std::pmr::vector<core::Buffer>                                                     m_Buffers;
    std::pmr::vector<std::size_t>                                                      m_BufferStrides;
};

class IndexArray {
public:
    class Builder {
    public:
        Builder&                    Count(std::size_t count) noexcept;
        Builder&                    Type(IndexType type) noexcept;
        std::shared_ptr<IndexArray> Build();

    private:
        IndexType   index_type  = IndexType::UINT32;
        std::size_t index_count = 0;
    };

    auto  GetIndexSize() const noexcept { return get_index_type_size(m_IndexType); }
    auto  GetIndexCount() const noexcept { return m_IndexCount; }
    auto& Buffer() const noexcept { return m_Data; }

private:
    IndexType    m_IndexType  = IndexType::UINT32;
    std::size_t  m_IndexCount = 0;
    core::Buffer m_Data;
};

class Mesh : public SceneObject {
public:
    Mesh(std::unique_ptr<VertexArray> vertices, std::unique_ptr<IndexArray> indices, PrimitiveType type);

    inline auto& Vertices() const noexcept { return *m_Vertices; }
    inline auto& Indices() const noexcept { return *m_Indices; }
    inline auto  Primitive() const noexcept { return m_PrimitiveType; }

private:
    std::unique_ptr<VertexArray> m_Vertices;
    std::unique_ptr<IndexArray>  m_Indices;
    PrimitiveType                m_PrimitiveType;
};

}  // namespace hitagi::resource