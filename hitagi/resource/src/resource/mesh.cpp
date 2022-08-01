#include <hitagi/resource/mesh.hpp>

#include <spdlog/spdlog.h>

#include <numeric>

namespace hitagi::resource {
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

void IndexArray::Resize(std::size_t new_count) {
    if (new_count == index_count) return;

    index_count = new_count;
    cpu_buffer.Resize(new_count * get_index_type_size(type));
    dirty = true;
}

void IndexArray::Concat(const IndexArray& other) {
    std::size_t old_buffer_size = cpu_buffer.GetDataSize();

    Resize(index_count + other.index_count);
    if (type == other.type) {
        std::copy_n(other.cpu_buffer.GetData(), other.cpu_buffer.GetDataSize(), cpu_buffer.GetData() + old_buffer_size);
    } else {
        if (other.type == IndexType::UINT32) {
            auto array       = cpu_buffer.Span<std::uint16_t>();
            auto other_array = other.GetIndices<IndexType::UINT32>();
            std::transform(
                other_array.begin(),
                other_array.end(),
                std::next(array.begin(), index_count - other.index_count),
                [](std::uint32_t value) { return static_cast<std::uint16_t>(value); });
        } else {
            auto array       = cpu_buffer.Span<std::uint32_t>();
            auto other_array = other.GetIndices<IndexType::UINT16>();
            std::transform(
                other_array.begin(),
                other_array.end(),
                std::next(array.begin(), index_count - other.index_count),
                [](std::uint16_t value) { return static_cast<std::uint32_t>(value); });
        }
    }
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
        .indices  = std::make_shared<IndexArray>(total_index_count, IndexType::UINT32),
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
                .index_count   = sub_mesh.index_count,
                .vertex_offset = total_vertex_offset + sub_mesh.vertex_offset,
                .index_offset  = total_index_offset + sub_mesh.index_offset,
                .material      = sub_mesh.material,
            });
        }

        total_index_offset += mesh.vertices->vertex_count;
        total_vertex_offset += mesh.indices->index_count;
    }
    return result;
}

}  // namespace hitagi::resource