# Animation Rig

`asset.plugins:rig` is re-exported by `asset.plugins` and `asset`. It modifies a
local `Pose` using a layout-specific, ordered list of constraints. It does not
retarget skeletons, require Humanoid, query physics, or move the actor.

## Definition and instance

`RigDefinition(shared_ptr<const JointLayout>, RigDesc)` validates immutable CPU
data. Descriptions use `RigJointId` (the layout's logical `JointSlot::target`,
not a slot or mesh palette index), `RigControlId`, and `RigConstraintId`.
Construction resolves joint and control references to slots, rejects duplicate
IDs, missing references, zero axes, invalid weights, invalid direct IK chains,
and targets inside their own solved subtree. The resulting definition can be
shared across actors. Constraints execute in description order; overlapping
writes are intentional and later operations see earlier results. There is no
implicit dependency sorting or iterative/global constraint solving.

`RigInstance` owns control values, constraint weight multipliers, work Pose,
model matrices and diagnostic buffers. Allocate one per actor. `SetControl`
updates a persistent value; `valid=false` invalidates it. `SetWeight` multiplies
the authored operation weights and accepts only [0,1]. `Reset` invalidates all
controls and clears reports, retaining weights and enabled state. Solvers are
currently stateless: zero delta evaluates normally, and discontinuity
has no additional effect. Pausing target updates is a host policy.

`Evaluate(context, input, output)` supports aliasing and returns an instance-owned
span of diagnostics, valid until its next evaluation/reset. Every evaluation
starts from input, not previous output. The host must supply a fresh base Pose;
feeding a weighted result back as the next base would accumulate corrections.
Invalid input throws before writing output. Missing controls and geometric
degeneracy skip an operation. Disabled/zero-weight rigs pass input through.

## Coordinates and solvers

All controls use model space: the `JointBinding` frame, not world space. A
`RigJointTarget` combines the current joint model transform and a rigid local
offset. `LocalPose` is reused for rigid controls/offsets, with unit scale required.
All joints must have unit scale; scale the external binding frame instead.
For a uniformly scaled actor, transform world target positions by its full
inverse matrix and orientations by its inverse rotation, without injecting scale
into the control. Nonuniform world-frame scaling/shear is outside this contract.

- `AimConstraint`: aims a local axis at the target position. Without a pole,
  uses the shortest rotation and preserves the remaining input twist. With a
  pole, projects the local up axis and pole vector onto the aim plane and aligns
  them. An antiparallel aim has a deterministic perpendicular-axis fallback;
  coincident targets or collinear pole directions report `Degenerate`.
- `TwoBoneIKConstraint`: analytic root/middle/tip solve, with separate position
  and tip orientation weights. Positions and lengths are never stretched. The
  reachable interval is clamped and unreachable targets report a residual.
  A collinear pole falls back to the incoming bend plane, then a deterministic
  perpendicular direction. This fallback is not a temporal stabilizer; supply a
  well-conditioned pole for animated controls. Full target orientation is
  optional and applied after weighted parent rotations.
- `CopyLocalRotationConstraint`: copies a source joint's current local rotation
  into a separate deformation joint, with a weight. Both joints must use the
  same local rotation basis; this performs no retargeting or basis conversion.
  Destination translation and scale are preserved, and descendants are updated.

`RigDiagnostic` reports `Solved`, `Skipped`, `MissingTarget`, `Degenerate`, or
`Unreachable`. Position error is in model units; angular error is radians.
Residuals measure the actual weighted result. Aim reports direction error rather
than endpoint distance. CopyLocalRotation reports status without an angular
residual. A solved weighted operation need not reach the target.

Model matrices update only the changed subtree after a rotation, reusing
preallocated dirty flags and matrix storage. The topological scan still visits
the layout; no claim of crowd-scale optimization is made. Layout compatibility includes logical IDs,
parents and rest transforms, not merely joint count.

## Composition

```cpp
import asset;

asset::RigDesc desc;
desc.controls = {{.id = {100}, .name = "left_foot"},
                 {.id = {101}, .name = "left_knee_pole"}};
desc.constraints.push_back({.id = {1}, .operation = asset::TwoBoneIKConstraint{
    .root = {hip_target_id}, .middle = {knee_target_id}, .tip = {ankle_target_id},
    .target = asset::RigControlId{100}, .pole = asset::RigControlId{101}}});
auto definition = std::make_shared<asset::RigDefinition>(layout, std::move(desc));
auto& rig = actor.Emplace<asset::RigInstance>(definition);
rig.SetControl({100}, {.model_transform = {.position = foot_in_model}, .valid = true});
rig.SetControl({101}, {.model_transform = {.position = pole_in_model}, .valid = true});
plugins.Add(world, asset::RigPlugin{});
```

`RigPlugin` installs `solve_rigs` before `apply_animation_poses`. The host must
explicitly order its producer and controls before `solve_rigs`; the plugin does
not select or infer who owns the base pose:

```cpp
schedule.SetOrder("sample_base_pose", "move_character");
schedule.SetOrder("move_character", "update_rig_controls");
schedule.SetOrder("update_rig_controls", "solve_rigs");
```

The controller consumes root motion and resolves movement first. Ground/contact
logic then converts targets using the actual actor transform. Rig writes Pose,
base PoseSystem writes entities, and existing secondary physics/skinning follow.
Rig leaves the root-motion delta untouched. Multiple base producers remain
invalid. The host owns any feedback policy for the base pose producer.

Legacy `AnimationPlayer`/`AnimationTarget` directly writes entities. It does not
automatically feed this stage: hosts using it with Rig must sample clips into a
fresh Pose with `SamplePose` and avoid competing entity writers.

## Authoring and current boundary

There is no general Rig resource/cook encoding yet. HTGC rejects `RigInstance`
even without `AnimationPose`, rather than silently discarding behavior. Build
CPU definitions in the host for now. No Blender node-graph import, arbitrary
chain solver, arbitrary Euler joint limits, stretching, full-body IK, or visual editor is claimed.
TwoBoneIK now optionally accepts a local-space signed `RigHingeLimit` on the
middle joint. It locks orthogonal swing and uses projected CCD after the analytic
seed, with bounded iterations and tolerance. Partial operation weights soften
the result, including limits; residuals remain observable.
Foot contact detection, world/platform anchors, lock/release state, pelvis policy,
and collision queries belong to a character controller and are not implemented
by this plugin. TwoBoneIK alone is not a foot-lock system.

## Verification

`RigTest.*` covers endpoint/length preservation, unreachable and degenerate
inputs, aim pole/twist behavior, mirrored IK poles, weighted residuals, independent
instances, reset/disable, layout validation, ordered target reads and ECS execution.
`CookedPoseProducerTest` checks rejection of unencoded rig behavior.
