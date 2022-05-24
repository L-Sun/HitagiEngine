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

std::bitset<magic_enum::enum_count<VertexAttribute>()> VertexArray::GetSlotMask() const {
    std::bitset<magic_enum::enum_count<VertexAttribute>()> mask;
    for (std::size_t slot = 0; slot < m_Buffers.size(); slot++) {
        mask.set(slot, !m_Buffers.at(slot).Empty());
    }
    return mask;
}

core::Buffer& VertexArray::GetBuffer(VertexAttribute attr) {
    return GetBuffer(magic_enum::enum_integer(attr));
}

core::Buffer& VertexArray::GetBuffer(std::size_t slot) {
    return m_Buffers.at(slot);
}

IndexArray::IndexArray(std::size_t index_count, IndexType type)
    : m_IndexCount(index_count),
      m_IndexType(type),
      m_Buffer(index_count * get_index_type_size(type)) {
}

Mesh::Mesh(
    std::shared_ptr<VertexArray>      _vertices,
    std::shared_ptr<IndexArray>       _indices,
    std::shared_ptr<MaterialInstance> _material)
    : material(std::move(_material)) {
    if (_vertices == nullptr) {
        if (auto logger = spdlog::get("AssetManager"))
            logger->warn("Can not create mesh with empty vetex array!");
    }
    if (_indices == nullptr) {
        if (auto logger = spdlog::get("AssetManager"))
            logger->warn("Can not create mesh with empty index array!");
    }

    vertices = std::move(_vertices);
    indices  = std::move(_indices);
}

}  // namespace hitagi::resource