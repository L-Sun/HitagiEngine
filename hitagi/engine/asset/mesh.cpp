export module asset:mesh;
import std;
import interop.magic_enum;
import core;
import math;
import utils;
import gfx;
import :resource;
import :material;

export namespace hitagi::asset {
enum struct VertexAttribute : std::uint8_t {
    Position    = 0,
    Normal      = 1,
    Tangent     = 2,
    Bitangent   = 3,
    Color0      = 4,
    Color1      = 5,
    Color2      = 6,
    Color3      = 7,
    UV0         = 8,
    UV1         = 9,
    UV2         = 10,
    UV3         = 11,
    BlendIndex  = 12,
    BlendWeight = 13,
    Custom1     = 14,
    Custom0     = 15,
};

namespace detail {
template <VertexAttribute e>
constexpr auto vertex_attr_type() {
    if constexpr (
        e == VertexAttribute::Position ||
        e == VertexAttribute::Normal ||
        e == VertexAttribute::Tangent ||
        e == VertexAttribute::Bitangent)
        return math::vec3f{};
    else if constexpr (
        e == VertexAttribute::Color0 ||
        e == VertexAttribute::Color1 ||
        e == VertexAttribute::Color2 ||
        e == VertexAttribute::Color3)
        return math::vec4f{};
    else if constexpr (
        e == VertexAttribute::UV0 ||
        e == VertexAttribute::UV1 ||
        e == VertexAttribute::UV2 ||
        e == VertexAttribute::UV3)
        return math::vec2f{};
    else if constexpr (
        e == VertexAttribute::BlendIndex)
        return math::vec4u{};
    else if constexpr (
        e == VertexAttribute::BlendWeight)
        return math::vec4f{};
    else
        return float{};
}
}  // namespace detail

template <VertexAttribute e>
using VertexDataType = decltype(detail::vertex_attr_type<e>());

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
        core::Buffer                    cpu_buffer = {};
        std::shared_ptr<gfx::GPUBuffer> gpu_buffer = nullptr;
    };

    // Construction data is mutable; the resource takes ownership and exposes only const views.
    using Data = utils::EnumArray<core::Buffer, VertexAttribute>;
    VertexArray(std::size_t vertex_count, Data data, std::string_view name = "");
    VertexArray(const VertexArray&)            = delete;
    VertexArray& operator=(const VertexArray&) = delete;
    VertexArray(VertexArray&&)                 = delete;
    VertexArray& operator=(VertexArray&&)      = delete;

    bool        Empty() const noexcept;
    inline auto Size() const noexcept { return m_VertexCount; }
    auto        GetAttributeData(VertexAttribute attr) const noexcept -> utils::optional_ref<const AttributeData>;
    auto        GetAttributeData(const gfx::VertexAttribute& attr) const noexcept -> utils::optional_ref<const AttributeData>;

    template <VertexAttribute T>
    auto Span() const noexcept -> std::span<const VertexDataType<T>>;

    void Load(const ResourceLoadContext& context) final;
    void Unload() final;

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
        core::Buffer                    cpu_buffer = {};
        std::shared_ptr<gfx::GPUBuffer> gpu_buffer = nullptr;
    };

    IndexArray(IndexType type, core::Buffer data, std::string_view name = "");
    IndexArray(const IndexArray&)            = delete;
    IndexArray& operator=(const IndexArray&) = delete;
    IndexArray(IndexArray&&)                 = delete;
    IndexArray& operator=(IndexArray&&)      = delete;

    inline bool        Empty() const noexcept { return m_IndexCount == 0; }
    inline auto        Size() const noexcept { return m_IndexCount; }
    inline auto        Type() const noexcept { return m_Data.type; }
    inline const auto& GetIndexData() const noexcept { return m_Data; }
    inline auto        GetGPUData() const noexcept { return m_Data.gpu_buffer; }

    template <IndexType T>
    auto Span() const noexcept -> std::span<const IndexDataType<T>>;

    void Load(const ResourceLoadContext& context) final;
    void Unload() final;

