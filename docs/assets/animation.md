# Skeletal animation through ECS

The animation foundation lives in `asset.base`, re-exported by `asset`. Optional
techniques live in `asset.plugins`; see [Animation Rig](rig.md) for
Pose constraints, layout binding and plugin composition.

The runtime uses joint entities and GPU compute linear blend skinning. `AnimationClip` and `Skin` are shared resources; neither owns a mesh
instance, playback state, or current pose. Joint entities use the existing
`Transform` and `RelationShip` components. A separate Skeleton resource is not
required by this version.

## Ownership and composition

| Data | Owner | Sharing |
| --- | --- | --- |
| Vertex positions, joint indices/weights, UVs, indices, materials | `Mesh` | Shared by instances |
| TRS curves and duration | `AnimationClip` | Shared, immutable after construction |
| Inverse bind matrices | `Skin` | Shared, immutable after construction |
| Clip, time, speed, looping, playing | `AnimationPlayer` component | Per player entity |
| Player reference and logical target ID | `AnimationTarget` component | Per animated entity |
| Current local/world pose | Joint entities' `Transform` components | Multiple skinned instances can reference the same joints |
| Skin reference, joint entity list and immutable matrix/bounds snapshot | `SkinnedMesh` component | Per mesh instance |

The two bindings are independent:

- `AnimationTarget::target` resolves a track in the referenced player's clip.
  These are logical IDs; they do not index a mesh or require a mesh to exist.
- `BlendIndex` addresses `SkinnedMesh::joints` and `Skin`'s inverse bind matrices
  in the same order. The skin slot order need not match animation target IDs.

Two characters can share Mesh, Skin and Clip resources while using different
players and joint entities. Body and clothing entities can share the same joint
entities, provided each mesh has a compatible Skin. Ordinary child entities can
serve as attachments. A target has one player; procedural control can remove
`AnimationTarget`, or explicitly run after animation sampling.

All referenced entities must belong to the same live Scene/World. Entity handles
do not keep the World alive. A destroyed player is ignored; a destroyed or
missing joint rejects the skin update, clears its output, and reports an error.
Fix the binding or remove the dependent skinned instance before rendering again.

## Update and rendering

`Scene` registers the animation and skinning systems alongside its hierarchy
systems. The host supplies elapsed seconds through the owned World:

```cpp
scene.GetWorld().Update(job_system, delta_seconds); // waits and propagates errors
```

The default duration is zero: `GetWorld().Update(job_system)` still propagates transforms
and evaluates the current pose, without advancing animation time.
Durations must be finite and nonnegative; negative player speed plays backward.

The phase order is playback time, legacy animation sampling, optional plugin
evaluation, pose application, local/world transform propagation, then skinning. These phases have explicit `Schedule::SetOrder`
dependencies because cross-entity component accesses are not inferred by the
scheduler. User systems touching joint transforms or playback state must declare
their relationship to `advance_animations`, `animate_targets`,
`apply_animation_poses`, `update_local_matrix`, `update_world_matrix_flat`, and
`skin_meshes` as appropriate.
The current scheduler executes each task's entity loop serially; this version
updates palettes serially; vertex deformation is parallel GPU work.

Extract draws after the update:

```cpp
render::pbr::RenderDrawItem forward =
    render::pbr::RenderDrawItem::FromEntity(mesh_entity);
render::deferred::RenderDrawItem deferred =
    render::deferred::RenderDrawItem::FromEntity(mesh_entity, object_id);
```

Both helpers capture the shared Mesh, world transform and current `SkinPose`
snapshot. Extraction before a skinned instance has been evaluated throws.
For manually assembled draw items, pass `skin_pose` explicitly.

Toon, PBR and Deferred share `render::GpuSkinning`. A compute pass consumes
immutable source streams and the captured joint palette, then writes position,
normal, tangent and bitangent buffers. Graphics passes declare vertex reads of
those outputs; RenderGraph supplies queue dependencies, barriers and retention.
Toon outline and fill reuse the same deformation. Other attributes, indices and
materials remain shared. Transient output buffers are pooled by RenderGraph;
there is no runtime vertex readback or per-frame upload of deformed vertices. Deferred geometry caching includes the deformation snapshot identity;
keying only by Mesh would incorrectly combine independent poses. `Submit` and
`SetFrame` retain the captured snapshot across later Scene updates.

## Minimal setup

This example assumes a prepared `shared_mesh` with BlendIndex/BlendWeight
attributes. Its palette has one joint, and its bind pose has identity transforms.
The same setup can be repeated with the same assets to create independent actors.

```cpp
asset::AnimationTrack track{.target = 7};
track.positions.keys = {
    {0.0, math::vec3f(0.0f)},
    {1.0, math::vec3f(2.0f, 0.0f, 0.0f)},
};
std::shared_ptr<asset::AnimationClip> clip =
    std::make_shared<asset::AnimationClip>(1.0, std::vector{track});
std::shared_ptr<asset::Skin> skin =
    std::make_shared<asset::Skin>(std::vector{math::mat4f::identity()});

ecs::Entity actor = scene.GetWorld().GetEntityManager().Create(asset::Transform(math::mat4f::identity()), asset::RelationShip{}, asset::MetaInfo("Actor"));
actor.Emplace<asset::AnimationPlayer>(clip);

ecs::Entity joint = scene.GetWorld().GetEntityManager().Create(asset::Transform(math::mat4f::identity()), asset::RelationShip(actor), asset::MetaInfo("Joint"));
joint.Emplace<asset::AnimationTarget>(actor, 7);

ecs::Entity body = scene.GetWorld().GetEntityManager().Create(asset::Transform(math::mat4f::identity()), asset::RelationShip(actor), asset::MetaInfo("Body"), asset::MeshComponent{shared_mesh});
body.Emplace<asset::SkinnedMesh>(skin, std::vector{joint});

scene.GetWorld().Update(job_system, 0.25);
render::pbr::RenderDrawItem draw = render::pbr::RenderDrawItem::FromEntity(body);
```

