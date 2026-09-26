# HTGC binary format

HTGC (Hitagi Cooked) is the engine's cooked-asset container. It serves one narrow goal: **runtime loading does no parsing**. Large payloads such as vertices, indices, and pixels are already in their final memory layout in the file, so loading is one `memcpy`. Structural metadata is fixed-size POD records, also `memcpy`'d out directly.

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
    std::uint32_t       version;    // kCookedVersion, currently 1
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

Every record is a trivially copyable POD. The `CookedRecord` concept enforces that, and each record's `sizeof` is locked by `static_assert`. Adding a field and forgetting to bump the version fails the build.

### Material side

| Record | Size | Key fields |
| --- | --- | --- |
| `ParameterRecord` | 104 | `name`, `type`, `texture_name`/`texture_path`, `flags`, `payload[64]` |
| `ShaderRecord` | 40 | `name`, `type`, `entry`, `path`, `source` |
| `PipelineRecord` | 64 | `primitive`, `cull_mode`, `depth_compare`, `render_format`, `depth_stencil_format`, plus three boolean bits |
| `PassRecord` | 112 | `contract`, `has_pipeline`, `shaders`, `bindings`, embedded `PipelineRecord` |
| `MaterialRecord` | 40 | `name`, `parameters`, `passes` |

`ParameterRecord.payload` is a 64-byte inline fixed payload. The largest alternative in `MaterialParameterValue` is `mat4f` (64 bytes). Inlining it into a fixed-size record wastes a few bytes and keeps parameter records fixed-size, so they can be `AppendArray`'d as a block.

Texture parameters do not store pixels. They store `texture_path` (relative to the asset root) and `texture_name`. An empty texture is expressed by the `kCookedParameterNullTexture` flag, which is distinct from an empty path.

### Mesh and scene side

| Record | Size | Key fields |
| --- | --- | --- |
| `VertexAttributeRecord` | 24 | `attribute` (enum name), `data` (blob view) |
| `SubMeshRecord` | 32 | `index_count`, `index_offset`, `vertex_offset`, `material_index` |
| `MeshRecord` | 104 | `vertex_count`, `attributes`, `index_type`, `index_count`, `indices`, `sub_meshes`, `aabb_min/max` |
| `MeshInstanceRecord` | 80 | `name`, `mesh_index`, `transform` |
| `CameraRecord` | 124 | projection parameters + `eye`/`look_dir`/`up` + `transform` |
| `LightRecord` | 144 | `type`, `intensity`, `color`, `position`/`direction`/`up`, cone angles |
| `SceneRecord` | 88 | `materials`, `meshes`, `instances`, `cameras`, `lights` |

`math::vec3f`, `math::Color`, and `math::mat4f` are embedded in the records directly, not converted to `std::array<float, N>`. Their `sizeof` and `alignof` are locked by `static_assert` as well, so a value read out of the file is usable immediately, with no extra `bit_cast`.

`MeshRecord` stores the AABB, so loading skips `ComputeAABB()`. That value can be computed at cook time.

`SubMeshRecord.material_index` is an index into the `SceneRecord.materials` array. `kCookedInvalidIndex` (`0xFFFFFFFF`) means there is no material.

### Enums are stored as strings

`gfx::ShaderType`, `gfx::CullMode`, `gfx::Format`, `asset::VertexAttribute`, `asset::IndexType`, `Light::Type`, and the rest are strings in the file. Readers restore them with `magic_enum::enum_cast`.

The reason is the failure mode. If the file stored integers, reordering an enum on the C++ side would make an old file **silently parse as the wrong value**. Stored as strings, the same change makes `enum_cast` fail and fall back to an explicit default or an error. The cost is a few dozen bytes of string table and one lookup, paid at load time, and it is negligible.

## Writer

Writing lives in `hitagi/editor/cook.cppm` (the `editor:cook` partition). The entry points are:

```cpp
auto CookEditorMaterial(const EditorMaterial&, EditorCookOptions = {}) -> core::Buffer;
auto CookEditorScene(asset::Scene&, EditorCookOptions = {})            -> core::Buffer;
```

Two builders:

* `StringTableBuilder`: `Add(string_view) -> StringRef`, deduplicated by content.
* `SectionBuilder`: `Append` / `AppendArray` align to 8 bytes first; `AppendBytes` uses the alignment the caller supplies (16 for the blob).

Assembly order:

```text
meta_offset    = sizeof(CookedFileHeader)                       // 80
strings_offset = meta_offset + meta.size
blob_offset    = align_up(strings_offset + strings.size, 16)
```

Record arrays are written **children first, then the parent**: `BuildMaterialRecord` `AppendArray`s the parameters and passes into meta to obtain `ArrayRef`s, and only then `Append`s the `MaterialRecord` itself. The root record's offset is written into the header as `root_offset`.

`CookedPath` turns every path into a generic path relative to `asset_root_path`. A path that falls outside the root is left unchanged.

## Reader

Reading lives in `hitagi/asset/cooked_binary.cpp` (the implementation partition `asset:cooked_binary`, not exported outside the module):

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

Arrays are walked with a lazy view and no extra allocation:

```cpp
template <CookedRecord T>
auto ReadArray(ArrayRef ref) const {
    return std::views::iota(std::uint64_t{0}, ref.count) |
           std::views::transform([this, ref](std::uint64_t i) {
               return Read<T>(ref.offset + i * sizeof(T));
           });
}
```

### Injected texture resolution

The parser does not `make_shared<Texture>` itself. It takes a callback:

```cpp
using CookedTextureResolver =
    std::function<std::shared_ptr<Texture>(const std::filesystem::path&, std::string_view)>;
```

`AssetManager` passes `AcquireTexture`, so one path has one `Texture` instance for the whole scene. When the callback is empty (parsing without a manager, for example in a unit test), every reference creates a new lazy texture.

`ResolveCookedPath` restores paths: an absolute path is returned as-is; a relative path is used as-is if it exists in the current working directory, otherwise it is joined onto the asset root.

## Evolution rules

1. Any change to a record layout must `+1` `kCookedVersion`.
2. Update the record's `static_assert(sizeof(...))` at the same time. It is the gate that catches "added a field, forgot the version".
3. Prefer adding fields at the end of a record, with explicit padding, so alignment stays predictable.
4. Do not switch enums to integer storage to save bytes (see the failure-mode argument above).
5. Cooked files do not belong in version control. After a format change, cook again.
