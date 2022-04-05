#include <bitset>
#include <hitagi/resource/enums.hpp>
#include <hitagi/resource/mesh.hpp>

#include <magic_enum.hpp>
#include <spdlog/spdlog.h>

#include <memory_resource>
#include <utility>
#include "hitagi/core/buffer.hpp"

using namespace hitagi::math;

namespace hitagi::resource {

auto VertexArray::Builder::BufferCount(std::uint8_t count) noexcept -> Builder& {
    buffer_count = count;
    return *this;
}

auto VertexArray::Builder::VertexCount(std::size_t count) noexcept -> Builder& {
    vertex_count = count;
    return *this;
}

auto VertexArray::Builder::AppendAttributeAt(std::uint8_t buffer_index, VertexAttribute attribute) noexcept -> Builder& {
    if (attributes[static_cast<std::uint8_t>(attribute)].enabled) {
        spdlog::get("AssetManager")->warn("duplicated attribute, will be ignored!");
        return *this;
    }

    attributes[static_cast<std::uint8_t>(attribute)] = {
        .enabled      = true,
        .buffer_index = buffer_index,
        .offset       = buffer_strides[buffer_index],
    };
    buffer_strides[buffer_index] += get_vertex_attribute_size(attribute);
    return *this;
}

std::shared_ptr<VertexArray> VertexArray::Builder::Build() {
    if (vertex_count == 0) {
        spdlog::get("AssetManager")->warn("vertex count can not be zero!");
        return nullptr;
    }
    if (buffer_count == 0) {
        spdlog::get("AssetManager")->warn("buffer count can not be zero!");
        return nullptr;
    }

    std::bitset<static_cast<std::uint8_t>(VertexAttribute::Num_Support)> enabled_slot;
    for (const auto& info : attributes) {
        if (info.enabled) enabled_slot.set(info.buffer_index);
    }

    if (enabled_slot.count() != buffer_count) {
        spdlog::get("AssetManager")->warn("At leaset one slot(buffer) was never assigned to an attribute.");
        return nullptr;
    }

    return VertexArray::Create(*this, std::pmr::polymorphic_allocator<VertexArray>(std::pmr::get_default_resource()));
}

VertexArray::VertexArray(const Builder& builder)
    : m_VertexCount(builder.vertex_count),
      m_AttributeInfo(builder.attributes) {
    for (auto stride : builder.buffer_strides) {
        if (stride == 0) break;
        m_BufferStrides.emplace_back(stride);
        m_Buffers.emplace_back(stride * m_VertexCount);
    }
}

auto IndexArray::Builder::Count(std::size_t count) noexcept -> Builder& {
    index_count = count;
    return *this;
}

auto IndexArray::Builder::Type(IndexType type) noexcept -> Builder& {
    index_type = type;
    return *this;
}

std::shared_ptr<IndexArray> IndexArray::Builder::Build() {
    if (index_count == 0) {
        spdlog::get("AssetManager")->warn("index count can not be zero!");
        return nullptr;
    }
    return IndexArray::Create(*this, std::pmr::polymorphic_allocator<IndexArray>(std::pmr::get_default_resource()));
}

IndexArray::IndexArray(const Builder& builder)
    : m_IndexType(builder.index_type),
      m_IndexCount(builder.index_count),
      m_Buffer(get_index_type_size(builder.index_type) * builder.index_count) {}

Mesh::Mesh(
    std::shared_ptr<VertexArray> vertices,
    std::shared_ptr<IndexArray>  indices,
    PrimitiveType                type,
    std::size_t                  index_count,
    std::size_t                  index_start,
    std::size_t                  vertex_start)
    : m_PrimitiveType(type) {
    if (vertices == nullptr) {
        spdlog::get("AssetManager")->warn("Can not create mesh with empty vetex array!");
        return;
    }
    if (indices == nullptr) {
        spdlog::get("AssetManager")->warn("Can not create mesh with empty index array!");
        return;
    }
    if (index_start + index_count > indices->IndexCount()) {
        spdlog::get("AssetManager")->warn("Can not create mesh since index used by mesh is out of range!");
        return;
    }
    if (vertex_start > vertices->VertexCount()) {
        spdlog::get("AssetManager")->warn("Can not create mesh since vertex used by mesh is out of range!");
        return;
    }

    m_Vertices    = std::move(vertices);
    m_Indices     = std::move(indices);
    m_IndexCount  = index_count;
    m_IndexStart  = index_start;
    m_VertexStart = vertex_start;
}

}  // namespace hitagi::resource