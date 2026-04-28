module;

#include <concepts>
#include <spdlog/logger.h>

export module asset;
import std;
import core;
import utils;
import math;
import gfx;
import ecs;

export namespace hitagi::asset {

class Resource {
public:
    enum struct Type : std::uint8_t {
        Scene,
        SceneNode,
        Vertex,
        Index,
        Mesh,
        Texture,
        Material,
        MaterialInstance,
        Camera,
        Light,
        Skeleton,
    };

    Resource(Type type, std::string_view name = "") : m_Type(type), m_Name(name), m_UUID(utils::UUID::Create()) {}

    Resource(const Resource&);
    Resource& operator=(const Resource&);

    Resource& operator=(Resource&&) = default;
    Resource(Resource&&)            = default;

    inline auto        GetType() const noexcept { return m_Type; }
    inline auto        GetName() const noexcept -> std::string_view { return m_Name; }
    inline const auto& GetUUID() const noexcept { return m_UUID; }
    auto               GetUniqueName() const noexcept -> std::pmr::string;

    inline void SetName(std::string_view name) noexcept { m_Name = name; }

protected:
    Type             m_Type;
    std::pmr::string m_Name;
    utils::UUID      m_UUID;
};

class ImageDecoder;

class Texture : public Resource {
public:
    Texture(std::uint32_t    width,
            std::uint32_t    height,
            gfx::Format      format,
            core::Buffer     data = {},
            std::string_view name = "");

    Texture(std::filesystem::path path, std::string_view name = "");

    Texture(const Texture&);
    Texture& operator=(const Texture&);
    Texture(Texture&&)            = default;
    Texture& operator=(Texture&&) = default;

    static auto DefaultTexture() -> std::shared_ptr<Texture>;
    static void DestroyDefaultTexture();

    inline auto  Empty() const noexcept { return m_CPUData.Empty(); }
    inline auto  Width() const noexcept { return m_Width; }
    inline auto  Height() const noexcept { return m_Height; }
    inline auto  Format() const noexcept { return m_Format; }
    inline auto  GetData() const noexcept { return m_CPUData.Span<const std::byte>(); }
    inline auto& GetPath() const noexcept { return m_Path; }
    inline auto  GetGPUData() const noexcept { return m_GPUData; }

    bool SetPath(const std::filesystem::path& path);
    bool Load(const std::shared_ptr<ImageDecoder>& decoder);
    void Unload();

    void InitGPUData(gfx::Device& device);

private:
    std::uint32_t m_Width = 0, m_Height = 0;
    gfx::Format   m_Format = gfx::Format::UNKNOWN;

    std::filesystem::path m_Path;

    bool                          m_Dirty   = true;
    core::Buffer                  m_CPUData = {};
    std::shared_ptr<gfx::Texture> m_GPUData = nullptr;

    static std::shared_ptr<Texture> m_DefaultTexture;
};

using MaterialParameterValue = std::variant<
    float,
    std::int32_t,
    std::uint32_t,
    math::vec2i,
    math::vec2u,
    math::vec2f,
    math::vec3i,
    math::vec3u,
    math::vec3f,
    math::vec4i,
    math::vec4u,
    math::vec4f,
    math::Color,
    math::mat4f,
    std::shared_ptr<Texture>>;

template <typename T>
concept MaterialParametric = requires(const MaterialParameterValue& parameter) {
    { std::get<T>(parameter) } -> std::same_as<const T&>;
};

class MaterialInstance;

struct MaterialParameter {
    std::pmr::string       name;
    MaterialParameterValue value;

    inline bool operator==(const MaterialParameter& rhs) const noexcept { return name == rhs.name && value == rhs.value; }
};

using MaterialParameters = std::pmr::vector<MaterialParameter>;

struct MaterialDesc {
    std::pmr::vector<gfx::ShaderDesc> shaders;
    gfx::RenderPipelineDesc           pipeline;
    MaterialParameters                parameters;
};

class Material : public Resource, public std::enable_shared_from_this<Material> {
public:
    Material(MaterialDesc desc, std::string_view name = "");

    Material(const Material&)            = delete;
    Material(Material&&)                 = delete;
    Material& operator=(const Material&) = delete;
    Material& operator=(Material&&)      = delete;