See [animation_test.cpp](../../hitagi/engine/asset/test/animation_test.cpp) for
independent players, shared joints, multiple influences, attachments and invalid
bindings. [skinning_test.cpp](../../hitagi/engine/render/test/skinning_test.cpp)
also renders two shared-Mesh instances and checks GPU pixels on both backends.

## Sampling and coordinate contract

Curves support Step and Linear interpolation, with shortest-arc quaternion slerp
for rotation. Keys must have strictly increasing finite times within the clip.
Quaternion keys are normalized when the clip is constructed. Missing channels
leave the current Transform field unchanged; clip switching does not implicitly
restore a rest pose. Pausing stops the clock while continuing to sample, so
assigning `player.time` provides seeking. Nonlooping playback clamps to the
endpoints. A zero-duration clip samples time zero.

For mesh world matrix `M`, joint world matrix `J[i]`, and inverse bind matrix
`B[i]`, the mesh-local skinning palette is:

```text
palette[i] = inverse(M) * J[i] * B[i]
```

`B[i]` maps bind-pose mesh-local positions to joint-local positions. The renderer
subsequently applies `M`; it must not apply the character transform a second
time. This convention supports different mesh and joint parents.

There are up to four influences per vertex. Nonnegative finite weights are
normalized during evaluation. Zero-weight slots ignore their indices; vertices
with four zero weights retain their original mesh-local attributes. Invalid
nonzero indices, incompatible palette sizes, missing attributes and singular
mesh transforms are rejected. Normals use the inverse transpose of the blended
matrix. At a singular blend they retain the source normal; this is a defined
fallback for a degenerate LBS pose, not a solution to LBS collapse artifacts.

## Current limits

- USD/UsdSkel import is available through the editor importer and USD Viewer;
  see the import contract below. HTGC v8 preserves node hierarchy, shared clips
  and skins, independent players and both target/palette mappings. See the
  [cooked animation encoding](cooked_binary_format.md#animation-and-skin-encoding).
  Independent external clip packages and cross-file rig/clip resolution are not
  implemented.
- Each legacy player samples one clip. Base provides root-motion extraction;
  the optional Rig plugin provides pose constraints and IK. Animation graphs,
  retargeting, events and additive layers remain unsupported.
- Runtime skin updates validate immutable weights/indices once per source and
  palette size, then update matrices and conservative bounds in O(joints).
  `SkinnedMesh::cpu_deformation` is an explicit offline/test reference option,
  disabled by default. Its vertex snapshots preserve the older CPU oracle for
  import tests and CPU/GPU numerical comparisons.
- Per-joint source bounds include all vertices with a positive influence;
  transformed bounds are unioned, including unweighted vertices. Nonnegative
  normalized LBS stays inside this conservative union. USD Viewer uses these
  bounds without scanning deformed vertices. They may be looser than exact
  mesh bounds; source assets and cooked records still contain no baked bounds.
- Rendering and Scene mutation remain host-serialized. Snapshots protect later
  frame consumption, not concurrent mutation of shared assets or a World.

## USD import and preview

`UsdSceneImporter` resolves bindings with `UsdSkelCache`, including inherited
bindings and native instance proxies. It converts the result to runtime assets
and ECS components before returning; no live USD stage is retained by Scene.

- Each Skeleton produces joint entities under its scene entity. Rest transforms
  supply joints absent from the animation. The bound animation produces a player
  on the Skeleton entity and targets on the animated joints.
- Animation tracks follow the animation's joint order. Skin palettes follow
  the mesh's `skel:joints`, falling back to the Skeleton order. Names resolve
  these two orders independently. Constant rigid influences and vertex
  influences both expand to the triangulated vertex stream.
- Native instances share compatible Mesh/Skin resources, while joint entities,
  playback clocks and deformed buffers remain per instance. Players referencing
  the same animation source share its Clip. Multiple meshes can bind one joint
  hierarchy, as in body and clothing.
- USD row-vector matrices are transposed for Hitagi. Each inverse bind matrix is
  `inverse(skeletonBind[i]) * geomBindTransform`. The final world position follows
  the Skeleton transform, including when the mesh has its own transform.
- Clip seconds are `(timeCode - startTimeCode) / timeCodesPerSecond`; display FPS
  does not determine playback speed. All clips use the stage's authored range,
  or the union of bound animation sample times when no range is authored.

The initial importer supports classic linear skinning with up to four nonzero
influences per point. It normalizes weights and rejects invalid indices, negative
weights, zero sums and excess influences. Unsupported blend shapes, dual
quaternion skinning, animated bindings/mesh points/topology, and animated or
reset-stack Skeleton ancestor transforms report errors. Ordinary unweighted
meshes under a Skeleton binding remain static. Only the bound clip is imported;
there is no clip library or retargeting policy.

Run the [USD Viewer](../../examples/usd-viewer/README.md) and choose **Open USD →
Skeletal animation**. The sample contains two native instances sharing geometry
and using different clips. Playback, pause and restart drive the same ECS systems
as programmatic scenes; framing uses the current deformed bounds.

[usd_skel_test.cpp](../../hitagi/editor/test/usd_skel_test.cpp) compares imported
deformation against OpenUSD, including differing joint orders, nonzero starting
time codes, independent instances and nonidentity coordinate transforms.
