#include <hitagi/resource/enums.hpp>
#include <hitagi/resource/mesh.hpp>
#include <hitagi/utils/utils.hpp>

#include <magic_enum.hpp>
#include <spdlog/spdlog.h>

using namespace hitagi::math;

namespace hitagi::resource {

VertexArray::VertexArray(std::size_t vertex_count)
    : m_VertexCount(vertex_count),
      m_Buffers(
          utils::create_array_inplcae<
              core::Buffer,
              magic_enum::enum_count<VertexAttribute>()>()) {
}

std::bitset<magic_enum::enum_count<VertexAttribute>()> VertexArray::GetAttributeMask() const {
    std::bitset<magic_enum::enum_count<VertexAttribute>()> mask;
    for (std::size_t i = 0; i < m_Buffers.size(); i++) {
        mask.set(i, !m_Buffers.at(i).Empty());
    }
    return mask;
}

core::Buffer& VertexArray::GetBuffer(VertexAttribute attr) {
    return m_Buffers[magic_enum::enum_integer(attr)];
}

void VertexArray::Resize(std::size_t count) {
    for (std::size_t i = 0; i < m_Buffers.size(); i++) {
        if (m_Buffers[i].Empty()) continue;
        m_Buffers[i].Resize(count * get_vertex_attribute_size(magic_enum::enum_cast<VertexAttribute>(i).value()));
    }
    m_VertexCount = count;
}

IndexArray::IndexArray(std::size_t index_count, IndexType type)
    : m_IndexCount(index_count),
      m_IndexType(type),
      m_Buffer(index_count * get_index_type_size(type)) {
}

void IndexArray::Resize(std::size_t count) {
    m_Buffer.Resize(count * IndexSize());
    m_IndexCount = count;
}

Mesh::Mesh(
    std::shared_ptr<VertexArray>      vertices,
    std::shared_ptr<IndexArray>       indices,
    std::shared_ptr<MaterialInstance> material,
    std::size_t                       index_count,
    std::size_t                       vertex_offset,
    std::size_t                       index_offset)
    : vertices(std::move(vertices)),
      indices(std::move(indices)),
      material(std::move(material)),
      index_count(index_count),
      vertex_offset(vertex_offset),
      index_offset(index_offset) {
    if (this->index_count == 0 && this->indices != nullptr)
        this->index_count = this->indices->IndexCount();
}

}  // namespace hitagi::resource