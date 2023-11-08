#include <hitagi/asset/mesh.hpp>
#include <hitagi/gfx/device.hpp>

namespace hitagi::asset {
VertexArray::VertexArray(std::size_t vertex_count, std::string_view name, xg::Guid guid)
    : Resource(name, guid),
      m_VertexCount(vertex_count) {
    magic_enum::enum_for_each<VertexAttribute>([this](VertexAttribute attr) {
        m_Attributes[attr].type = attr;
    });
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

auto VertexArray::GetAttributeData(VertexAttribute attr) const noexcept -> utils::optional_ref<const AttributeData> {
    if (m_Attributes[attr].cpu_buffer.Empty()) return std::nullopt;
    return m_Attributes[attr];
}

auto VertexArray::GetAttributeData(const gfx::VertexAttribute& attr) const noexcept -> utils::optional_ref<const AttributeData> {
    return GetAttributeData(semantic_to_vertex_attribute(attr.semantic));
}

void VertexArray::InitGPUData(gfx::Device& device) {
    for (auto& attribute : m_Attributes) {
        if (attribute.cpu_buffer.Empty() || !attribute.dirty) continue;
        attribute.gpu_buffer = device.CreateGPUBuffer(
            {
                .name          = std::pmr::string(fmt::format("{}-{}", m_Name, magic_enum::enum_name(attribute.type))),
                .element_size  = get_vertex_attribute_size(attribute.type),
                .element_count = m_VertexCount,
                .usages        = gfx::GPUBufferUsageFlags::Vertex | gfx::GPUBufferUsageFlags::CopyDst,
            },
            attribute.cpu_buffer.Span<const std::byte>());
        attribute.dirty = false;
    }
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

void IndexArray::InitGPUData(gfx::Device& device) {
    if (m_Data.cpu_buffer.Empty() || !m_Data.dirty) return;
    m_Data.gpu_buffer = device.CreateGPUBuffer(
        {
            .name          = m_Name,
            .element_size  = get_index_type_size(m_Data.type),
            .element_count = m_IndexCount,
            .usages        = gfx::GPUBufferUsageFlags::Index | gfx::GPUBufferUsageFlags::CopyDst,
        },
        m_Data.cpu_buffer.Span<const std::byte>());
    m_Data.dirty = false;
}

Mesh::Mesh(std::shared_ptr<VertexArray> vertices, std::shared_ptr<IndexArray> indices, std::string_view name, xg::Guid guid)
    : Resource(name, guid),
      vertices(std::move(vertices)),
      indices(std::move(indices)) {}

Mesh Mesh::operator+(const Mesh& rhs) const {
    if (Empty()) return rhs;
    if (rhs.Empty()) return *this;

    auto new_vertices = std::make_shared<VertexArray>(vertices->Size() + rhs.vertices->Size());

    magic_enum::enum_for_each<VertexAttribute>([&](auto attr) {
        constexpr VertexAttribute Attr = attr;
        using DataType                 = VertexDataType<Attr>;

        auto lhs_attr = vertices->Span<Attr>();
        auto rhs_attr = rhs.vertices->Span<Attr>();

        if (lhs_attr.empty() && rhs_attr.empty()) return;

        new_vertices->Modify<Attr>([&](auto values) {
            if (lhs_attr.empty()) {
                std::fill(values.begin(), std::next(values.begin(), vertices->Size()), DataType{});
                std::copy(rhs_attr.begin(), rhs_attr.end(), std::next(values.begin(), vertices->Size()));
            } else if (rhs_attr.empty()) {
                std::copy(lhs_attr.begin(), lhs_attr.end(), values.begin());
                std::fill(std::next(values.begin(), vertices->Size()), values.end(), DataType{});
            } else {
                std::copy(lhs_attr.begin(), lhs_attr.end(), values.begin());
                std::copy(rhs_attr.begin(), rhs_attr.end(), std::next(values.begin(), vertices->Size()));
            }
        });
    });

    std::shared_ptr<IndexArray> new_indices = nullptr;
    if (indices->Type() != rhs.indices->Type()) {
        new_indices = std::make_shared<IndexArray>(indices->Size() + rhs.indices->Size(), IndexType::UINT32);
    } else {
        new_indices = std::make_shared<IndexArray>(indices->Size() + rhs.indices->Size(), indices->Type());
    }
    magic_enum::enum_for_each<IndexType>([&](auto type) {
        if (new_indices->Type() != type()) return;
        auto lhs_values = indices->Span<type()>();
        auto rhs_values = rhs.indices->Span<type()>();

        new_indices->Modify<type()>([&](auto values) {
            std::copy(lhs_values.begin(), lhs_values.end(), values.begin());
            std::copy(rhs_values.begin(), rhs_values.end(), std::next(values.begin(), lhs_values.size()));
        });
    });

    Mesh result(new_vertices, new_indices);
    // merge sub meshes
    for (const auto& lhs_sub_mesh : sub_meshes) {
        result.sub_meshes.emplace_back(lhs_sub_mesh);
    }
    for (const auto& rhs_sub_mesh : rhs.sub_meshes) {
        result.sub_meshes.emplace_back(SubMesh{
            .index_count       = rhs_sub_mesh.index_count,
            .index_offset      = rhs_sub_mesh.index_offset + indices->Size(),
            .vertex_offset     = rhs_sub_mesh.vertex_offset + vertices->Size(),
            .material_instance = rhs_sub_mesh.material_instance,
        });
    }
    return result;
}

}  // namespace hitagi::asset