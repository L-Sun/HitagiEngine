#pragma once
#include <hitagi/asset/resource.hpp>
#include <hitagi/core/buffer.hpp>
#include <hitagi/math/vector.hpp>
#include <hitagi/gfx/gpu_resource.hpp>

#include <magic_enum.hpp>

namespace hitagi::asset {
class MaterialInstance;

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

namespace detail {
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
}  // namespace detail

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

template <VertexAttribute e>
using VertexDataType = std::invoke_result_t<decltype(detail::vertex_attr<e>)>;

constexpr inline auto semantic_to_vertex_attribute(std::string_view semantic) noexcept -> VertexAttribute {
    if (semantic == "POSITION" || semantic == "POSITION0")
        return VertexAttribute::Position;
    if (semantic == "NORMAL" || semantic == "NORMAL0")
        return VertexAttribute::Normal;
    if (semantic == "TANGENT" || semantic == "TANGENT0")
        return VertexAttribute::Tangent;
    if (semantic == "BINORMAL" || semantic == "BINORMAL0")
        return VertexAttribute::Bitangent;
    if (semantic == "COLOR" || semantic == "COLOR0")
        return VertexAttribute::Color0;
    if (semantic == "COLOR1")
        return VertexAttribute::Color1;
    if (semantic == "COLOR2")
        return VertexAttribute::Color2;
    if (semantic == "COLOR3")
        return VertexAttribute::Color3;
    if (semantic == "TEXCOORD" || semantic == "TEXCOORD0")
        return VertexAttribute::UV0;
    if (semantic == "TEXCOORD1")
        return VertexAttribute::UV1;
    if (semantic == "TEXCOORD2")
        return VertexAttribute::UV2;
    if (semantic == "TEXCOORD3")
        return VertexAttribute::UV3;
    if (semantic == "BLENDINDICES" || semantic == "BLENDINDICES0")
        return VertexAttribute::BlendIndex;
    if (semantic == "BLENDWEIGHT" || semantic == "BLENDWEIGHT0")
        return VertexAttribute::BlendWeight;
    return VertexAttribute::Custom0;
}

class VertexArray : public Resource {
public:
    struct AttributeData {
        VertexAttribute                 type;
        bool                            dirty      = true;
        core::Buffer                    cpu_buffer = {};
        std::shared_ptr<gfx::GPUBuffer> gpu_buffer = nullptr;
    };

    VertexArray(std::size_t vertex_count, std::string_view name = "");
    VertexArray(const VertexArray&);
    VertexArray& operator=(const VertexArray&);
    VertexArray(VertexArray&&)            = default;
    VertexArray& operator=(VertexArray&&) = default;

    bool        Empty() const noexcept;
    inline auto Size() const noexcept { return m_VertexCount; }
    auto        GetAttributeData(VertexAttribute attr) const noexcept -> utils::optional_ref<const AttributeData>;
    auto        GetAttributeData(const gfx::VertexAttribute& attr) const noexcept -> utils::optional_ref<const AttributeData>;

    template <VertexAttribute T>
    auto Span() const noexcept -> std::span<const VertexDataType<T>>;

    template <VertexAttribute T>
    void Modify(std::function<void(std::span<VertexDataType<T>>)> modifier);
    void Resize(std::size_t new_count);

    void InitGPUData(gfx::Device& device);

private:
    std::size_t                                      m_VertexCount;
    utils::EnumArray<AttributeData, VertexAttribute> m_Attributes;
};

enum struct IndexType : std::uint8_t {
    UINT16,
    UINT32,
};

constexpr auto get_index_type_size(IndexType type) {
    return type == IndexType::UINT16 ? sizeof(std::uint16_t) : sizeof(std::uint32_t);
}

template <IndexType T>
using IndexDataType = std::conditional_t<T == IndexType::UINT16, std::uint16_t, std::uint32_t>;

class IndexArray : public Resource {
public:
    struct IndexData {
        IndexType                       type;
        bool                            dirty      = true;
        core::Buffer                    cpu_buffer = {};
        std::shared_ptr<gfx::GPUBuffer> gpu_buffer = nullptr;
    };

