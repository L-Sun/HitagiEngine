#include <hitagi/resource/mesh.hpp>

#include <spdlog/spdlog.h>

#include <numeric>
#include <algorithm>

namespace hitagi::resource {
VertexArray::VertexArray(std::size_t count, std::string_view name, VertexAttributeMask attributes)
    : Resource(core::Buffer{}, name),
      vertex_count(count),
      attribute_mask(attributes) {
    std::size_t size_per_element = 0;
    for (std::size_t attr = 0; attr < attributes.size(); attr++) {
        if (attributes.test(attr)) {
            size_per_element += get_vertex_attribute_size(magic_enum::enum_cast<VertexAttribute>(attr).value());
        }
    }
    cpu_buffer = core::Buffer(count * size_per_element);
}

VertexArray::VertexArray(const VertexArray& other)
    : Resource(other.cpu_buffer, other.name),
      vertex_count(other.vertex_count),
      attribute_mask(other.attribute_mask) {
}

VertexArray& VertexArray::operator=(const VertexArray& rhs) {
    if (this != &rhs) {
        cpu_buffer     = rhs.cpu_buffer;
        name           = rhs.name;
        dirty          = true;
        vertex_count   = rhs.vertex_count;
        attribute_mask = rhs.attribute_mask;
    }
    return *this;
}

void VertexArray::Enable(VertexAttribute attr) {
    if (IsEnabled(attr)) return;

    dirty                = true;
    std::size_t new_size = 0;
    magic_enum::enum_for_each<VertexAttribute>([&](auto e) {
        if (IsEnabled(e()) || attr == e()) {
            new_size += vertex_count * get_vertex_attribute_size(e());
        }
    });

    core::Buffer new_buffer(new_size);
    std::size_t  src_buffer_offset  = 0;
    std::size_t  dest_buffer_offset = 0;
    magic_enum::enum_for_each<VertexAttribute>([&](auto e) {
        if (IsEnabled(e())) {
            std::copy_n(
                cpu_buffer.GetData() + src_buffer_offset,
                vertex_count * get_vertex_attribute_size(e()),
                new_buffer.GetData() + dest_buffer_offset);
            src_buffer_offset += vertex_count * get_vertex_attribute_size(e());
            dest_buffer_offset += vertex_count * get_vertex_attribute_size(e());
        } else if (e() == attr) {
            dest_buffer_offset += vertex_count * get_vertex_attribute_size(e());
        }
    });
    assert(dest_buffer_offset == new_buffer.GetDataSize());
    cpu_buffer = std::move(new_buffer);
    attribute_mask.set(magic_enum::enum_integer(attr));
}

bool VertexArray::IsEnabled(VertexAttribute attr) const {
    return attribute_mask.test(magic_enum::enum_integer(attr));
}

void VertexArray::Resize(std::size_t new_count) {
    if (new_count == vertex_count) return;

    dirty                = true;
    vertex_count         = new_count;
    std::size_t new_size = 0;
    magic_enum::enum_for_each<VertexAttribute>([&](auto attr) {
        if (IsEnabled(attr())) {
            new_size += vertex_count * get_vertex_attribute_size(attr());
        }
    });
    cpu_buffer.Resize(new_size);
}

IndexArray::IndexArray(const IndexArray& other)
    : Resource(other.cpu_buffer, other.name),
      index_count(other.index_count),
      type(other.type) {
}

IndexArray& IndexArray::operator=(const IndexArray& rhs) {
    if (this != &rhs) {
        cpu_buffer  = rhs.cpu_buffer;
        dirty       = true;
        name        = rhs.name;
        index_count = rhs.index_count;
        type        = rhs.type;
    }
    return *this;
}

void IndexArray::Resize(std::size_t new_count) {
    if (new_count == index_count) return;

    index_count = new_count;
    cpu_buffer.Resize(new_count * get_index_type_size(type));
    dirty = true;
}

Mesh merge_meshes(const std::pmr::vector<Mesh>& meshes) {
    if (meshes.size() == 1) return meshes.front();

    auto iter = std::adjacent_find(meshes.begin(), meshes.end(), [](const Mesh& a, const Mesh& b) {
        return a.vertices->attribute_mask != a.vertices->attribute_mask;
    });
    if (iter != meshes.end()) {
        auto logger = spdlog::get("AssetManager");
        if (logger) {
            logger->warn("there are vertices of meshes have unmatched attirbutes");
        } else {
            std::cout << "there are vertices of meshes have unmatched attirbutes" << std::endl;
        }
        return {};
    }

    auto total_vertex_count = std::reduce(meshes.begin(), meshes.end(), 0, [](std::size_t total, const Mesh& mesh) {
        return total + mesh.vertices->vertex_count;
    });
    auto total_index_count  = std::reduce(meshes.begin(), meshes.end(), 0, [](std::size_t total, const Mesh& mesh) {
        return total + mesh.indices->index_count;
    });

    Mesh result{
        .vertices = std::make_shared<VertexArray>(total_vertex_count),
        .indices  = std::make_shared<IndexArray>(total_index_count),
    };
    magic_enum::enum_for_each<VertexAttribute>([&](auto attr) {
        if (meshes.front().vertices->IsEnabled(attr())) {
            result.vertices->Enable(attr());
        }
    });

    std::size_t total_vertex_offset = 0, total_index_offset = 0;
    for (const auto& mesh : meshes) {
        // Copy vertices
        magic_enum::enum_for_each<VertexAttribute>([&](auto attr) {
            if (mesh.vertices->IsEnabled(attr())) {
                result.vertices->Modify<attr()>([&](auto dest_array) {
                    auto src_array = mesh.vertices->GetVertices<attr()>();
                    std::copy(src_array.begin(), src_array.end(), std::next(dest_array.begin(), total_vertex_offset));
                });
            }
        });

        // Copy indices
        {
            auto dest_array = result.indices->cpu_buffer.Span<std::uint32_t>();
            if (mesh.indices->type == IndexType::UINT32) {
                auto src_array = mesh.indices->GetIndices<IndexType::UINT32>();
                std::copy(src_array.begin(), src_array.end(), std::next(dest_array.begin(), total_index_offset));
            } else {
                auto src_array = mesh.indices->GetIndices<IndexType::UINT16>();
                std::transform(src_array.begin(), src_array.end(), std::next(dest_array.begin(), total_index_offset), [](std::uint16_t value) {
                    return static_cast<std::uint32_t>(value);
                });
            }
        }

        // merge submeshes
        for (const auto& sub_mesh : mesh.sub_meshes) {
            result.sub_meshes.emplace_back(Mesh::SubMesh{
                .index_count       = sub_mesh.index_count,
                .index_offset      = total_index_offset + sub_mesh.index_offset,
                .vertex_offset     = total_vertex_offset + sub_mesh.vertex_offset,
                .material_instance = sub_mesh.material_instance,
            });
        }

        total_index_offset += mesh.vertices->vertex_count;
        total_vertex_offset += mesh.indices->index_count;
    }
    return result;
}

}  // namespace hitagi::resource