    auto               CreateInstance() -> std::shared_ptr<MaterialInstance>;
    inline const auto& GetInstances() const noexcept { return m_Instances; }
    inline const auto& GetDefaultParameters() const noexcept { return m_Desc.parameters; }
    auto               CalculateMaterialBufferSize(bool enable_16_bytes_packing) const noexcept -> std::size_t;
    auto               GetPipeline(gfx::Device& device) const -> std::shared_ptr<gfx::RenderPipeline>;

    template <MaterialParametric>
    bool HasParameter(std::string_view name) const noexcept;

protected:
    friend MaterialInstance;

    void AddInstance(MaterialInstance* instance) noexcept;
    void RemoveInstance(MaterialInstance* instance) noexcept;

    std::pmr::unordered_set<MaterialInstance*> m_Instances;

    MaterialDesc                                           m_Desc;
    mutable std::pmr::vector<std::shared_ptr<gfx::Shader>> m_Shaders;
    mutable std::shared_ptr<gfx::RenderPipeline>           m_Pipeline = nullptr;
};

struct SplitMaterialParameters {
    MaterialParameters only_in_instance;
    MaterialParameters only_in_material;
    MaterialParameters in_both;
};

class MaterialInstance : public Resource {
public:
    MaterialInstance(MaterialParameters parameters = {}, std::string_view name = "");
    MaterialInstance(const MaterialInstance&);
    MaterialInstance& operator=(const MaterialInstance&);
    MaterialInstance(MaterialInstance&&) noexcept = default;
    MaterialInstance& operator=(MaterialInstance&&) noexcept;
    ~MaterialInstance();

    void        SetMaterial(std::shared_ptr<Material> material);
    inline auto GetMaterial() const noexcept { return m_Material; }

    inline const auto& GetParameters() const noexcept { return m_Parameters; }
    inline auto&       GetParameters() noexcept { return m_Parameters; }

    void SetParameter(MaterialParameter parameter) noexcept;
    template <MaterialParametric T>
    void SetParameter(std::string_view name, T value) noexcept;
    template <MaterialParametric T>
    auto GetParameter(std::string_view name) const noexcept -> std::optional<T>;
    auto GetSplitParameters() const noexcept -> SplitMaterialParameters;
    auto GetAssociatedTextures() const noexcept -> std::pmr::vector<std::shared_ptr<Texture>>;
    auto GenerateMaterialBuffer(bool enable_16_bytes_packing) const noexcept -> core::Buffer;

    template <MaterialParametric T>
    bool HasParameter(std::string_view name) const noexcept;

private:
    std::shared_ptr<Material> m_Material = nullptr;
    MaterialParameters        m_Parameters;
};

template <MaterialParametric T>
bool Material::HasParameter(std::string_view name) const noexcept {
    for (const auto& param : m_Desc.parameters) {
        if (param.name == name && std::holds_alternative<T>(param.value)) {
            return true;
        }
    }
    return false;
}

template <MaterialParametric T>
void MaterialInstance::SetParameter(std::string_view name, T value) noexcept {
    SetParameter(MaterialParameter{.name = std::pmr::string(name), .value = value});
}

template <MaterialParametric T>
auto MaterialInstance::GetParameter(std::string_view name) const noexcept -> std::optional<T> {
    for (const auto& param : m_Parameters) {
        if (param.name == name && std::holds_alternative<T>(param.value)) {
            return std::get<T>(param.value);
        }
    }
    return std::nullopt;
}

template <MaterialParametric T>
bool MaterialInstance::HasParameter(std::string_view name) const noexcept {
    for (const auto& param : m_Parameters) {
        if (param.name == name && std::holds_alternative<T>(param.value)) {
            return true;
        }
    }
    return false;
}

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
    Mesh operator+(const Mesh& rhs) const;

    void AddSubMesh(const SubMesh& sub_mesh);
    void ComputeAABB();
    bool Empty() const noexcept { return vertices == nullptr || indices == nullptr || sub_meshes.empty(); }

    std::pmr::vector<SubMesh>    sub_meshes;
    std::shared_ptr<VertexArray> vertices;
    std::shared_ptr<IndexArray>  indices;
    math::AABBf                  aabb;
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

class Camera : public Resource {
public:
    struct Parameters {
        float aspect         = 16.0f / 9.0f;
        float near_clip      = 1.0f;
        float far_clip       = 1000.0f;
        float horizontal_fov = 60.0_deg;

