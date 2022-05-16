#include <hitagi/resource/enums.hpp>
#include <hitagi/resource/mesh.hpp>
#include <hitagi/utils/utils.hpp>

#include <magic_enum.hpp>
#include <spdlog/spdlog.h>

using namespace hitagi::math;

namespace hitagi::resource {

VertexArray::VertexArray(std::size_t vertex_count, allocator_type alloc)
    : SceneObject(alloc),
      m_VertexCount(vertex_count),
      m_Buffers(
          utils::create_array_inplcae<
              core::Buffer,
              magic_enum::enum_count<VertexAttribute>()>(alloc)) {
}

bool VertexArray::IsEnable(VertexAttribute attr) const {
    return !m_Buffers.at(magic_enum::enum_index(attr).value()).Empty();
}

IndexArray::IndexArray(std::size_t index_count, IndexType type, allocator_type alloc)
    : SceneObject(alloc),
      m_IndexCount(index_count),
      m_IndexType(type),
      m_Buffer(index_count * get_index_type_size(type), alloc) {
}

Mesh::Mesh(
    std::shared_ptr<VertexArray>      vertices,
    std::shared_ptr<IndexArray>       indices,
    std::shared_ptr<MaterialInstance> material,
    PrimitiveType                     type,
    allocator_type                    alloc)
    : SceneObject(alloc),
      m_Material(std::move(material)),
      m_PrimitiveType(type) {
    if (vertices == nullptr) {
        spdlog::get("AssetManager")->warn("Can not create mesh with empty vetex array!");
        return;
    }
    if (indices == nullptr) {
        spdlog::get("AssetManager")->warn("Can not create mesh with empty index array!");
        return;
    }

    m_Vertices = std::move(vertices);
    m_Indices  = std::move(indices);
}

Mesh::Mesh(const Mesh& other, allocator_type alloc)
    : SceneObject(alloc),
      m_Vertices(other.m_Vertices),
      m_Indices(other.m_Indices),
      m_Material(other.m_Material),
      m_PrimitiveType(other.m_PrimitiveType) {}

Mesh& Mesh::operator=(Mesh&& rhs) noexcept {
    SceneObject::operator=(rhs);
    if (this != &rhs) {
        m_Vertices      = rhs.m_Vertices;
        m_Indices       = rhs.m_Indices;
        m_Material      = rhs.m_Material;
        m_PrimitiveType = rhs.m_PrimitiveType;
    }
    return *this;
}

}  // namespace hitagi::resource