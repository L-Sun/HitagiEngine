module;

export module asset:cooked_format;
import std;
import math;

// HTGC (Hitagi Cooked) binary container layout.
//
// This partition is the single source of truth for the on-disk format: both the
// editor cook writer and the runtime loader import these PODs, so the two ends
// can never drift apart. Any layout change must bump kCookedVersion; loaders
// reject mismatched files and the asset is expected to be re-cooked (cooked
// data is a build artifact, not an archive format).
//
// File layout:
//   FileHeader | meta section (fixed-size records) | string table | blob (16B aligned)
//
// All offsets inside records are section-relative; the header stores absolute
// file offsets of the three sections. Byte order is little-endian only.
export namespace hitagi::asset {

inline constexpr std::array<char, 4> kCookedMagic{'H', 'T', 'G', 'C'};
inline constexpr std::uint32_t       kCookedVersion         = 1;
inline constexpr std::uint32_t       kCookedInvalidIndex    = 0xFFFF'FFFFu;
inline constexpr std::size_t         kCookedBlobAlignment   = 16;
inline constexpr std::size_t         kCookedRecordAlignment = 8;

enum struct CookedAssetType : std::uint32_t {
    Material = 1,
    Scene    = 2,
};

// References into the blob section (bytes).
struct BufferView {
    std::uint64_t offset = 0;
    std::uint64_t size   = 0;
};

// References into the meta section (element count of a fixed-size record array).
struct ArrayRef {
    std::uint64_t offset = 0;
    std::uint64_t count  = 0;
};

// References into the string table.
struct StringRef {
    std::uint32_t offset = 0;
    std::uint32_t length = 0;
};

struct CookedFileHeader {
    std::array<char, 4> magic{};
    std::uint32_t       version = 0;
    CookedAssetType     asset_type{};
    std::uint32_t       flags = 0;
    BufferView          meta{};
    BufferView          strings{};
    BufferView          blob{};
    std::uint64_t       root_offset = 0;  // root record offset inside the meta section
    std::uint64_t       reserved    = 0;
};

// --- material records ------------------------------------------------------

// MaterialParameterValue's largest alternative is mat4f (64 bytes); inlining a
// fixed payload keeps parameter records fixed-size for a few wasted bytes.
inline constexpr std::size_t kCookedParameterPayloadSize = 64;

enum CookedParameterFlags : std::uint32_t {
    kCookedParameterNone        = 0,
    kCookedParameterNullTexture = 1 << 0,
};

struct ParameterRecord {
    StringRef                                             name{};
    StringRef                                             type{};  // e.g. "Float", "Vec3f", "Texture2D"
    StringRef                                             texture_name{};
    StringRef                                             texture_path{};
    std::uint32_t                                         flags    = kCookedParameterNone;
    std::uint32_t                                         _padding = 0;
    std::array<std::byte, kCookedParameterPayloadSize>    payload{};
};

struct ShaderRecord {
    StringRef name{};
    StringRef type{};  // gfx::ShaderType name
    StringRef entry{};
    StringRef path{};
    StringRef source{};
};

// Enums are stored as strings (restored via magic_enum) so that reordering the
// C++ enums can never silently corrupt cooked files.
struct PipelineRecord {
    StringRef     name{};
    StringRef     primitive{};
    StringRef     cull_mode{};
    StringRef     depth_compare{};
    StringRef     render_format{};
    StringRef     depth_stencil_format{};
    std::uint32_t front_counter_clockwise = 0;
    std::uint32_t depth_test              = 0;
    std::uint32_t depth_write             = 0;
    std::uint32_t _padding                = 0;
};

struct PassRecord {
    StringRef      contract{};
    std::uint32_t  has_pipeline = 0;
    std::uint32_t  _padding     = 0;
    ArrayRef       shaders{};   // -> ShaderRecord[]
    ArrayRef       bindings{};  // -> StringRef[]
    PipelineRecord pipeline{};
};

struct MaterialRecord {
    StringRef name{};
    ArrayRef  parameters{};  // -> ParameterRecord[]
    ArrayRef  passes{};      // -> PassRecord[]
};

// --- mesh records -----------------------------------------------------------

struct VertexAttributeRecord {
    StringRef  attribute{};  // asset::VertexAttribute name
    BufferView data{};       // vertex_count * sizeof(VertexDataType<attribute>)
};

struct SubMeshRecord {
    std::uint64_t index_count    = 0;
    std::uint64_t index_offset   = 0;
    std::uint64_t vertex_offset  = 0;
    std::uint32_t material_index = kCookedInvalidIndex;
    std::uint32_t _padding       = 0;
};

struct MeshRecord {
    StringRef            name{};
    std::uint64_t        vertex_count = 0;
    ArrayRef             attributes{};  // -> VertexAttributeRecord[]
    StringRef            index_type{};  // asset::IndexType name
    std::uint64_t        index_count = 0;
    BufferView           indices{};
    ArrayRef      sub_meshes{};  // -> SubMeshRecord[]
    math::vec3f   aabb_min{};    // cooked at build time; the loader skips ComputeAABB
    math::vec3f   aabb_max{};
};

// --- scene records ----------------------------------------------------------

struct MeshInstanceRecord {
    StringRef     name{};
    std::uint32_t mesh_index = kCookedInvalidIndex;
    std::uint32_t _padding   = 0;
    math::mat4f   transform{};
};

struct CameraRecord {
    StringRef   name{};
    math::mat4f transform{};
    float       aspect         = 16.0f / 9.0f;
    float       near_clip      = 1.0f;
    float       far_clip       = 1000.0f;
    float       horizontal_fov = 0.0f;
    math::vec3f eye{};
    math::vec3f look_dir{};
    math::vec3f up{};
};

struct LightRecord {
    StringRef   name{};
    StringRef   type{};  // asset::Light::Type name
    math::mat4f transform{};
    float       intensity = 1.0f;
    math::Color color{};
    math::vec3f position{};
    math::vec3f direction{};
    math::vec3f up{};
    float       inner_cone_angle = 0.0f;
    float       outer_cone_angle = 0.0f;
};

struct SceneRecord {
    StringRef name{};
    ArrayRef  materials{};  // -> MaterialRecord[]
    ArrayRef  meshes{};     // -> MeshRecord[]
    ArrayRef  instances{};  // -> MeshInstanceRecord[]
    ArrayRef  cameras{};    // -> CameraRecord[]
    ArrayRef  lights{};     // -> LightRecord[]
};

inline auto IsCookedBinary(std::span<const std::byte> data) noexcept -> bool {
    return data.size() >= sizeof(CookedFileHeader) &&
           std::ranges::equal(data.first(kCookedMagic.size()), std::as_bytes(std::span{kCookedMagic}));
}

template <typename T>
concept CookedRecord = std::is_trivially_copyable_v<T>;

}  // namespace hitagi::asset

