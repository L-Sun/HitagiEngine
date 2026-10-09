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
        bool                            dirty      = true;
        core::Buffer                    cpu_buffer = {};
        std::shared_ptr<gfx::GPUBuffer> gpu_buffer = nullptr;
    };

    VertexArray(std::size_t vertex_count, std::string_view name = "");
    VertexArray(VertexArray&&)            = default;
    VertexArray& operator=(VertexArray&&) = default;

    bool        Empty() const noexcept;
    inline auto Size() const noexcept { return m_VertexCount; }
    auto        GetAttributeData(VertexAttribute attr) const noexcept -> utils::optional_ref<const AttributeData>;
    auto        GetAttributeData(const gfx::VertexAttribute& attr) const noexcept -> utils::optional_ref<const AttributeData>;

    template <VertexAttribute T>
    auto Span() const noexcept -> std::span<const VertexDataType<T>>;
    template <VertexAttribute T>
    auto Span() noexcept -> std::span<VertexDataType<T>>;
    template <VertexAttribute T>
    void Modify(std::function<void(std::span<VertexDataType<T>>)> modifier);
    void Resize(std::size_t new_count);

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
        bool                            dirty      = true;
        core::Buffer                    cpu_buffer = {};
        std::shared_ptr<gfx::GPUBuffer> gpu_buffer = nullptr;
    };

    IndexArray(std::size_t count, IndexType type = IndexType::UINT16, std::string_view name = "");

    IndexArray(IndexArray&&)            = default;
    IndexArray& operator=(IndexArray&&) = default;

    inline std::size_t Empty() const noexcept { return m_IndexCount == 0; }
    inline auto        Size() const noexcept { return m_IndexCount; }
    inline auto        Type() const noexcept { return m_Data.type; }
    inline const auto& GetIndexData() const noexcept { return m_Data; }
    inline auto        GetGPUData() const noexcept { return m_Data.gpu_buffer; }

    template <IndexType T>
    auto Span() const noexcept -> std::span<const IndexDataType<T>>;
    template <IndexType T>
    auto Span() noexcept -> std::span<IndexDataType<T>>;
    template <IndexType T>
    void Modify(std::function<void(std::span<IndexDataType<T>>)> modifier);
    void Resize(std::size_t new_count);

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

    Mesh(std::string_view name = "") : Resource(Type::Mesh, name) {}
    Mesh(std::shared_ptr<VertexArray> vertices, std::shared_ptr<IndexArray> indices, std::string_view name = "");

    void AddSubMesh(const SubMesh& sub_mesh);
    void ComputeAABB();
    bool Empty() const noexcept { return vertices == nullptr || indices == nullptr || sub_meshes.empty(); }

    void Load(const ResourceLoadContext& context) final;
    void Unload() final;

    std::pmr::vector<SubMesh>    sub_meshes;
    std::shared_ptr<VertexArray> vertices;
    std::shared_ptr<IndexArray>  indices;
    math::AABBf                  aabb;
};

struct MeshComponent {
    std::shared_ptr<Mesh> mesh;
};

struct MeshFactory {
    static auto Cube() -> std::shared_ptr<Mesh>;
};

}  // namespace hitagi::asset

