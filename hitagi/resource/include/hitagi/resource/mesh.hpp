#pragma once
#include <hitagi/resource/scene_object.hpp>
#include <hitagi/resource/enums.hpp>
#include <hitagi/resource/material_instance.hpp>
#include <hitagi/core/buffer.hpp>
#include <hitagi/utils/private_build.hpp>

#include <magic_enum.hpp>

#include <stdexcept>
#include <bitset>

namespace hitagi::resource {

class VertexArray : public SceneObject {
public:
    VertexArray(std::size_t vertex_count);
    VertexArray(const VertexArray& other) = default;
    VertexArray(VertexArray&& other)      = default;

    inline std::size_t VertexCount() const noexcept { return m_VertexCount; }

    std::bitset<magic_enum::enum_count<VertexAttribute>()> GetSlotMask() const;

    core::Buffer& GetBuffer(VertexAttribute attr);
    core::Buffer& GetBuffer(std::size_t slot);

    // the buffer will be constructed if `GetVertices` is invoke first time with attribute Attr
    template <VertexAttribute Attr>
    std::span<VertexType<Attr>> GetVertices();

    void Resize(std::size_t count);

private:
    std::size_t                                                         m_VertexCount = 0;
    std::array<core::Buffer, magic_enum::enum_count<VertexAttribute>()> m_Buffers;
};

class IndexArray : public SceneObject {
public:
    IndexArray(std::size_t index_count, IndexType type);
    IndexArray(const IndexArray& other) = default;
    IndexArray(IndexArray&& other)      = default;

    auto  IndexSize() const noexcept { return get_index_type_size(m_IndexType); }
    auto  IndexCount() const noexcept { return m_IndexCount; }
    auto& Buffer() const noexcept { return m_Buffer; }

    // The template parameter need to be same with the type used by constructor
    template <IndexType T>
    std::span<IndexDataType<T>> GetIndices();

    void Resize(std::size_t count);

private:
    std::size_t  m_IndexCount = 0;
    IndexType    m_IndexType  = IndexType::UINT32;
    core::Buffer m_Buffer;
};

struct Mesh : public SceneObject {
public:
    // if `index_count` is not set, its value will be the size of indices
    Mesh(
        std::shared_ptr<VertexArray>      vertices      = nullptr,
        std::shared_ptr<IndexArray>       indices       = nullptr,
        std::shared_ptr<MaterialInstance> material      = nullptr,
        std::size_t                       index_count   = 0,
        std::size_t                       vertex_offset = 0,
        std::size_t                       index_offset  = 0);

    Mesh(const Mesh& other)      = default;
    Mesh& operator=(const Mesh&) = default;
    Mesh(Mesh&& other)           = default;
    Mesh& operator=(Mesh&& rhs)  = default;

    std::shared_ptr<VertexArray>      vertices;
    std::shared_ptr<IndexArray>       indices;
    std::shared_ptr<MaterialInstance> material;
    std::size_t                       index_count;
    std::size_t                       vertex_offset;
    std::size_t                       index_offset;
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