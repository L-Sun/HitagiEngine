#pragma once
#include <hitagi/core/buffer.hpp>
#include <hitagi/math/vector.hpp>
#include <hitagi/asset/material.hpp>
#include <hitagi/asset/resource.hpp>
#include <hitagi/asset/scene_node.hpp>

#include <magic_enum.hpp>

#include <functional>
#include <type_traits>

namespace hitagi::asset {

enum struct VertexAttribute : std::uint8_t {
    Position    = 0,   // vec3f
    Normal      = 1,   // vec3f
    Tangent     = 2,   // vec3f
    Bitangent   = 3,   // vec3f
    Color0      = 4,   // vec4f
    Color1      = 5,   // vec4f
    Color2      = 6,   // vec4f
    Color3      = 7,   // vec4f
    UV0         = 8,   // vec2f
    UV1         = 9,   // vec2f
    UV2         = 10,  // vec2f
    UV3         = 11,  // vec2f
    BlendIndex  = 12,  // vec4u
    BlendWeight = 13,  // vec4f

    // Custom the default value type is float
    Custom1 = 14,
    Custom0 = 15,
};

namespace detial {
template <VertexAttribute e>
constexpr auto vertex_attr() noexcept {
    if constexpr (e == VertexAttribute::Position ||
                  e == VertexAttribute::Normal ||
                  e == VertexAttribute::Tangent ||
                  e == VertexAttribute::Bitangent)
        return math::vec3f{};
    else if constexpr (e == VertexAttribute::Color0 ||
                       e == VertexAttribute::Color1 ||
                       e == VertexAttribute::Color2 ||
                       e == VertexAttribute::Color3)
        return math::vec4f{};
    else if constexpr (e == VertexAttribute::UV0 ||
                       e == VertexAttribute::UV1 ||
                       e == VertexAttribute::UV2 ||
                       e == VertexAttribute::UV3)
        return math::vec2f{};
    else if constexpr (e == VertexAttribute::BlendIndex)
        return math::vec4u{};
    else if constexpr (e == VertexAttribute::BlendWeight)
        return math::vec4f{};
    else
        return float{};
}
}  // namespace detial

template <VertexAttribute e>
using VertexDataType = std::invoke_result_t<decltype(detial::vertex_attr<e>)>;

constexpr std::size_t get_vertex_attribute_size(VertexAttribute attribute) {
    switch (attribute) {
        case VertexAttribute::Position:
        case VertexAttribute::Normal:
        case VertexAttribute::Tangent:
        case VertexAttribute::Bitangent:
            return sizeof(math::vec3f);
        case VertexAttribute::Color0:
        case VertexAttribute::Color1:
        case VertexAttribute::Color2:
        case VertexAttribute::Color3:
            return sizeof(math::vec4f);
        case VertexAttribute::UV0:
        case VertexAttribute::UV1:
        case VertexAttribute::UV2:
        case VertexAttribute::UV3:
            return sizeof(math::vec2f);
        case VertexAttribute::BlendIndex:
            return sizeof(math::vec4u);
        case VertexAttribute::BlendWeight:
            return sizeof(math::vec4f);
        default:
            return sizeof(float);
    }
}

struct VertexArray : public Resource {
    VertexArray(VertexAttribute attribute, std::size_t count, std::string_view name = "");

    VertexArray(const VertexArray&);
    VertexArray& operator=(const VertexArray&);

    VertexArray(VertexArray&&)            = default;
    VertexArray& operator=(VertexArray&&) = default;

    inline std::size_t Empty() const noexcept { return vertex_count == 0; }
    void               Resize(std::size_t new_count);

    VertexAttribute attribute;
    std::size_t     vertex_count = 0;
};

enum struct IndexType : std::uint8_t {
    UINT16 = 16,  // trick to fix overload of `Modify` for the MSVC
    UINT32,
};
constexpr auto get_index_type_size(IndexType type) {
    return type == IndexType::UINT16 ? sizeof(std::uint16_t) : sizeof(std::uint32_t);
}

template <IndexType T>
using IndexDataType = std::conditional_t<T == IndexType::UINT16, std::uint16_t, std::uint32_t>;

struct IndexArray : public Resource {
    IndexArray(std::size_t count, std::string_view name = "", IndexType type = IndexType::UINT32)
        : Resource(count * get_index_type_size(type), name), index_count(count), type(type) {}

    IndexArray(const IndexArray&);
    IndexArray& operator=(const IndexArray&);
    IndexArray(IndexArray&&)            = default;
    IndexArray& operator=(IndexArray&&) = default;

    inline std::size_t Empty() const noexcept { return index_count == 0; }
    void               Resize(std::size_t new_count);

    std::size_t index_count;
    IndexType   type;
};

struct Mesh {
    std::pmr::string                                                name;
    utils::EnumArray<std::shared_ptr<VertexArray>, VertexAttribute> vertices = utils::create_enum_array<std::shared_ptr<VertexArray>, VertexAttribute>(nullptr);
    std::shared_ptr<IndexArray>                                     indices  = nullptr;

    struct SubMesh {
        std::size_t                       index_count;
        std::size_t                       index_offset;
        std::size_t                       vertex_offset;
        PrimitiveType                     primitive;
        std::shared_ptr<MaterialInstance> material_instance;
    };
    std::pmr::vector<SubMesh> sub_meshes;

    operator bool() const noexcept;

    template <VertexAttribute... Attrs>
    void Modify(std::function<void(std::span<VertexDataType<Attrs>>...)>&& modifier);

    template <IndexType T>
    void Modify(std::function<void(std::span<IndexDataType<T>>)>&& modifier);

    template <VertexAttribute T>
    std::span<const VertexDataType<T>> Span() const;

    template <IndexType T>
    std::span<const IndexDataType<T>> Span() const;
};
using MeshNode = SceneNodeWithObject<Mesh>;

Mesh merge_meshes(const std::pmr::vector<Mesh>& meshes);

template <VertexAttribute T>
std::span<const VertexDataType<T>> Mesh::Span() const {
    if (vertices[T] == nullptr || vertices[T]->attribute != T) return {};

    return {reinterpret_cast<const VertexDataType<T>*>(vertices[T]->cpu_buffer.GetData()), vertices[T]->vertex_count};
}

template <VertexAttribute... Attrs>
void Mesh::Modify(std::function<void(std::span<VertexDataType<Attrs>>...)>&& modifier) {
    auto get_array = [&](auto attr) {
        if (vertices[attr()] == nullptr) return std::span<VertexDataType<attr()>>{};

        vertices[attr()]->dirty = true;
        return vertices[attr()]->cpu_buffer.template Span<VertexDataType<attr()>>();
    };

    modifier(get_array(magic_enum::enum_constant<Attrs>{})...);
}

template <IndexType T>
std::span<const IndexDataType<T>> Mesh::Span() const {
    if (indices == nullptr || indices->type != T) return {};

    return indices->cpu_buffer.Span<const IndexDataType<T>>();
}

template <IndexType T>
void Mesh::Modify(std::function<void(std::span<IndexDataType<T>>)>&& modifier) {
    if (indices == nullptr || indices->type != T) return;

    modifier(indices->cpu_buffer.Span<IndexDataType<T>>());
    indices->dirty = true;
}

}  // namespace hitagi::asset