        math::vec3f eye      = {0.0f, -1.0f, 0.0f};
        math::vec3f look_dir = {0.0f, 1.0f, 0.0f};
        math::vec3f up       = {0.0f, 0.0f, 1.0f};
    } parameters;

    Camera(Parameters parameters, std::string_view name = "")
        : Resource(Type::Camera, name), parameters(parameters) {}
};

struct CameraComponent {
    std::shared_ptr<Camera> camera;
};

class Light : public Resource {
public:
    enum struct Type : std::uint8_t {
        Point,
        Spot,
        Direction,
    };
    struct Parameters {
        Type        type             = Type::Spot;
        float       intensity        = 1.0f;
        math::Color color            = math::Color::White();
        math::vec3f position         = {3.0f, 3.0f, 3.0f};
        math::vec3f direction        = {0.0f, -1.0f, 0.0f};
        math::vec3f up               = {0.0f, 1.0f, 0.0f};
        float       inner_cone_angle = 30.0_deg;
        float       outer_cone_angle = 60.0_deg;
    } parameters;

    Light(Parameters parameters, std::string_view name = "")
        : Resource(Resource::Type::Light, name), parameters(parameters) {}
};

struct LightComponent {
    std::shared_ptr<Light> light;
};

struct Bone {
    std::pmr::string                        name;
    std::pmr::vector<std::shared_ptr<Bone>> children;
    std::weak_ptr<Bone>                     parent;

    math::mat4f offset_matrix;
};

struct Skeleton : public Resource {
    Skeleton(std::string_view name = "") : Resource(Type::Skeleton, name) {}

    std::pmr::vector<std::shared_ptr<Bone>> bones;
};

struct SkeletonComponent {
    std::shared_ptr<Skeleton> skeleton;
};

struct MetaInfo {
    MetaInfo(std::string_view name) : name(name) {}

    std::pmr::string name;
};
static_assert(ecs::Component<MetaInfo>);

struct RelationShip {
    RelationShip(ecs::Entity parent = {}) : parent(parent) {}

    ecs::Entity parent;

    const auto& GetChildren() const noexcept { return children; }

private:
    friend struct RelationShipSystem;
    friend struct TransformSystem;
    ecs::Entity                          prev_parent = {};
    std::pmr::unordered_set<ecs::Entity> children;
    bool                                 subtree_dirty = true;
};
static_assert(ecs::Component<RelationShip>);

struct RelationShipSystem {
    static void OnUpdate(ecs::Schedule& schedule);
};

struct Transform {
    Transform(math::vec3f position = math::vec3f(0.0f), math::quatf rotation = math::quatf::identity(), math::vec3f scaling = math::vec3f(1.0f))
        : position(position),
          rotation(rotation),
          scaling(scaling),
          local_matrix(math::translate(position) * math::rotate(rotation) * math::scale(scaling)),
          world_matrix(local_matrix),
          cached_position(position),
          cached_rotation(rotation),
          cached_scaling(scaling) {}

    math::vec3f position;
    math::quatf rotation;
    math::vec3f scaling;

    math::mat4f local_matrix;
    math::mat4f world_matrix;

    inline void ApplyScale(float value) noexcept { scaling = value; }
    inline void Translate(const math::vec3f& value) noexcept { position += value; }
    inline void Rotate(const math::quatf& value) noexcept { rotation = value * rotation; }

    inline auto ToMatrix() const noexcept { return math::translate(position) * math::rotate(rotation) * math::scale(scaling); }

private:
    friend struct TransformSystem;
    math::vec3f cached_position;
    math::quatf cached_rotation;
    math::vec3f cached_scaling;
};

struct TransformSystem {
    static void OnUpdate(ecs::Schedule& schedule);
};

class Scene : public Resource {
public:
    Scene(std::string_view name = "");

    void Update();

    auto CreateEmptyEntity(math::mat4f transform, ecs::Entity parent, std::string_view name) -> ecs::Entity;
    auto CreateMeshEntity(std::shared_ptr<Mesh> mesh, math::mat4f transform, ecs::Entity parent, std::string_view name) -> ecs::Entity;
    auto CreateCameraEntity(std::shared_ptr<Camera> camera, math::mat4f transform, ecs::Entity parent, std::string_view name) -> ecs::Entity;
    auto CreateLightEntity(std::shared_ptr<Light> light, math::mat4f transform, ecs::Entity parent, std::string_view name) -> ecs::Entity;
    auto CreateSkeletonEntity(std::shared_ptr<Skeleton> skeleton, math::mat4f transform, ecs::Entity parent, std::string_view name) -> ecs::Entity;