private:
    std::size_t m_IndexCount;
    IndexData   m_Data;
};

class Mesh : public Resource {
public:
    struct SubMesh {
        std::size_t               index_count;
        std::size_t               index_offset  = 0;
        std::size_t               vertex_offset = 0;
        std::shared_ptr<Material> material;
    };

    Mesh(std::shared_ptr<VertexArray> vertices = {}, std::shared_ptr<IndexArray> indices = {},
         std::pmr::vector<SubMesh> sub_meshes = {}, std::string_view name = "");
    Mesh(const Mesh&)            = delete;
    Mesh& operator=(const Mesh&) = delete;
    Mesh(Mesh&&)                 = delete;
    Mesh& operator=(Mesh&&)      = delete;
    bool  Empty() const noexcept { return vertices == nullptr || indices == nullptr || sub_meshes.empty(); }

    void Load(const ResourceLoadContext& context) final;
    void Unload() final;

    const std::pmr::vector<SubMesh>    sub_meshes;
    const std::shared_ptr<VertexArray> vertices;
    const std::shared_ptr<IndexArray>  indices;
};

struct MeshComponent {
    std::shared_ptr<Mesh> mesh;
    // Slot i replaces submesh i's material; null slots keep the mesh default.
    std::vector<std::shared_ptr<Material>> material_overrides;
};

inline auto ResolveSubMeshes(const Mesh& mesh, std::span<const std::shared_ptr<Material>> overrides) {
    if (overrides.size() > mesh.sub_meshes.size()) throw std::invalid_argument("Too many material overrides");
    return std::views::iota(std::size_t{0}, mesh.sub_meshes.size()) |
           std::views::transform([&mesh, overrides](std::size_t i) {
               auto sub_mesh = mesh.sub_meshes[i];
               if (i < overrides.size() && overrides[i]) sub_mesh.material = overrides[i];
               return sub_mesh;
           });
}

}  // namespace hitagi::asset

