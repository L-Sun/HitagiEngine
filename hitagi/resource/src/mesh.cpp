#include <hitagi/resource/enums.hpp>
#include <hitagi/resource/mesh.hpp>

#include <magic_enum.hpp>
#include <memory_resource>
#include "hitagi/core/buffer.hpp"
#include "spdlog/spdlog.h"

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

    size_t num_enabled_attribute = 0;
    for (const auto& info : attributes) {
        if (info.enabled) num_enabled_attribute++;
    }

    if (num_enabled_attribute != buffer_count) {
        spdlog::get("AssetManager")->warn("At leaset one slot(buffer) was never assigned to an attribute.");
        return nullptr;
    }

    auto result = std::allocate_shared<VertexArray>(std::pmr::polymorphic_allocator<VertexArray>(std::pmr::get_default_resource()));

    result->m_VertexCount   = vertex_count;
    result->m_AttributeInfo = attributes;
    for (auto stride : buffer_strides) {
        if (stride == 0) break;
        result->m_BufferStrides.emplace_back(stride);
    }
    for (std::size_t index = 0; index < buffer_count; index++) {
        result->m_Buffers.emplace_back(buffer_strides.at(index) * vertex_count);
    }

    return result;
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
    auto result          = std::allocate_shared<IndexArray>(std::pmr::polymorphic_allocator<IndexArray>(std::pmr::get_default_resource()));
    result->m_IndexType  = index_type;
    result->m_IndexCount = index_count;
    result->m_Data       = core::Buffer(get_index_type_size(index_type) * index_count);
}

}  // namespace hitagi::resource