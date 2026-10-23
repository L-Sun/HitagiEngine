# HTGC binary format

HTGC (Hitagi Cooked) is the engine's cooked-asset container. It avoids text parsing at runtime. Vertex/index payloads are copied in their native layouts. Structural metadata uses fixed-size records; compact affine matrices and animation channels are decoded into runtime objects. References and payload lengths are validated before use.

Cooked data is a **build artifact, not an archive format**. On a version mismatch the loader errors and asks for a re-cook. There is no backward compatibility. In exchange, the format can evolve with the implementation.

## Why this is not JSON

Materials and scenes used to be JSON. The bottleneck was not the structural fields. It was the vertex and index arrays: JSON parses every float as text and `push_back`s every element. The same mesh in binary form collapses that work into one `memcpy`. That is the only reason the format was redone.

## File layout

```text
┌────────────────────────────────────────────┐
│ CookedFileHeader          80 bytes         │  file offset 0
├────────────────────────────────────────────┤
│ meta section                               │  array of fixed-size records, 8-byte aligned
│   MaterialRecord / MeshRecord / PassRecord │
│   ParameterRecord / SubMeshRecord / ...    │
├────────────────────────────────────────────┤
│ string table                               │  raw bytes, not NUL-terminated
├────────────────────────────────────────────┤
│ blob section                               │  starts 16-byte aligned
│   vertex attribute data / index data       │
└────────────────────────────────────────────┘
```

Offsets come in two layers:

* The three `BufferView`s in the header store **absolute file offsets**.
* `ArrayRef`, `BufferView`, and `StringRef` inside records store **offsets relative to their own section**.

Endianness: **little-endian only**, locked by a `static_assert` in `cooked_format.cpp`.

## Header

```cpp
struct CookedFileHeader {           // 80 bytes
    std::array<char, 4> magic{};    // 'H','T','G','C'
    std::uint32_t       version;    // kCookedVersion, currently 8
    CookedAssetType     asset_type; // Material = 1, Scene = 2
    std::uint32_t       flags;
    BufferView          meta;       // absolute file offset + length
    BufferView          strings;
    BufferView          blob;
    std::uint64_t       root_offset;// offset of the root record within the meta section
    std::uint64_t       reserved;
};
```

`root_offset` points at a `MaterialRecord` (material files) or a `SceneRecord` (scene files). It is the entry point of the whole record graph.

## Reference types

```cpp
struct BufferView { std::uint64_t offset, size;  };  // 16B, points into the blob
struct ArrayRef   { std::uint64_t offset, count; };  // 16B, points at a fixed-size record array in meta
struct StringRef  { std::uint32_t offset, length;};  //  8B, points into the string table
```

`StringRef` with `length == 0` is the empty string. The string table deduplicates by content, so a repeated name is stored once.

## Records

Every record is a trivially copyable POD. The `CookedRecord` concept enforces that, and each record's `sizeof` is locked by `static_assert`. Record size changes are checked at build time; any layout or interpretation change also requires an explicit version bump.

### Material side

| Record | Size | Key fields |
| --- | --- | --- |
| `ParameterRecord` | 104 | `name`, `type`, `texture_name`/`texture_path`, `flags`, `payload[64]` |
| `ShaderModuleRecord` | 24 | `name`, `path`, `source` |
| `ShaderRecord` | 56 | `name`, `type`, `entry`, `modules`, `specialization_types` |
| `VertexInputRecord` | 48 | semantic, format, binding/location, offset/stride, per-instance flag |
| `PipelineRecord` | 304 | vertex layout array; complete assembly, rasterization, depth/stencil, blend and attachment formats |
| `PassRecord` | 352 | `contract`, `has_pipeline`, `shaders`, `bindings`, embedded `PipelineRecord` |
| `MaterialRecord` | 40 | `name`, `parameters`, `passes` |

`ParameterRecord.payload` is a 64-byte inline fixed payload. The largest alternative in `MaterialParameterValue` is `mat4f` (64 bytes). Inlining it into a fixed-size record wastes a few bytes and keeps parameter records fixed-size, so they can be `AppendArray`'d as a block.

Texture parameters do not store pixels. They store `texture_path` (relative to the asset root) and `texture_name`. An empty texture is expressed by the `kCookedParameterNullTexture` flag, which is distinct from an empty path.

