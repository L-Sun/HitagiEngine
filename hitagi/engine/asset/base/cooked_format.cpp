export module asset.base:cooked_format;
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
inline constexpr std::uint32_t       kCookedVersion         = 8;
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
    StringRef                                          name{};
    StringRef                                          type{};  // e.g. "Float", "Vec3f", "Texture2D"
    StringRef                                          texture_name{};
    StringRef                                          texture_path{};
    std::uint32_t                                      flags    = kCookedParameterNone;
    std::uint32_t                                      _padding = 0;
    std::array<std::byte, kCookedParameterPayloadSize> payload{};
};

struct ShaderModuleRecord {
    StringRef name{};
    StringRef path{};
    StringRef source{};
};

struct ShaderRecord {
    StringRef name{};
    StringRef type{};  // gfx::ShaderType name
    StringRef entry{};
    ArrayRef  modules{};               // ShaderModuleRecord[], in dependency order
    ArrayRef  specialization_types{};  // StringRef[]
};

// Enums are stored as strings (restored via magic_enum) so that reordering the
// C++ enums can never silently corrupt cooked files.
struct VertexInputRecord {
    StringRef     semantic{};
    StringRef     format{};
    std::uint32_t binding      = 0;
    std::uint32_t location     = 0;
    std::uint64_t offset       = 0;
    std::uint64_t stride       = 0;
    std::uint32_t per_instance = 0;
    std::uint32_t _padding     = 0;
};

struct PipelineRecord {
    StringRef     name{};
    StringRef     primitive{};
    StringRef     cull_mode{};
    StringRef     depth_compare{};
    StringRef     render_format{};
    StringRef     depth_stencil_format{};
    ArrayRef      vertex_input_layout{};
    std::uint32_t front_counter_clockwise = 0;
    std::uint32_t depth_test              = 0;
    std::uint32_t depth_write             = 0;
    std::uint32_t _padding                = 0;
    std::uint32_t restart_enable{};
    StringRef     fill_mode{};
    std::uint32_t depth_clamp_enable{};
    std::uint32_t depth_bias_enable{};
    float         depth_bias{};
    float         depth_bias_clamp{};
    float         depth_bias_slope_factor{};
    std::uint32_t stencil_test_enable{};
    StringRef     front_fail_op{};
    StringRef     front_pass_op{};
    StringRef     front_depth_fail_op{};
    StringRef     front_compare_op{};
    std::uint32_t front_compare_mask{};
    std::uint32_t front_write_mask{};
    std::uint32_t front_reference{};
    StringRef     back_fail_op{};
    StringRef     back_pass_op{};
    StringRef     back_depth_fail_op{};
    StringRef     back_compare_op{};
    std::uint32_t back_compare_mask{};
    std::uint32_t back_write_mask{};
    std::uint32_t back_reference{};
    std::uint32_t depth_bounds_test_enable{};
    math::vec2f   depth_bounds{};
    std::uint32_t blend_enable{};
    StringRef     src_color_blend_factor{};
    StringRef     dst_color_blend_factor{};
    StringRef     color_blend_op{};
    StringRef     src_alpha_blend_factor{};
    StringRef     dst_alpha_blend_factor{};
    StringRef     alpha_blend_op{};
    math::vec4f   blend_constants{};
    std::uint32_t color_write_mask{};
    std::uint32_t logic_operation_enable{};
    StringRef     logic_op{};
};

struct PassRecord {
    StringRef      contract{};
    std::uint32_t  has_pipeline = 0;
    std::uint32_t  _padding     = 0;
    ArrayRef       shaders{};   // -> ShaderRecord[]
    ArrayRef       bindings{};  // -> ParameterRecord[] (mandatory typed contract defaults)
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
    StringRef     name{};
    std::uint64_t vertex_count = 0;
    ArrayRef      attributes{};  // -> VertexAttributeRecord[]
    StringRef     index_type{};  // asset::IndexType name
    std::uint64_t index_count = 0;
    BufferView    indices{};
    ArrayRef      sub_meshes{};  // -> SubMeshRecord[]
};

// --- scene records ----------------------------------------------------------

// Float32 TRS (40 bytes), preserving signed scales and quaternion components.
struct NodeRecord {
    StringRef     name{};
    std::uint32_t parent   = kCookedInvalidIndex;
    std::uint32_t reserved = 0;
    math::vec3f   translation{};
    math::quatf   rotation = math::quatf::identity();
    math::vec3f   scale    = math::vec3f(1.0f);
};
struct MeshInstanceRecord {
    std::uint32_t node       = kCookedInvalidIndex;
    std::uint32_t mesh_index = kCookedInvalidIndex;
    ArrayRef      material_overrides{};    // uint32 material indices; invalid means mesh default
};
struct CameraRecord {
    std::uint32_t node           = kCookedInvalidIndex;
    float         aspect         = 16.0f / 9.0f;
    float         near_clip      = 1.0f;
    float         far_clip       = 1000.0f;
    float         horizontal_fov = 0.0f;
};
struct LightRecord {
    StringRef     type{};
    std::uint32_t node      = kCookedInvalidIndex;
    float         intensity = 1.0f;
    math::Color   color{};
    float         inner_cone_angle = 0.0f, outer_cone_angle = 0.0f;
};
// Row-major first three rows; the fourth is implicitly (0,0,0,1).
// Only affine transforms use this encoding. Material Mat4f remains unrestricted.
struct AffineMatrixRecord {
    std::array<float, 12> values{};
};
struct SkinRecord {
    StringRef  name{};
    BufferView inverse_bind_matrices{};  // tightly packed AffineMatrixRecord[]
};
// Explicit wire values, independent of runtime enum order.
enum class CurveProperty : std::uint32_t { Translation = 0,
                                           Rotation    = 1,
                                           Scale       = 2 };