    auto& GetRootEntity() noexcept { return m_RootEntity; }
    auto& GetMeshEntities() noexcept { return m_MeshEntities; }
    auto& GetCameraEntities() noexcept { return m_CameraEntities; }
    auto& GetLightEntities() noexcept { return m_LightEntities; }

    auto GetCurrentCamera() const noexcept { return m_CurrentCamera; }

    auto GetWorld() noexcept -> ecs::World& { return m_World; }

private:
    ecs::World m_World;

    ecs::Entity m_CurrentCamera;

    ecs::Entity                   m_RootEntity;
    std::pmr::vector<ecs::Entity> m_MeshEntities;
    std::pmr::vector<ecs::Entity> m_CameraEntities;
    std::pmr::vector<ecs::Entity> m_LightEntities;
};

class MeshFactory {
public:
    static auto Line(const math::vec3f& from, const math::vec3f& to) -> std::shared_ptr<Mesh>;
    static auto BoxWireframe(const math::vec3f& bb_min, const math::vec3f& bb_max) -> std::shared_ptr<Mesh>;
    static auto Cube() -> std::shared_ptr<Mesh>;
};

enum struct ImageFormat : std::uint8_t {
    UNKOWN,
    PNG,
    JPEG,
    TGA,
    BMP,
};

inline constexpr ImageFormat get_image_format(std::string_view ext) noexcept {
    if (ext == ".jpeg" || ext == ".jpg")
        return ImageFormat::JPEG;
    else if (ext == ".bmp")
        return ImageFormat::BMP;
    else if (ext == ".tga")
        return ImageFormat::TGA;
    else if (ext == ".png")
        return ImageFormat::PNG;
    return ImageFormat::UNKOWN;
}

class ImageDecoder {
public:
    ImageDecoder(std::shared_ptr<spdlog::logger> logger = nullptr) : m_Logger(std::move(logger)) {}

    virtual auto Decode(const std::filesystem::path& path) -> std::shared_ptr<Texture>;
    virtual auto Decode(const core::Buffer& buffer) -> std::shared_ptr<Texture> = 0;
    virtual ~ImageDecoder()                                                     = default;

protected:
    std::shared_ptr<spdlog::logger> m_Logger;
};

class ImageEncoder {
public:
    ImageEncoder(std::shared_ptr<spdlog::logger> logger = nullptr) : m_Logger(std::move(logger)) {}

    virtual auto Encode(const Texture& texture, const std::filesystem::path& path) -> bool;
    virtual auto Encode(const Texture& texture) -> core::Buffer = 0;
    virtual ~ImageEncoder()                                     = default;

protected:
    std::shared_ptr<spdlog::logger> m_Logger;
};

class PngDecoder : public ImageDecoder {
public:
    using ImageDecoder::Decode;
    using ImageDecoder::ImageDecoder;
    auto Decode(const core::Buffer& buffer) -> std::shared_ptr<Texture> final;
};

class PngEncoder : public ImageEncoder {
public:
    using ImageEncoder::Encode;
    using ImageEncoder::ImageEncoder;
    auto Encode(const Texture& texture) -> core::Buffer final;
};

class JpegDecoder : public ImageDecoder {
public:
    using ImageDecoder::Decode;
    using ImageDecoder::ImageDecoder;
    auto Decode(const core::Buffer& buffer) -> std::shared_ptr<Texture> final;
};

class BmpDecoder : public ImageDecoder {
public:
    using ImageDecoder::Decode;
    using ImageDecoder::ImageDecoder;
    auto Decode(const core::Buffer& buffer) -> std::shared_ptr<Texture> final;
};

class TgaDecoder : public ImageDecoder {
public:
    using ImageDecoder::Decode;
    using ImageDecoder::ImageDecoder;
    auto Decode(const core::Buffer& buffer) -> std::shared_ptr<Texture> final;
};

class AssetManager;

enum struct SceneFormat : std::uint8_t {
    UNKOWN,
    GLTF,
    GLB,
    BLEND,
    FBX,
};

inline constexpr SceneFormat get_scene_format(std::string_view ext) noexcept {
    if (ext == ".gltf")
        return SceneFormat::GLTF;
    if (ext == "glb")
        return SceneFormat::GLB;
    else if (ext == ".blend")
        return SceneFormat::BLEND;
    else if (ext == ".fbx")
        return SceneFormat::FBX;
    return SceneFormat::UNKOWN;
}

class SceneParser {
public:
    SceneParser(std::shared_ptr<spdlog::logger> logger = nullptr) : m_Logger(std::move(logger)) {}

