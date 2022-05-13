#pragma once
#include <hitagi/resource/scene_object.hpp>
#include <hitagi/resource/enums.hpp>
#include <hitagi/resource/material.hpp>
#include <hitagi/core/buffer.hpp>
#include <hitagi/utils/private_build.hpp>

#include <magic_enum.hpp>

#include <stdexcept>

namespace hitagi::resource {

class VertexArray {
public:
    using allocator_type = std::pmr::polymorphic_allocator<>;

    VertexArray(std::size_t vertex_count, allocator_type alloc = {});

    inline std::size_t VertexCount() const noexcept { return m_VertexCount; }
    bool               IsEnable(VertexAttribute attr) const;

    core::Buffer& GetBuffer(VertexAttribute attr);

    // the buffer will be constructed if `GetVertices` is invoke first time with attribute Attr
    template <VertexAttribute Attr>
    std::span<VertexType<Attr>> GetVertices();

private:
    std::size_t                                                         m_VertexCount = 0;
    std::array<core::Buffer, magic_enum::enum_count<VertexAttribute>()> m_Buffers;
};

class IndexArray : utils::enable_private_allocate_shared_build<IndexArray> {
public:
    using allocator_type = std::pmr::polymorphic_allocator<>;

    IndexArray(std::size_t index_count, IndexType type, allocator_type alloc = {});

    auto  IndexSize() const noexcept { return get_index_type_size(m_IndexType); }
    auto  IndexCount() const noexcept { return m_IndexCount; }
    auto& Buffer() const noexcept { return m_Buffer; }

    // The template parameter need to be same with the type used by constructor
    template <IndexType T>
    std::span<IndexDataType<T>> GetIndices();

private:
    std::size_t  m_IndexCount = 0;
    IndexType    m_IndexType  = IndexType::UINT32;
    core::Buffer m_Buffer;
};

struct Mesh : public SceneObject {
public:
    Mesh(
        std::shared_ptr<VertexArray> vertices,
        std::shared_ptr<IndexArray>  indices,
        // TODO use default material
        std::shared_ptr<MaterialInstance> material     = nullptr,
        PrimitiveType                     type         = PrimitiveType::TriangleList,
        std::size_t                       index_count  = 0,
        std::size_t                       index_start  = 0,
        std::size_t                       vertex_start = 0,
        allocator_type                    alloc        = {});

    Mesh(const Mesh& other, allocator_type alloc = {});
    Mesh& operator=(const Mesh&) = default;

    Mesh(Mesh&& other) = default;
    Mesh& operator     =(Mesh&& rhs) noexcept;

private:
    std::shared_ptr<VertexArray>      m_Vertices;
    std::shared_ptr<IndexArray>       m_Indices;
    std::shared_ptr<MaterialInstance> m_Material;
    PrimitiveType                     m_PrimitiveType;
    std::size_t                       m_IndexCount;
    std::size_t                       m_IndexStart;
    std::size_t                       m_VertexStart;
};

template <VertexAttribute Attr>
std::span<VertexType<Attr>> VertexArray::GetVertices() {
    auto& buffer = m_Buffers.at(magic_enum::enum_index(Attr).value());
    if (buffer.Empty()) buffer.Resize(m_VertexCount * get_vertex_attribute_size(Attr));
    return buffer.Span<VertexType<Attr>>();
}

template <IndexType T>
std::span<IndexDataType<T>> IndexArray::GetIndices() {
    if (T != m_IndexType)
        throw std::invalid_argument(fmt::format(
            "expect template parameter: ({}), but get ({})",
            magic_enum::enum_name(m_IndexType),
            magic_enum::enum_name(T)));

    return m_Buffer.Span<IndexDataType<T>>();
}

}  // namespace hitagi::resource