enum class CurveInterpolation : std::uint32_t { Step   = 0,
                                                Linear = 1 };
enum class CurveTimeEncoding : std::uint32_t { Uniform  = 0,
                                               Explicit = 1 };
struct CurveRecord {
    CurveProperty      property{};
    CurveInterpolation interpolation{};
    std::uint32_t      key_count = 0;
    CurveTimeEncoding  time_encoding{};
    double             start = 0, step = 0;
    BufferView         times{};   // float64[], absent for uniform/single-key curves
    BufferView         values{};  // packed float32 xyz or xyzw, with no per-key padding
};
struct AnimationTrackRecord {
    std::uint32_t target   = 0;
    std::uint32_t reserved = 0;
    ArrayRef      curves{};  // sparse CurveRecord[], absent channels retain node defaults
};
struct AnimationClipRecord {
    StringRef name{};
    double    duration = 0;
    ArrayRef  tracks{};
};
struct AnimationPlayerRecord {
    std::uint32_t node = kCookedInvalidIndex;
    std::uint32_t clip = kCookedInvalidIndex;
    double        time = 0, speed = 1;
    std::uint32_t looping = 1, playing = 1;
};
struct AnimationTargetRecord {
    std::uint32_t node        = kCookedInvalidIndex;
    std::uint32_t player_node = kCookedInvalidIndex;
    std::uint32_t target      = 0;
};
struct SkinnedMeshRecord {
    std::uint32_t node = kCookedInvalidIndex;
    std::uint32_t skin = kCookedInvalidIndex;
    ArrayRef      joints{};  // uint32 node indices in skin palette order, NOT track order
};
struct SceneRecord {
    StringRef     name{};
    ArrayRef      materials{}, meshes{}, nodes{}, instances{}, cameras{}, lights{};
    ArrayRef      skins{}, clips{}, players{}, targets{}, skinned_meshes{};
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
static_assert(sizeof(math::quatf) == 16 && alignof(math::quatf) == 4);
static_assert(sizeof(math::Color) == 16 && alignof(math::Color) == 4);
static_assert(sizeof(math::mat4f) == 64 && alignof(math::mat4f) == 4);

static_assert(sizeof(BufferView) == 16);
static_assert(sizeof(ArrayRef) == 16);
static_assert(sizeof(StringRef) == 8);
static_assert(sizeof(CookedFileHeader) == 80);
static_assert(sizeof(ParameterRecord) == 104);
static_assert(sizeof(ShaderModuleRecord) == 24);
static_assert(sizeof(ShaderRecord) == 56);
static_assert(sizeof(VertexInputRecord) == 48);
static_assert(sizeof(PipelineRecord) == 304);
static_assert(sizeof(PassRecord) == 352);
static_assert(sizeof(MaterialRecord) == 40);
static_assert(sizeof(VertexAttributeRecord) == 24);
static_assert(sizeof(SubMeshRecord) == 32);
static_assert(sizeof(MeshRecord) == 80);
static_assert(sizeof(MeshInstanceRecord) == 24);
static_assert(sizeof(CameraRecord) == 20);
static_assert(sizeof(LightRecord) == 40);
static_assert(sizeof(NodeRecord) == 56);
static_assert(sizeof(AffineMatrixRecord) == 48);
static_assert(sizeof(SkinRecord) == 24);
static_assert(sizeof(CurveRecord) == 64);
static_assert(sizeof(AnimationTrackRecord) == 24);
static_assert(sizeof(AnimationClipRecord) == 32);
static_assert(sizeof(AnimationPlayerRecord) == 32);
static_assert(sizeof(AnimationTargetRecord) == 12);
static_assert(sizeof(SkinnedMeshRecord) == 24);
static_assert(sizeof(SceneRecord) == 184);

static_assert(
    CookedRecord<CookedFileHeader> && CookedRecord<ParameterRecord> && CookedRecord<ShaderRecord> &&
    CookedRecord<PipelineRecord> && CookedRecord<PassRecord> && CookedRecord<MaterialRecord> &&
    CookedRecord<VertexAttributeRecord> && CookedRecord<SubMeshRecord> && CookedRecord<MeshRecord> &&
    CookedRecord<MeshInstanceRecord> && CookedRecord<CameraRecord> && CookedRecord<LightRecord> &&
    CookedRecord<SceneRecord> && CookedRecord<NodeRecord> && CookedRecord<AffineMatrixRecord> &&
    CookedRecord<SkinRecord> && CookedRecord<CurveRecord> && CookedRecord<AnimationTrackRecord> &&
    CookedRecord<AnimationClipRecord> && CookedRecord<AnimationPlayerRecord> &&
    CookedRecord<AnimationTargetRecord> && CookedRecord<SkinnedMeshRecord>);

}  // namespace hitagi::asset