namespace hitagi::asset {
VertexArray::VertexArray(std::size_t vertex_count, std::string_view name)
    : Resource(Type::Vertex, name),
      m_VertexCount(vertex_count) {
    magic_enum::enum_for_each<VertexAttribute>([this](VertexAttribute attr) {
        m_Attributes[attr].type = attr;
    });
}

template <VertexAttribute T>
auto VertexArray::Span() const noexcept -> std::span<const VertexDataType<T>> {
    if (m_Attributes[T].cpu_buffer.Empty()) return {};

    return std::span<const VertexDataType<T>>(
        reinterpret_cast<const VertexDataType<T>*>(m_Attributes[T].cpu_buffer.GetData()),
        m_VertexCount);
}

template <VertexAttribute T>
auto VertexArray::Span() noexcept -> std::span<VertexDataType<T>> {
    if (m_Attributes[T].cpu_buffer.Empty()) return {};

    return std::span<VertexDataType<T>>(
        reinterpret_cast<VertexDataType<T>*>(m_Attributes[T].cpu_buffer.GetData()),
        m_VertexCount);
}

template <VertexAttribute T>
void VertexArray::Modify(std::function<void(std::span<VertexDataType<T>>)> modifier) {
    auto& attribute = m_Attributes[T];
    if (attribute.cpu_buffer.Empty() && m_VertexCount != 0) {
        attribute.cpu_buffer = core::Buffer(m_VertexCount * sizeof(VertexDataType<T>));
    }

    modifier(std::span<VertexDataType<T>>(
        reinterpret_cast<VertexDataType<T>*>(attribute.cpu_buffer.GetData()),
        attribute.cpu_buffer.Empty() ? 0 : m_VertexCount));
    attribute.dirty = true;
}

void VertexArray::Resize(std::size_t new_count) {
    if (new_count == m_VertexCount) return;

    for (auto& attribute : m_Attributes) {
        if (attribute.cpu_buffer.Empty()) continue;

        const auto new_size = new_count * get_vertex_attribute_size(attribute.type);
        if (new_size == 0)
            attribute.cpu_buffer = {};
        else
            attribute.cpu_buffer.Resize(new_size);
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

void VertexArray::Load(const ResourceLoadContext& context) {
    if (GetLoadState() == ResourceLoadState::Loaded && std::ranges::none_of(m_Attributes, [](const auto& attribute) { return attribute.dirty; })) return;
    for (auto& attribute : m_Attributes) {
        if (attribute.cpu_buffer.Empty() || (!attribute.dirty && attribute.gpu_buffer != nullptr)) continue;
        attribute.gpu_buffer = hitagi::gfx::GPUBuffer::Create(context.device,
                                                              {
                                                                  .name   = std::pmr::string(std::format("{}-{}", m_Name, magic_enum::enum_name(attribute.type))),
                                                                  .size   = attribute.cpu_buffer.GetDataSize(),
                                                                  .usages = gfx::GPUBufferUsageFlags::Vertex | gfx::GPUBufferUsageFlags::CopyDst,
                                                              },
                                                              attribute.cpu_buffer.Span<const std::byte>());
        attribute.dirty = false;
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

IndexArray::IndexArray(std::size_t count, IndexType type, std::string_view name)
    : Resource(Type::Index, name),
      m_IndexCount(count),
      m_Data{
          .type       = type,
          .cpu_buffer = core::Buffer(count * get_index_type_size(type), nullptr),
      } {
}

template <IndexType T>
auto IndexArray::Span() const noexcept -> std::span<const IndexDataType<T>> {
    if (T != m_Data.type || m_Data.cpu_buffer.Empty()) return {};

    return std::span<const IndexDataType<T>>(
        reinterpret_cast<const IndexDataType<T>*>(m_Data.cpu_buffer.GetData()),
        m_IndexCount);
}

template <IndexType T>
auto IndexArray::Span() noexcept -> std::span<IndexDataType<T>> {
    if (T != m_Data.type || m_Data.cpu_buffer.Empty()) return {};

    return std::span<IndexDataType<T>>(
        reinterpret_cast<IndexDataType<T>*>(m_Data.cpu_buffer.GetData()),
        m_IndexCount);
}

template <IndexType T>
void IndexArray::Modify(std::function<void(std::span<IndexDataType<T>>)> modifier) {
    if (T != m_Data.type) {
        modifier({});
        return;
    }
    if (m_Data.cpu_buffer.Empty() && m_IndexCount != 0) {
        m_Data.cpu_buffer = core::Buffer(m_IndexCount * sizeof(IndexDataType<T>));
    }

    modifier(std::span<IndexDataType<T>>(
        reinterpret_cast<IndexDataType<T>*>(m_Data.cpu_buffer.GetData()),
        m_Data.cpu_buffer.Empty() ? 0 : m_IndexCount));
    m_Data.dirty = true;
}

void IndexArray::Resize(std::size_t new_count) {
    if (new_count == m_IndexCount) return;

    const auto new_size = new_count * get_index_type_size(m_Data.type);
    if (new_size == 0)
        m_Data.cpu_buffer = {};
    else
        m_Data.cpu_buffer.Resize(new_size);
    m_IndexCount = new_count;
    m_Data.dirty = true;
}

void IndexArray::Load(const ResourceLoadContext& context) {
    if (GetLoadState() == ResourceLoadState::Loaded && !m_Data.dirty) return;
    if (m_Data.cpu_buffer.Empty()) {
        throw std::invalid_argument("Index array is empty");
    }
    m_Data.gpu_buffer = hitagi::gfx::GPUBuffer::Create(context.device,
                                                       {
                                                           .name   = m_Name,
                                                           .size   = m_Data.cpu_buffer.GetDataSize(),
                                                           .usages = gfx::GPUBufferUsageFlags::Index | gfx::GPUBufferUsageFlags::CopyDst,
                                                       },
                                                       m_Data.cpu_buffer.Span<const std::byte>());
    m_Data.dirty = false;
    SetLoadState(ResourceLoadState::Loaded);
}

void IndexArray::Unload() {
    if (GetLoadState() != ResourceLoadState::Loaded) return;
    m_Data.gpu_buffer = nullptr;
    SetLoadState(ResourceLoadState::Unloaded);
}

Mesh::Mesh(std::shared_ptr<VertexArray> vertices, std::shared_ptr<IndexArray> indices, std::string_view name)
    : Resource(Type::Mesh, name),
      vertices(std::move(vertices)),
      indices(std::move(indices)) {
    ComputeAABB();
}

void Mesh::ComputeAABB() {
    aabb = {};
    if (!vertices) return;
    for (const auto& position : vertices->Span<VertexAttribute::Position>()) {
        aabb.Expand(position);
    }
}

void Mesh::AddSubMesh(const SubMesh& sub_mesh) {
    if (sub_mesh.index_count == 0) return;
    if (sub_mesh.index_offset + sub_mesh.index_count > indices->Size()) {
        throw std::out_of_range("Sub mesh index out of range");
    }
    if (sub_mesh.vertex_offset > vertices->Size()) {
        throw std::out_of_range("Sub mesh vertex out of range");
    }
    sub_meshes.emplace_back(sub_mesh);
}

void Mesh::Load(const ResourceLoadContext& context) {
    if (vertices) vertices->Load(context);
    if (indices) indices->Load(context);
    SetLoadState(ResourceLoadState::Loaded);
}

void Mesh::Unload() {
    if (GetLoadState() != ResourceLoadState::Loaded) return;
    vertices->Unload();
    indices->Unload();
    SetLoadState(ResourceLoadState::Unloaded);
}

auto MeshFactory::Cube() -> std::shared_ptr<Mesh> {
    constexpr std::array positions{
        math::vec3f{-0.5f, -0.5f, 0.5f},
        math::vec3f{0.5f, -0.5f, 0.5f},
        math::vec3f{0.5f, 0.5f, 0.5f},
        math::vec3f{-0.5f, 0.5f, 0.5f},
        math::vec3f{0.5f, -0.5f, -0.5f},
        math::vec3f{-0.5f, -0.5f, -0.5f},
        math::vec3f{-0.5f, 0.5f, -0.5f},
        math::vec3f{0.5f, 0.5f, -0.5f},
        math::vec3f{-0.5f, -0.5f, -0.5f},
        math::vec3f{-0.5f, -0.5f, 0.5f},
        math::vec3f{-0.5f, 0.5f, 0.5f},
        math::vec3f{-0.5f, 0.5f, -0.5f},
        math::vec3f{0.5f, -0.5f, 0.5f},
        math::vec3f{0.5f, -0.5f, -0.5f},
        math::vec3f{0.5f, 0.5f, -0.5f},
        math::vec3f{0.5f, 0.5f, 0.5f},
        math::vec3f{-0.5f, 0.5f, 0.5f},
        math::vec3f{0.5f, 0.5f, 0.5f},
        math::vec3f{0.5f, 0.5f, -0.5f},
        math::vec3f{-0.5f, 0.5f, -0.5f},
        math::vec3f{-0.5f, -0.5f, -0.5f},
        math::vec3f{0.5f, -0.5f, -0.5f},
        math::vec3f{0.5f, -0.5f, 0.5f},
        math::vec3f{-0.5f, -0.5f, 0.5f},
    };
    constexpr std::array normals{
        math::vec3f{0.0f, 0.0f, 1.0f},
        math::vec3f{0.0f, 0.0f, 1.0f},
        math::vec3f{0.0f, 0.0f, 1.0f},
        math::vec3f{0.0f, 0.0f, 1.0f},
        math::vec3f{0.0f, 0.0f, -1.0f},
        math::vec3f{0.0f, 0.0f, -1.0f},
        math::vec3f{0.0f, 0.0f, -1.0f},
        math::vec3f{0.0f, 0.0f, -1.0f},
        math::vec3f{-1.0f, 0.0f, 0.0f},
        math::vec3f{-1.0f, 0.0f, 0.0f},
        math::vec3f{-1.0f, 0.0f, 0.0f},
        math::vec3f{-1.0f, 0.0f, 0.0f},
        math::vec3f{1.0f, 0.0f, 0.0f},
        math::vec3f{1.0f, 0.0f, 0.0f},
        math::vec3f{1.0f, 0.0f, 0.0f},
        math::vec3f{1.0f, 0.0f, 0.0f},
        math::vec3f{0.0f, 1.0f, 0.0f},
        math::vec3f{0.0f, 1.0f, 0.0f},
        math::vec3f{0.0f, 1.0f, 0.0f},
        math::vec3f{0.0f, 1.0f, 0.0f},
        math::vec3f{0.0f, -1.0f, 0.0f},
        math::vec3f{0.0f, -1.0f, 0.0f},
        math::vec3f{0.0f, -1.0f, 0.0f},
        math::vec3f{0.0f, -1.0f, 0.0f},
    };
    constexpr std::array uvs{
        math::vec2f{0.0f, 1.0f},
        math::vec2f{1.0f, 1.0f},
        math::vec2f{1.0f, 0.0f},
        math::vec2f{0.0f, 0.0f},
        math::vec2f{0.0f, 1.0f},
        math::vec2f{1.0f, 1.0f},
        math::vec2f{1.0f, 0.0f},
        math::vec2f{0.0f, 0.0f},
        math::vec2f{0.0f, 1.0f},
        math::vec2f{1.0f, 1.0f},
        math::vec2f{1.0f, 0.0f},
        math::vec2f{0.0f, 0.0f},
        math::vec2f{0.0f, 1.0f},
        math::vec2f{1.0f, 1.0f},
        math::vec2f{1.0f, 0.0f},
        math::vec2f{0.0f, 0.0f},
        math::vec2f{0.0f, 1.0f},
        math::vec2f{1.0f, 1.0f},
        math::vec2f{1.0f, 0.0f},
        math::vec2f{0.0f, 0.0f},
        math::vec2f{0.0f, 1.0f},
        math::vec2f{1.0f, 1.0f},
        math::vec2f{1.0f, 0.0f},
        math::vec2f{0.0f, 0.0f},
    };
    constexpr std::array<std::uint16_t, 36> indices{
        0,
        1,
        2,
        2,
        3,
        0,
        4,
        5,
        6,
        6,
        7,
        4,
        8,
        9,
        10,
        10,
        11,
        8,
        12,
        13,
        14,
        14,
        15,
        12,
        16,
        17,
        18,
        18,
        19,
        16,
        20,
        21,
        22,
        22,
        23,
        20,
    };

    auto mesh = std::make_shared<Mesh>(
        std::make_shared<VertexArray>(positions.size(), "cube"),
        std::make_shared<IndexArray>(indices.size(), IndexType::UINT16, "cube"),
        "cube");

    mesh->vertices->Modify<VertexAttribute::Position>([&](auto data) { std::ranges::copy(positions, data.begin()); });
    mesh->vertices->Modify<VertexAttribute::Normal>([&](auto data) { std::ranges::copy(normals, data.begin()); });
    mesh->vertices->Modify<VertexAttribute::UV0>([&](auto data) { std::ranges::copy(uvs, data.begin()); });
    mesh->indices->Modify<IndexType::UINT16>([&](auto data) { std::ranges::copy(indices, data.begin()); });
    mesh->AddSubMesh({
        .index_count = indices.size(),
    });
    mesh->ComputeAABB();
    return mesh;
}
}  // namespace hitagi::asset
