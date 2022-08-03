#pragma once
#include <hitagi/core/buffer.hpp>
#include <hitagi/math/vector.hpp>
#include <hitagi/resource/material.hpp>
#include <hitagi/resource/resource.hpp>

#include <magic_enum.hpp>

#include <functional>
#include <type_traits>

namespace hitagi::resource {
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
    /**/ if constexpr (e == VertexAttribute::Position ||
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
using VertexType = std::invoke_result_t<decltype(detial::vertex_attr<e>)>;

constexpr std::size_t get_vertex_attribute_size(VertexAttribute attribute) {
    return magic_enum::enum_switch(
        [](auto e) {
            return sizeof(VertexType<e>);
        },
        attribute, 4);
}

enum struct IndexType : std::uint8_t {
    UINT16,
    UINT32,
};

template <IndexType T>
using IndexDataType = std::conditional_t<T == IndexType::UINT16, std::uint16_t, std::uint32_t>;

constexpr auto get_index_type_size(IndexType type) {
    return type == IndexType::UINT16 ? sizeof(std::uint16_t) : sizeof(std::uint32_t);
}

struct VertexArray : public Resource {
    VertexArray(VertexArray&&)            = default;
    VertexArray& operator=(VertexArray&&) = default;

    VertexArray(std::size_t count) : vertex_count(count) {}

    void Resize(std::size_t new_count);
    void Enable(VertexAttribute attr);
    bool IsEnabled(VertexAttribute attr) const;

    template <VertexAttribute Attr>
    auto GetVertices() const;

    template <VertexAttribute... Attrs>
    void Modify(std::function<void(std::span<VertexType<Attrs>>...)>&& modifier);

    std::size_t                                            vertex_count = 0;
    std::bitset<magic_enum::enum_count<VertexAttribute>()> attribute_mask;
};

struct IndexArray : public Resource {
    IndexArray(IndexArray&&)            = default;
    IndexArray& operator=(IndexArray&&) = default;

    IndexArray(std::size_t count, IndexType type, std::string_view name = "")
        : Resource(count * get_index_type_size(type)), index_count(count), type(type) {}

    void Concat(const IndexArray& other);
    void Resize(std::size_t new_count);

    template <IndexType T>
    auto GetIndices() const;

    template <IndexType T>
    void Modify(std::function<void(std::span<IndexDataType<T>>)>&& modifier);

    std::size_t index_count = 0;
    IndexType   type        = IndexType::UINT32;
};

struct Mesh {
    std::shared_ptr<VertexArray> vertices;
    std::shared_ptr<IndexArray>  indices;

    struct SubMesh {
        std::size_t      index_count;
        std::size_t      vertex_offset;
        std::size_t      index_offset;
        MaterialInstance material;
    };
    bool                      visiable = true;
    std::pmr::vector<SubMesh> sub_meshes;
};

Mesh merge_meshes(const std::pmr::vector<Mesh>& meshes);

template <VertexAttribute Attr>
auto VertexArray::GetVertices() const {
    std::size_t buffer_offset = 0;
    for (std::size_t i = 0; i < magic_enum::enum_integer(Attr); i++) {
        if (attribute_mask.test(i)) buffer_offset += vertex_count * get_vertex_attribute_size(magic_enum::enum_cast<VertexAttribute>(i).value());
    }
    return std::span<const VertexType<Attr>>(
        reinterpret_cast<const VertexType<Attr>*>(cpu_buffer.GetData() + buffer_offset),
        vertex_count);
}

template <VertexAttribute... Attrs>
void VertexArray::Modify(std::function<void(std::span<VertexType<Attrs>>...)>&& modifier) {
    if (!(IsEnabled(Attrs) && ... && true)) {
        (Enable(Attrs), ...);
    };

    auto get_array = [&](auto attr) {
        auto buffer_offset = 0;
        for (std::size_t i = 0; i < magic_enum::enum_integer(attr()); i++) {
            if (attribute_mask.test(i)) buffer_offset += vertex_count * get_vertex_attribute_size(magic_enum::enum_cast<VertexAttribute>(i).value());
        }
        return std::span<VertexType<attr()>>(
            reinterpret_cast<VertexType<attr()>*>(cpu_buffer.GetData() + buffer_offset),
            vertex_count);
    };

    modifier(get_array(magic_enum::enum_constant<Attrs>{})...);
    dirty = true;
}

template <IndexType T>
auto IndexArray::GetIndices() const {
    if (T != type)
        throw std::invalid_argument(fmt::format(
            "expect template parameter: ({}), but get ({})",
            magic_enum::enum_name(type),
            magic_enum::enum_name(T)));

    return cpu_buffer.Span<const IndexDataType<T>>();
}

template <IndexType T>
void IndexArray::Modify(std::function<void(std::span<IndexDataType<T>>)>&& func) {
    if (T != type)
        throw std::invalid_argument(fmt::format(
            "expect template parameter: ({}), but get ({})",
            magic_enum::enum_name(type),
            magic_enum::enum_name(T)));

    dirty = true;
    func(cpu_buffer.Span<IndexDataType<T>>());
}

}  // namespace hitagi::resource