namespace hitagi::asset {
VertexArray::VertexArray(std::size_t vertex_count, Data data, std::string_view name)
    : Resource(Type::Vertex, name), m_VertexCount(vertex_count) {
    magic_enum::enum_for_each<VertexAttribute>([&](VertexAttribute attr) {
        auto&      buffer = data[attr];
        const auto stride = get_vertex_attribute_size(attr);
        if (!buffer.Empty() && (buffer.GetDataSize() % stride || buffer.GetDataSize() / stride != vertex_count))
            throw std::invalid_argument("Vertex attribute count mismatch");
        m_Attributes[attr].type       = attr;
        m_Attributes[attr].cpu_buffer = std::move(buffer);
    });
}

template <VertexAttribute T>
auto VertexArray::Span() const noexcept -> std::span<const VertexDataType<T>> {
    if (m_Attributes[T].cpu_buffer.Empty()) return {};

    return std::span<const VertexDataType<T>>(
        reinterpret_cast<const VertexDataType<T>*>(m_Attributes[T].cpu_buffer.GetData()),
        m_VertexCount);
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

void VertexArray::Load(const ResourceLoadContext& context) {
    if (GetLoadState() == ResourceLoadState::Loaded) return;
    for (auto& attribute : m_Attributes) {
        if (attribute.cpu_buffer.Empty()) continue;
        attribute.gpu_buffer = hitagi::gfx::GPUBuffer::Create(
            context.device,
            {
                .name   = std::pmr::string(std::format("{}-{}", m_Name, magic_enum::enum_name(attribute.type))),
                .size   = attribute.cpu_buffer.GetDataSize(),
                .usages = gfx::GPUBufferUsageFlags::Vertex | gfx::GPUBufferUsageFlags::CopyDst,
            },
            attribute.cpu_buffer.Span<const std::byte>());
    }
    SetLoadState(ResourceLoadState::Loaded);
}

void VertexArray::Unload() {
    if (GetLoadState() != ResourceLoadState::Loaded) return;
    for (auto& attribute : m_Attributes) {
        attribute.gpu_buffer = nullptr;
    }
    SetLoadState(ResourceLoadState::Unloaded);
}

IndexArray::IndexArray(IndexType type, core::Buffer data, std::string_view name)
    : Resource(Type::Index, name), m_Data{.type = type, .cpu_buffer = std::move(data)} {
    if (type != IndexType::UINT16 && type != IndexType::UINT32)
        throw std::invalid_argument("Invalid index type");

    const auto stride = get_index_type_size(type);
    if (m_Data.cpu_buffer.GetDataSize() % stride) throw std::invalid_argument("Index data size mismatch");
    m_IndexCount = m_Data.cpu_buffer.GetDataSize() / stride;
}

template <IndexType T>
auto IndexArray::Span() const noexcept -> std::span<const IndexDataType<T>> {
    if (T != m_Data.type || m_Data.cpu_buffer.Empty()) return {};

    return std::span<const IndexDataType<T>>(
        reinterpret_cast<const IndexDataType<T>*>(m_Data.cpu_buffer.GetData()),
        m_IndexCount);
}

void IndexArray::Load(const ResourceLoadContext& context) {
    if (GetLoadState() == ResourceLoadState::Loaded) return;
    if (m_Data.cpu_buffer.Empty()) {
        throw std::invalid_argument("Index array is empty");
    }
    m_Data.gpu_buffer = hitagi::gfx::GPUBuffer::Create(
        context.device,
        {
            .name   = m_Name,
            .size   = m_Data.cpu_buffer.GetDataSize(),
            .usages = gfx::GPUBufferUsageFlags::Index | gfx::GPUBufferUsageFlags::CopyDst,
        },
        m_Data.cpu_buffer.Span<const std::byte>());
    SetLoadState(ResourceLoadState::Loaded);
}

void IndexArray::Unload() {
    if (GetLoadState() != ResourceLoadState::Loaded) return;
    m_Data.gpu_buffer = nullptr;
    SetLoadState(ResourceLoadState::Unloaded);
}

Mesh::Mesh(std::shared_ptr<VertexArray> vertices, std::shared_ptr<IndexArray> indices,
           std::pmr::vector<SubMesh> sub_meshes, std::string_view name)
    : Resource(Type::Mesh, name), sub_meshes(std::move(sub_meshes)), vertices(std::move(vertices)), indices(std::move(indices)) {
    if (this->sub_meshes.empty()) return;
    if (!this->vertices || !this->indices || this->vertices->Span<VertexAttribute::Position>().empty())
        throw std::invalid_argument("Mesh requires positions and indices");
    for (const auto& sub_mesh : this->sub_meshes) {
        if (!sub_mesh.index_count ||
            sub_mesh.index_offset > this->indices->Size() ||
            sub_mesh.index_count > this->indices->Size() - sub_mesh.index_offset ||
            sub_mesh.vertex_offset >= this->vertices->Size()) {
            throw std::out_of_range("Invalid submesh range");
        }

        const auto validate = [&](auto values) {
            for (auto index : values.subspan(sub_mesh.index_offset, sub_mesh.index_count))
                if (index >= this->vertices->Size() - sub_mesh.vertex_offset)
                    throw std::out_of_range("Submesh index exceeds vertex range");
        };

        if (this->indices->Type() == IndexType::UINT16)
            validate(this->indices->Span<IndexType::UINT16>());
        else
            validate(this->indices->Span<IndexType::UINT32>());
    }
}

void Mesh::Load(const ResourceLoadContext& context) {
    if (vertices) vertices->Load(context);
    if (indices) indices->Load(context);
    SetLoadState(ResourceLoadState::Loaded);
}

void Mesh::Unload() {
    if (GetLoadState() != ResourceLoadState::Loaded) return;
    if (vertices) vertices->Unload();
    if (indices) indices->Unload();
    SetLoadState(ResourceLoadState::Unloaded);
}

}  // namespace hitagi::asset