// Layout locks: internal sanity checks, not part of the exported interface.
namespace hitagi::asset {

static_assert(std::endian::native == std::endian::little, "HTGC is a little-endian format");

// math types are embedded in records; lock their byte layout too.
static_assert(sizeof(math::vec3f) == 12 && alignof(math::vec3f) == 4);
static_assert(sizeof(math::Color) == 16 && alignof(math::Color) == 4);
static_assert(sizeof(math::mat4f) == 64 && alignof(math::mat4f) == 4);

static_assert(sizeof(BufferView) == 16);
static_assert(sizeof(ArrayRef) == 16);
static_assert(sizeof(StringRef) == 8);
static_assert(sizeof(CookedFileHeader) == 80);
static_assert(sizeof(ParameterRecord) == 104);
static_assert(sizeof(ShaderRecord) == 40);
static_assert(sizeof(PipelineRecord) == 64);
static_assert(sizeof(PassRecord) == 112);
static_assert(sizeof(MaterialRecord) == 40);
static_assert(sizeof(VertexAttributeRecord) == 24);
static_assert(sizeof(SubMeshRecord) == 32);
static_assert(sizeof(MeshRecord) == 104);
static_assert(sizeof(MeshInstanceRecord) == 80);
static_assert(sizeof(CameraRecord) == 124);
static_assert(sizeof(LightRecord) == 144);
static_assert(sizeof(SceneRecord) == 88);

static_assert(
    CookedRecord<CookedFileHeader> && CookedRecord<ParameterRecord> && CookedRecord<ShaderRecord> &&
    CookedRecord<PipelineRecord> && CookedRecord<PassRecord> && CookedRecord<MaterialRecord> &&
    CookedRecord<VertexAttributeRecord> && CookedRecord<SubMeshRecord> && CookedRecord<MeshRecord> &&
    CookedRecord<MeshInstanceRecord> && CookedRecord<CameraRecord> && CookedRecord<LightRecord> &&
    CookedRecord<SceneRecord>);

}  // namespace hitagi::asset
