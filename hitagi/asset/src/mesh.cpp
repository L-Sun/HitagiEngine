#include <hitagi/asset/mesh.hpp>

namespace hitagi::asset {
VertexArray::VertexArray(std::size_t vertex_count, std::string_view name, xg::Guid guid)
    : Resource(name, guid),
      m_VertexCount(vertex_count) {
}

VertexArray::VertexArray(const VertexArray& other)
    : Resource(other),
      m_VertexCount(other.m_VertexCount),
      m_Attributes(other.m_Attributes) {
    for (auto& attribute : m_Attributes) {
        attribute.dirty = true;
    }
}

VertexArray& VertexArray::operator=(const VertexArray& rhs) {
    if (this != &rhs) {
        Resource::operator=(rhs);

        m_VertexCount = rhs.m_VertexCount;
        m_Attributes  = rhs.m_Attributes;

        for (auto& attribute : m_Attributes) {
            attribute.dirty = true;
        }
    }
    return *this;
}

void VertexArray::Resize(std::size_t new_count) {
    if (new_count == m_VertexCount) return;

    for (auto& attribute : m_Attributes) {
        // This attribute is not enable
        if (attribute.cpu_buffer.Empty()) continue;

        attribute.cpu_buffer.Resize(m_VertexCount * get_vertex_attribute_size(attribute.type));
        attribute.dirty = true;
    }
    m_VertexCount = new_count;
}

bool VertexArray::Empty() const noexcept {
    if (m_VertexCount == 0) return true;
    for (const auto& attribute : m_Attributes) {
        if (!attribute.cpu_buffer.Empty()) return false;
    }
    return true;
}

IndexArray::IndexArray(std::size_t count, IndexType type, std::string_view name, xg::Guid guid)
    : Resource(name, guid), m_IndexCount(count), m_Data{.type = type, .cpu_buffer = core::Buffer(count * get_index_type_size(type))} {
}

IndexArray::IndexArray(const IndexArray& other)
    : Resource(other),
      m_IndexCount(other.m_IndexCount),
      m_Data(other.m_Data) {
    m_Data.dirty = true;
}

IndexArray& IndexArray::operator=(const IndexArray& rhs) {
    if (this != &rhs) {
        Resource::operator=(rhs);

        m_IndexCount = rhs.m_IndexCount;
        m_Data       = rhs.m_Data;
        m_Data.dirty = true;
    }
    return *this;
}

void IndexArray::Resize(std::size_t new_count) {
    if (new_count == m_IndexCount) return;

    m_Data.cpu_buffer.Resize(new_count * get_index_type_size(m_Data.type));
    m_IndexCount = new_count;
    m_Data.dirty = true;
}

Mesh::Mesh(std::shared_ptr<VertexArray> vertices, std::shared_ptr<IndexArray> indices, std::string_view name, xg::Guid guid)
    : Resource(name, guid),
      m_Vertices(std::move(vertices)),
      m_Indices(std::move(indices)) {}

Mesh Mesh::operator+(const Mesh& rhs) const {
    if (Empty()) return rhs;
    if (rhs.Empty()) return *this;

    auto new_vertices = std::make_shared<VertexArray>(m_Vertices->Size() + rhs.m_Vertices->Size());

    magic_enum::enum_for_each<VertexAttribute>([&](auto attr) {
        auto lhs_attr = m_Vertices->Span<attr()>();
        auto rhs_attr = rhs.m_Vertices->Span<attr()>();

        if (lhs_attr.empty() && rhs_attr.empty()) return;

        new_vertices->Modify<attr()>([&](auto values) {
            if (lhs_attr.empty()) {
                std::fill(values.begin(), std::next(values.begin(), m_Vertices->Size()), VertexDataType<attr()>{});
                std::copy(rhs_attr.begin(), rhs_attr.end(), std::next(values.begin(), m_Vertices->Size()));
            } else if (rhs_attr.empty()) {
                std::copy(lhs_attr.begin(), lhs_attr.end(), values.begin());
                std::fill(std::next(values.begin(), m_Vertices->Size()), values.end(), VertexDataType<attr()>{});
            } else {
                std::copy(lhs_attr.begin(), lhs_attr.end(), values.begin());
                std::copy(rhs_attr.begin(), rhs_attr.end(), std::next(values.begin(), m_Vertices->Size()));
            }
        });
    });

    std::shared_ptr<IndexArray> new_indices = nullptr;
    if (m_Indices->Type() != rhs.m_Indices->Type()) {
        new_indices = std::make_shared<IndexArray>(m_Indices->Size() + rhs.m_Indices->Size(), IndexType::UINT32);
    } else {
        new_indices = std::make_shared<IndexArray>(m_Indices->Size() + rhs.m_Indices->Size(), m_Indices->Type());
    }
    magic_enum::enum_for_each<IndexType>([&](auto type) {
        if (new_indices->Type() != type()) return;
        auto lhs_values = m_Indices->Span<type()>();
        auto rhs_values = rhs.m_Indices->Span<type()>();

        new_indices->Modify<type()>([&](auto values) {
            std::copy(lhs_values.begin(), lhs_values.end(), values.begin());
            std::copy(rhs_values.begin(), rhs_values.end(), std::next(values.begin(), lhs_values.size()));
        });
    });

    Mesh result(new_vertices, new_indices);
    // merge submeshes
    for (const auto& lhs_sub_mesh : m_SubMeshes) {
        result.AddSubMesh(lhs_sub_mesh);
    }
    for (const auto& rhs_sub_mesh : rhs.m_SubMeshes) {
        result.AddSubMesh({
            .index_count       = rhs_sub_mesh.index_count,
            .index_offset      = rhs_sub_mesh.index_offset + m_Indices->Size(),
            .vertex_offset     = rhs_sub_mesh.vertex_offset + m_Vertices->Size(),
            .primitive         = rhs_sub_mesh.primitive,
            .material_instance = rhs_sub_mesh.material_instance,
        });
    }
    return result;
}

}  // namespace hitagi::asset