    IndexArray(std::size_t count, IndexType type = IndexType::UINT16, std::string_view name = "");

    IndexArray(const IndexArray&);
    IndexArray& operator=(const IndexArray&);
    IndexArray(IndexArray&&)            = default;
    IndexArray& operator=(IndexArray&&) = default;

    inline std::size_t Empty() const noexcept { return m_IndexCount == 0; }
    inline auto        Size() const noexcept { return m_IndexCount; }
    inline auto        Type() const noexcept { return m_Data.type; }
    inline const auto& GetIndexData() const noexcept { return m_Data; }

    template <IndexType T>
    auto Span() const noexcept -> std::span<const IndexDataType<T>>;

    template <IndexType T>
    void Modify(std::function<void(std::span<IndexDataType<T>>)> modifier);
    void Resize(std::size_t new_count);

    void InitGPUData(gfx::Device& device);

private:
    std::size_t m_IndexCount;
    IndexData   m_Data;
};

class Mesh : public Resource {
public:
    struct SubMesh {
        std::size_t                       index_count;
        std::size_t                       index_offset  = 0;
        std::size_t                       vertex_offset = 0;
        std::shared_ptr<MaterialInstance> material_instance;
    };

    Mesh(std::string_view name = "") : Resource(Type::Mesh, name) {}
    Mesh(std::shared_ptr<VertexArray> vertices, std::shared_ptr<IndexArray> indices, std::string_view name = "");
    // merge two mesh
    Mesh operator+(const Mesh& rhs) const;

    void AddSubMesh(const SubMesh& sub_mesh);
    bool Empty() const noexcept { return vertices == nullptr || indices == nullptr || sub_meshes.empty(); }

    std::pmr::vector<SubMesh>    sub_meshes;
    std::shared_ptr<VertexArray> vertices;
    std::shared_ptr<IndexArray>  indices;
};

struct MeshComponent {
    std::shared_ptr<Mesh> mesh;
};

template <VertexAttribute T>
auto VertexArray::Span() const noexcept -> std::span<const VertexDataType<T>> {
    if (m_Attributes[T].cpu_buffer.Empty()) return {};

    return std::span<const VertexDataType<T>>(
        reinterpret_cast<const VertexDataType<T>*>(m_Attributes[T].cpu_buffer.GetData()),
        m_VertexCount);
}

template <VertexAttribute T>
void VertexArray::Modify(std::function<void(std::span<VertexDataType<T>>)> modifier) {
    if (m_Attributes[T].cpu_buffer.Empty()) {
        // Enable this attribute for editing
        m_Attributes[T].cpu_buffer = core::Buffer(m_VertexCount * sizeof(VertexDataType<T>));
    }
    modifier(std::span<VertexDataType<T>>(
        reinterpret_cast<VertexDataType<T>*>(m_Attributes[T].cpu_buffer.GetData()),
        m_VertexCount));
    m_Attributes[T].dirty = true;
}

template <IndexType T>
auto IndexArray::Span() const noexcept -> std::span<const IndexDataType<T>> {
    if (T != m_Data.type) return {};

    return std::span<const IndexDataType<T>>(
        reinterpret_cast<const IndexDataType<T>*>(m_Data.cpu_buffer.GetData()),
        m_IndexCount);
}

template <IndexType T>
void IndexArray::Modify(std::function<void(std::span<IndexDataType<T>>)> modifier) {
    if (T != m_Data.type) {
        modifier({});
        return;
    }

    modifier(std::span<IndexDataType<T>>(reinterpret_cast<IndexDataType<T>*>(m_Data.cpu_buffer.GetData()), m_IndexCount));
    m_Data.dirty = true;
}

}  // namespace hitagi::asset