    virtual auto Parse(const std::filesystem::path& path, const std::filesystem::path& resource_base_path = {}) -> std::shared_ptr<Scene> = 0;

    virtual ~SceneParser() = default;

protected:
    std::shared_ptr<spdlog::logger> m_Logger;
};

class AssimpParser : public SceneParser {
public:
    AssimpParser(utils::EnumArray<std::shared_ptr<ImageDecoder>, ImageFormat>      image_decoders,
                 std::function<std::shared_ptr<asset::Material>(std::string_view)> material_getter = {},
                 std::shared_ptr<spdlog::logger>                                   logger          = nullptr)
        : SceneParser(std::move(logger)), m_ImageDecoders(std::move(image_decoders)), m_MaterialGetter(std::move(material_getter)) {}

    auto Parse(const std::filesystem::path& path, const std::filesystem::path& resource_base_path = {}) -> std::shared_ptr<Scene> final;

private:
    utils::EnumArray<std::shared_ptr<ImageDecoder>, ImageFormat>      m_ImageDecoders;
    std::function<std::shared_ptr<asset::Material>(std::string_view)> m_MaterialGetter;
};

class MaterialParser {
public:
    virtual ~MaterialParser() = default;
    MaterialParser(std::shared_ptr<spdlog::logger> logger = nullptr) : m_Logger(std::move(logger)) {}

    virtual auto Parse(const std::filesystem::path& path) -> std::shared_ptr<Material>;
    virtual auto Parse(const core::Buffer& buffer) -> std::shared_ptr<Material> = 0;

protected:
    std::shared_ptr<spdlog::logger> m_Logger;
};

class MaterialJSONParser : public MaterialParser {
public:
    using MaterialParser::MaterialParser;

    using MaterialParser::Parse;
    auto Parse(const core::Buffer& buffer) -> std::shared_ptr<Material> final;
};

class AssetManager final : public core::RuntimeModule {
public:
    AssetManager(std::filesystem::path asset_base_path);
    ~AssetManager() final;

    static auto Get() -> AssetManager* { return static_cast<AssetManager*>(core::RuntimeModule::GetModule("AssetManager")); }

    Scene CreateEmptyScene(std::string_view name);

    std::shared_ptr<Scene>    ImportScene(const std::filesystem::path& path);
    std::shared_ptr<Texture>  ImportTexture(const std::filesystem::path& path);
    std::shared_ptr<Material> ImportMaterial(const std::filesystem::path& path);

    void AddScene(std::shared_ptr<Scene> scene);
    void AddCamera(std::shared_ptr<Camera> camera);
    void AddLight(std::shared_ptr<Light> light);
    void AddMesh(std::shared_ptr<Mesh> mesh);
    void AddSkeleton(std::shared_ptr<Skeleton> skeleton);
    void AddTexture(std::shared_ptr<Texture> texture);

    std::shared_ptr<Material> GetMaterial(std::string_view name);
    inline const auto&        GetAllMaterials() const noexcept { return m_Assets.materials; }

private:
    void InitBuiltinMaterial();

    std::filesystem::path m_BasePath;

    std::shared_ptr<MaterialParser>                              m_MaterialParser;
    utils::EnumArray<std::shared_ptr<ImageDecoder>, ImageFormat> m_ImageDecoders;
    utils::EnumArray<std::shared_ptr<ImageEncoder>, ImageFormat> m_ImageEncoders;
    utils::EnumArray<std::shared_ptr<SceneParser>, SceneFormat>  m_SceneParsers;

    struct Assets {
        template <typename T>
        using SharedPtrSet = std::pmr::set<std::shared_ptr<T>>;

        SharedPtrSet<Scene>    scenes;
        SharedPtrSet<Material> materials;
        SharedPtrSet<Camera>   cameras;
        SharedPtrSet<Light>    lights;
        SharedPtrSet<Mesh>     meshes;
        SharedPtrSet<Skeleton> skeletons;
        SharedPtrSet<Texture>  textures;
    } m_Assets;
};

}  // namespace hitagi::asset