In v2, `PassRecord.bindings` references an ordered `ParameterRecord[]`, not `StringRef[]`. Each record stores the binding name, expected type (the typed default's variant alternative), and mandatory default value. Default texture references are resolved through the same asset registry as ordinary parameters. v1 artifacts must be re-cooked.

Cook validates final material parameters against these bindings before writing either material or scene records. Missing parameters and type mismatches are build errors, even though runtime can recover using the stored defaults.

### Mesh and scene side

| Record | Size | Key fields |
| --- | --- | --- |
| `VertexAttributeRecord` | 24 | `attribute` (enum name), `data` (blob view) |
| `SubMeshRecord` | 32 | `index_count`, `index_offset`, `vertex_offset`, `material_index` |
| `MeshRecord` | 80 | `vertex_count`, `attributes`, `index_type`, `index_count`, `indices`, `sub_meshes` |
| `NodeRecord` | 56 | `name`, `parent`, local translation/quaternion/scale |
| `MeshInstanceRecord` | 24 | `node`, `mesh_index`, material override indices |
| `CameraRecord` | 20 | `node`, projection parameters |
| `LightRecord` | 40 | `node`, `type`, intensity/color and cone angles |
| `SkinRecord` | 24 | `name`, inverse-bind matrix blob |
| `CurveRecord` | 64 | channel/interpolation, key count, time encoding, time/value blobs |
| `AnimationTrackRecord` | 24 | logical target slot, sparse curve records |
| `AnimationClipRecord` | 32 | `name`, duration, tracks |
| `AnimationPlayerRecord` | 32 | node/clip indices, time/speed, loop/play flags |
| `AnimationTargetRecord` | 12 | target node, player node, logical target slot |
| `SkinnedMeshRecord` | 24 | node/skin indices, ordered joint-node array |
| `SceneRecord` | 184 | name and eleven array references; no designated root or current camera |

Nodes preserve the hierarchy, including empty joints and non-renderable groups.
Version 8 stores a forest of entities: zero or more nodes may have no parent. Empty scenes are valid. Every node is allocated independently on load; parent and animation references are then remapped. There is no implicit scene root or current-camera field. Camera selection belongs to the host. Older cooked files must be re-cooked.
Local TRS uses 40 bytes (float32 xyz, quaternion xyzw, scale xyz), preserving
negative scales without matrix decomposition. Camera/light/mesh records attach
components to these nodes, without duplicating the transform or entity name.
Scene nodes require Transform and RelationShip; this is not a generic snapshot
of arbitrary ECS archetypes or gameplay components.

All node and asset references are file-local indices, never ECS IDs or pointers.
Mesh, Skin and Clip assets are deduplicated by source object identity within the
scene. Sharing across separately loaded scene files is not implied. Component
records are independent: multiple meshes may bind the same joints; different
players may bind the same Clip with independent playback configurations.

Version 8 removes instance bounds. The Viewer derives its framing bounds from
current geometry; neither asset components nor cooked records store AABBs.
Older cooked files must be re-cooked.

Version 7 adds instance material overrides and cooked vertex layouts, preserves all pipeline state, and removes duplicate camera/light identity and pose. Older files must be re-cooked. Each override is a uint32 material index; the invalid index means use the mesh default. Missing trailing overrides also use defaults. Camera/light pose comes exclusively from the node TRS.

`SubMeshRecord.material_index` is an index into the `SceneRecord.materials` array. `kCookedInvalidIndex` (`0xFFFFFFFF`) means there is no material.

## Animation and skin encoding

- **Inverse bind matrices:** row-major 3x4 float32 records, 48 bytes each. Only
  affine matrices are accepted; the omitted fourth row is `(0,0,0,1)`. Loading
  reconstructs a 4x4 matrix. This saves 25% of the matrix payload and preserves
  translations, signed/nonuniform scales and shear. Arbitrary material Mat4f
  parameters retain all 16 elements, since they may be projective.
- **Channels:** only nonempty translation, rotation and scale channels are
  written. Absent channels retain the node's serialized local values. Constant
  channels with identical values are reduced to one key. Values remain float32
  xyz/xyzw, without per-key struct padding or lossy quantization.
- **Times:** float64 seconds, separate from values. Uniform keys store only
  `start`, `step`, and count, and reconstruct with `fma(index, step, start)`.
  The writer uses this encoding only if every source timestamp matches exactly.
  Single keys need no time array. Irregular time arrays are stored verbatim and
  shared by content across channels/clips within the scene.
- **Bindings:** animation target IDs are Clip-local slots. Skin palette indices
  instead index both the inverse-bind array and the instance's ordered joint
  node array. These two index spaces are independent. BlendIndex/BlendWeight
  continue through the existing Mesh vertex-attribute encoding. Zero-weight
  slots ignore their indices, matching runtime skinning behavior.

World transforms, skin palettes, deformed vertices, GPU
handles are rebuilt, not serialized. Loading allocates all node identities,
then restores parents, asset components, players and target/skin bindings.
One zero-delta update evaluates the saved initial time and creates skin outputs.

General runtime enums (shader type, format, vertex attribute, etc.) remain
strings. Animation-specific wire enums have explicit frozen uint32 values:
Translation/Rotation/Scale = 0/1/2, Step/Linear = 0/1, Uniform/Explicit = 0/1.
They are converted explicitly to runtime enums; unknown values are errors.

## Writer

Writing lives in `hitagi/editor/cook.cppm` (the `editor:cook` partition). The entry points are:

```cpp
auto CookEditorMaterial(const EditorMaterial&, EditorCookOptions = {}) -> core::Buffer;
auto CookEditorScene(asset::Scene&, EditorCookOptions = {})            -> core::Buffer;
```

`CookEditorScene` does not run systems or mutate playback/transforms. Its input
must contain the intended load-time local TRS and player defaults. The default
`SceneCookState::Initial` rejects nonzero player times, preventing accidental
export of an advanced preview. `SceneCookState::Current` explicitly captures
the supplied preview state instead. Neither option can recover authored rest
values that were already overwritten by playback; cook the authoring scene
before preview or explicitly choose the current state. Missing animation
channels preserve those supplied local defaults.

Entity enumeration reads storage rather than cached child lists, so newly
created nodes and direct parent edits need no preparatory update. External
entity references, disconnected/cyclic hierarchies, missing skin attributes,
invalid active influence indices and incompatible skin palettes are errors.

Two builders:

* `StringTableBuilder`: `Add(string_view) -> StringRef`, deduplicated by content.
* `SectionBuilder`: `Append` / `AppendArray` align to 8 bytes first; `AppendBytes` uses the alignment the caller supplies (16 for mesh/affine payloads, 8 for times, 4 for curve values).

Assembly order:

```text
meta_offset    = sizeof(CookedFileHeader)                       // 80
strings_offset = meta_offset + meta.size
blob_offset    = align_up(strings_offset + strings.size, 16)
```

Record arrays are written **children first, then the parent**: `BuildMaterialRecord` `AppendArray`s the parameters and passes into meta to obtain `ArrayRef`s, and only then `Append`s the `MaterialRecord` itself. The root record's offset is written into the header as `root_offset`.

`CookedPath` turns every path into a generic path relative to `asset_root_path`. A path that falls outside the root is left unchanged.

## Reader

Reading lives in `hitagi/engine/asset/cooked_binary.cpp` (the implementation partition `asset:cooked_binary`, not exported outside the module):

```cpp
auto ParseCookedMaterial(std::span<const std::byte>, root, CookedTextureResolver) -> std::shared_ptr<Material>;
auto ParseCookedScene(std::span<const std::byte>, root, CookedTextureResolver)    -> std::shared_ptr<Scene>;
```

`CookedReader` validates the whole header in its constructor:

```text
1. length >= sizeof(CookedFileHeader)
2. magic == 'HTGC'
3. version == kCookedVersion            otherwise error "re-cook the asset"
4. asset_type == the expected type
5. the absolute offset and length of each of the three sections fall inside the file
```

Every later `Read<T>`, `GetString`, and `GetBlob` checks the range and throws on an out-of-bounds access. Records are `memcpy`'d out rather than aliased in place. Records are small, so the copy is negligible, and the reader never has to worry about the alignment of a record inside the file.

`ReadArray` validates the complete range with division before constructing its
lazy view, preventing count/stride overflow. Curve lengths and encodings,
finite values, key ordering, quaternion validity, hierarchy cycles, component
duplicates and entity/asset indices are checked. Invalid loads throw before the
Scene is published to AssetManager. Partial scene objects are destroyed.

### Injected texture resolution

The parser does not `make_shared<Texture>` itself. It takes a callback:

```cpp
using CookedTextureResolver =
    std::function<std::shared_ptr<Texture>(const std::filesystem::path&, std::string_view)>;
```

`AssetManager` passes `AcquireTexture`, so one path has one `Texture` instance for the whole scene. The texture resolver is an explicit required dependency of the internal parser.

`ResolveCookedPath` restores paths: an absolute path is returned as-is; a relative path is used as-is if it exists in the current working directory, otherwise it is joined onto the asset root.

## Evolution rules

1. Any change to a record layout must `+1` `kCookedVersion`.
2. Update the record's `static_assert(sizeof(...))` at the same time. It is the gate that catches "added a field, forgot the version".
3. Prefer adding fields at the end of a record, with explicit padding, so alignment stays predictable.
4. Do not write runtime enum ordinals. Explicit wire enums must keep frozen values and validate unknown values.
5. Cooked files do not belong in version control. After a format change, cook again.

## Verification

`CookedAnimationTest` checks shared assets with independent players, reversed
skin/track order, signed scales, affine matrix reconstruction, compact curves,
shared irregular times, deterministic output, explicit preview capture,
non-mesh animated nodes, forward references, unweighted vertices, and malformed
files. `UsdSkelImportTest` compares imported and HTGC-loaded skinning at several
times and writes `temp/htgc/skinned.hcscene` for file-path loading verification.
Existing material and static-scene round trips remain part of the same suite.

```powershell
xmake build asset_pipeline_tests
xmake r asset_pipeline_tests
```
