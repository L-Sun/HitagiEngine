#include <hitagi/resource/mesh.hpp>

#include <spdlog/spdlog.h>

#include <numeric>
#include <algorithm>

namespace hitagi::resource {
VertexArray::VertexArray(VertexAttribute attribute, std::size_t count, std::string_view name)
    : Resource(core::Buffer{count * get_vertex_attribute_size(attribute)}, name),
      attribute(attribute),
      vertex_count(count) {
}

VertexArray::VertexArray(const VertexArray& other)
    : Resource(other.cpu_buffer, other.name),
      attribute(other.attribute),
      vertex_count(other.vertex_count) {}

VertexArray& VertexArray::operator=(const VertexArray& rhs) {
    if (this != &rhs) {
        cpu_buffer   = rhs.cpu_buffer;
        name         = rhs.name;
        dirty        = true;
        vertex_count = rhs.vertex_count;
        attribute    = rhs.attribute;
    }
    return *this;
}

void VertexArray::Resize(std::size_t new_count) {
    if (new_count == vertex_count) return;

    dirty        = true;
    vertex_count = new_count;
    cpu_buffer.Resize(vertex_count * get_vertex_attribute_size(attribute));
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

Mesh::operator bool() const noexcept {
    // vertex array exists
    auto iter = std::find_if(vertices.begin(), vertices.end(), [](const std::shared_ptr<VertexArray>& attribute_array) { return attribute_array != nullptr; });
    if (iter == vertices.end()) return false;

    // all vertex array has same count
    iter = std::find_if_not(iter, vertices.end(), [&](const std::shared_ptr<VertexArray>& attribute_array) {
        return attribute_array == nullptr || attribute_array->vertex_count == (*iter)->vertex_count;
    });
    if (iter != vertices.end()) return false;

    // index array exists and has sub_meshes
    return indices != nullptr && !indices->Empty() && !sub_meshes.empty();
}

Mesh merge_meshes(const std::pmr::vector<Mesh>& meshes) {
    if (meshes.size() == 1) return meshes.front();

    if (auto iter = std::find_if(meshes.begin(), meshes.end(), [](const Mesh& mesh) -> bool { return mesh; }); iter != meshes.end()) {
        auto logger = spdlog::get("AssetManager");
        if (logger) {
            logger->warn("The mesh ({}) is invalid", iter->name);
        } else {
            fmt::print("The mesh ({}) is invalid\n", iter->name);
        }
    }

    auto iter = std::adjacent_find(meshes.begin(), meshes.end(), [](const Mesh& a, const Mesh& b) {
        bool matched = true;
        for (std::size_t index = 0; index < magic_enum::enum_count<VertexAttribute>(); index++) {
            matched = matched & (a.vertices[index] != nullptr && b.vertices[index] != nullptr) || (a.vertices[index] == nullptr && b.vertices[index] == nullptr);
        }
        // find the no match index
        return !matched;
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

    std::pmr::vector<std::size_t> vertex_counts;
    std::transform(meshes.begin(), meshes.end(), std::back_insert_iterator(vertex_counts), [](const Mesh& mesh) {
        return std::find_if(mesh.vertices.begin(), mesh.vertices.end(), [](auto attribute) { return attribute != nullptr; })->get()->vertex_count;
    });
    auto total_vertex_count = std::reduce(vertex_counts.begin(), vertex_counts.end(), 0, [](std::size_t total, const std::size_t& count) {
        return total + count;
    });
    auto total_index_count  = std::reduce(meshes.begin(), meshes.end(), 0, [](std::size_t total, const Mesh& mesh) {
        return total + mesh.indices->index_count;
    });

    Mesh result{
        .indices = std::make_shared<IndexArray>(total_index_count),
    };

    std::size_t index = 0, total_vertex_offset = 0, total_index_offset = 0;
    for (const auto& mesh : meshes) {
        // Copy vertices
        for (std::size_t slot = 0; slot < magic_enum::enum_count<VertexAttribute>(); slot++) {
            VertexAttribute attribute = magic_enum::enum_cast<VertexAttribute>(slot).value();
            if (mesh.vertices[slot] == nullptr) continue;
            if (result.vertices[slot] == nullptr) {
                result.vertices[slot] = std::make_shared<VertexArray>(attribute, total_vertex_count);
            }
            std::memcpy(result.vertices[slot]->cpu_buffer.GetData() + total_vertex_offset * get_vertex_attribute_size(attribute),
                        mesh.vertices[slot]->cpu_buffer.GetData(),
                        mesh.vertices[slot]->cpu_buffer.GetDataSize());
        }

        // Copy indices
        {
            auto dest_array = result.indices->cpu_buffer.Span<std::uint32_t>();
            if (mesh.indices->type == IndexType::UINT32) {
                auto src_array = mesh.Span<IndexType::UINT32>();
                std::copy(src_array.begin(), src_array.end(), std::next(dest_array.begin(), total_index_offset));
            } else {
                auto src_array = mesh.Span<IndexType::UINT16>();
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

        total_index_offset += vertex_counts[index++];
        total_vertex_offset += mesh.indices->index_count;
    }
    return result;
}

}  // namespace hitagi::resource