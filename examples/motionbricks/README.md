# MotionBricks runtime demo

Windows / NVIDIA CUDA 12 / TensorRT 11.4 demonstration using Sour Miku.
The character USD has **no baked animation clip**. TensorRT generates locomotion
and transitions at runtime. Idle and seated states loop source poses, following
the official runtime state machine. Blender prepares assets and calibration.

From the repository root, using the prepared local evaluation assets:

```powershell
xmake f -m release --toolchain=msvc --vs=2026 --motionbricks=y
xmake build motionbricks-demo
xmake r motionbricks-demo
```

The command above preserves this workstation's release/MSVC 2026 configuration;
adapt those existing compiler settings on another machine rather than resetting them.
The option defaults off so normal engine/viewer builds do not require TensorRT.
The demo uses the system CUDA 12 SDK and the repository's pinned TensorRT xmake
package. The current local TensorRT plan is specific to this GPU/SDK; rebuild it
after changing either, using `docs/development/motionbricks-data.md`.

The interactive demo currently uses a clean floor with object interactions disabled.
Keys 5–8 and Space below describe the retained environment regression path;
`--test-preset <index> --variant <variant>` exercises it.

Controls:

- W / A / S / D: camera-relative movement; linear styles turn toward movement.
- Hold either Shift: faster movement. Release movement keys: generate a transition to idle.
- 1–4: change locomotion style in place, preserving the current pose and position.
- 5–8: highlight an interaction approach arrow; this never moves the character.
- Tab: next variant of a nearby available interaction, otherwise next locomotion speed/style.
- Space: perform the nearby, correctly faced interaction; press again to stand from sitting.
- F: attack; press again to buffer Cross / Jab / Cross (up to three hits). Holding triggers once.
- J: forward jump; early-air F branches into a jump attack.
- Ctrl: forward roll. Actions use reference windows as model inputs. See `docs/development/motionbricks-actions.md`.
- Q / E: turn in place; directional and crouch presets retain facing while WASD strafes.
- R: reset character, generation history, and contact anchors.
- Mouse drag / wheel: orbit / zoom. The camera follows the character.
- Text fields capture typing; movement/reset are suppressed while editing them.
- F9: start/stop recording the current view. The status bar shows REC, saving,
  and saved/error states. Closing the window normally also finishes recording.
- F2: cycle full diagnostics, hidden, and trajectory-only diagnostics.
- F4: pause/resume motion generation; mouse camera controls remain available.
- N while paused: advance one 30 Hz simulation step.

Diagnostics show the actual last inference, not another synthesized prediction:
yellow skeletons are the active preset's one to four target poses, gray is the
four-frame history root path, magenta is the decoded raw model root path, and green
is the endpoint-corrected path. Yellow dashed lines show intent, not dense inputs.
The network chooses timing within each target's allowed range. Cyan is the actual
SOMA pose before retarget IK. During idle/seated playback, stale inference guides
are hidden and only the current SOMA pose is drawn.
Source positions use the same scale/root offset as retargeting; proportions still
belong to SOMA. Orange draws the actual rendered character's hips, knees, ankles
and heel-to-toe support lines, and reports pelvis correction. These need not
coincide with SOMA positions: rotation retargeting preserves Miku's bone lengths.
Lines show through meshes. Approach arrows remain visible with diagnostics hidden;
an available interaction turns its arrow green and shows a Space prompt.
Recordings include these overlays and `model.jsonl`, saving the actual root,
velocity and pose input arrays, actual masks and selected duration, plus world-space guides and raw/corrected root
paths whenever a captured frame sees a new inference. Multiple inferences between
captures are not recorded. `--test-diagnostics` checks layer cycling, pause and step.

All eight SOMA documents from NVlabs/motionbricks commit
`54fc563aeaaf819a753d72cc9fdc6713989cdd43` are loaded, with 43 target nodes:

| Key | Public preset | Tab variants |
| --- | --- | --- |
| 1 | locomotion_bs1_linear | Walk, Jog, Run |
| 2 | locomotion_bs2_directional | WASD forward/backward/left/right, including diagonal blends |
| 3 | locomotion_bs2_directional_crouch | Crouched directional movement |
| 4 | locomotion_style_variations_bs1_linear | Skipping, Vogue, Injured Torso, Injured Leg, Zombie, Combat |
| 5 | avoidance | Walk, Jog |
| 6 | jump_0_5m | Jump, Jump2 |
| 7 | jump_0_75m | OneHandedJump0, OneHandedJump1, NoHandsJump |
| 8 | sit | Space to sit; Space again to stand |

The importer preserves exact authored sampling times (interpolating source BVH
quaternions before FK), hierarchy placement, hips constraints, pose counts, token
ranges, sequence order, early-transition values and hand pin flags. These are
scene affordances associated with the authored boxes, not a general mesh-based
affordance detector. At runtime, every object is queried independently of the
selected locomotion style. Entry-side distance, lateral clearance and actual body
heading determine whether Space is available. The character walks there using
normal controls; selection and triggering never teleport it. Generated transitions
start from its actual four-frame history and end at the object's placed targets.
Tab cycles an available object's variants. R returns to the arena origin.
Obstacles are visual guides, not physical collision shapes; walking through one
without using its interaction is still possible. Object placements currently match
the imported static arena; moving arbitrary USD geometry does not update affordances.

The current free jump uses the authored action library described in
`docs/development/motionbricks-actions.md`. It remains a generated, timed action
with floor contact handling rather than a ballistic physics controller.

Recordings go to a separate timestamped folder under `temp/motionbricks/recordings/`.
Each contains `video.mp4`, original PNG frames, a timestamped `state.csv` with
controls and root/camera positions, and `joints.csv` with rendered world-space
bone transforms. Tell the developer when a recording is ready; files stay local.
FFmpeg must be on PATH for MP4 export (installed on this workstation). Encoding
failure retains PNGs and diagnostics, with details in `encode.log`.
Capture targets at most 30 Hz, uses actual elapsed timestamps and skips captures
when PNG compression is busy. GPU readback still has overhead. Diagnostics are
sampled with captured frames, not a deterministic input replay. MP4 export runs
in the background; another recording can begin after export completes. Resizing
the window keeps the first capture's output size, fitting later frames within it.
Use `--test-recording` to exercise F9 stop/restart and shutdown finalization.

The floor is a visual reference with contact plane Y=0, not an environment collision
simulation. Movement axes follow the camera projected onto the floor.

## Current conditioning updates

Four-frame reference windows share one rigid placement. Locomotion estimates
phase from generated history and advances through reference gait cycles; speed
transitions blend phase-aligned walk/jog/run poses with acceleration limits.
Idle support anchors hand over to the new stance over 0.35 seconds.
The action controller generates each stage from clip target windows and buffers
combo inputs; attack timing resamples decoded model output. This is an
experimental integration with unresolved action-quality issues.

## Runtime path

`Controller` owns a persistent TensorRT engine, execution context, CUDA stream and
device buffers. The demo synchronously runs inference on its main thread when a
new segment is needed. It is not a background inference or frame-pacing design.

At 30 Hz it retains four SOMA poses. The controller follows
[Smart Locomotion, paper section 6.1](https://arxiv.org/html/2604.24833v1#S6.SS1):

1. Extrapolate a naive root target one second using the requested velocity.
2. Apply equation (6)'s critically damped spring to position and unwrapped heading,
   using velocity measured from generated history. Gamma is locally tuned to 4/s.
3. Place the selected official style's target poses at this spring target.
   Body motion comes from the authored sample times; planar target displacement
   comes from the movement command. Directional diagonals blend local transforms
   between the cardinal targets and reconstruct the skeleton with FK.
4. Encode the official 30-joint inputs with hip-based heading alignment. Masks
   follow the target's pose count; derivatives are unknown at the last context
   and last target frame. Padded target slots repeat the last supplied pose.
5. Let the model choose timing within the target's allowed token range. Its root
   module and decoder refine trajectories internally. The exported model does
   not expose the root module's intermediate trajectory.
6. Trim three boundary frames at both ends and apply endpoint correction. Moving
   controls replan every eight model frames, or after changed input with a
   three-frame minimum interval. Interaction sequences use authored early-exit
   counts instead. Render poses interpolate between 30 Hz simulation samples.

The public targets and sequences are exact imports. Keyboard speed selections,
cardinal diagonal blending, gamma=4/s, eight-frame moving replanning, and fixed
interaction eligibility thresholds are local control policies. Linear mode exposes its named
speed samples directly instead of a continuous analog-speed blendspace.

On stopping, a generated transition (locally limited to 6..8 tokens for braking)
finishes before source idle playback begins. The previous unconditional replanning
could indefinitely avoid the idle target and feed bent arms/twisted legs into the
next generation. Idle and sitting now follow upstream IdleState/SittingBehavior:
transitions are generated, then authored idle/seated loops are played and recorded
as history. Standing and crouching have separate idle loops. A new movement or
interaction request exits the loop immediately. This is runtime state control,
not an offline baked recording of the interactive session.

Model output and loop poses are retargeted to Miku, corrected with contact
hysteresis, foot anchors, two-bone IK and authored hand pins, then written to
ECS joints before world transforms and GPU skinning. D-bone and selected sleeve
helpers use explicit bind-space driver offsets. This is an approximation of the
authoring rig, not a runtime implementation of every MMD constraint.

History contains the actual displayed SOMA state (generated or looped), before target-space foot correction.
There is no Miku-to-SOMA feedback loop, pelvis collision solver, uneven-ground
query, skirt/hair simulation, or full-body collision guarantee.

## Assets and checks

Run Blender with `--background --factory-startup --python-exit-code 1 --python
tools/motionbricks/prepare_runtime.py` after the resource preparation described in
the data document. This exports `prepared/sour_runtime.usdc` and `runtime.txt`.
The latter records the 77-joint source layout, target bindings/axes, foot landmarks,
and source conditioning bank. Then run Blender with
`--background --factory-startup --python-exit-code 1 --python tools/motionbricks/prepare_presets.py`.
This exports `presets.txt`, the source manifest and the `sour_presets.usda` arena
layer. Original PMX, upstream JSON/BVH and model weights remain unchanged.

```powershell
xmake r motionbricks-demo --test
xmake r motionbricks-demo --test-variable-rate
xmake r motionbricks-demo --test-idle
xmake r motionbricks-demo --test-walk
xmake r motionbricks-demo --test-long-idle
xmake r motionbricks-demo --test-preset 6 --variant 1
xmake r motionbricks-demo --test-jump
xmake r motionbricks-demo --test-turn
python tools/motionbricks/validate_smart_locomotion.py
```

Each test also writes `temp/motionbricks/runtime/model.jsonl`. The Python validator
checks actual inference masks, authored velocity, the position/heading spring
equations, selected durations, trimmed output lengths and corrected endpoints.
Run it after a moving test; idle-only traces deliberately do not meet its coverage
requirement. Results are overwritten by the next test, so copy them if needed.

The two-minute idle regression walks, turns and runs before stopping, then
compares actual rendered pelvis, both wrists and both ankles across repeated
idle cycles. It checks position and rotation, as well as the existing sole and
knee constraints. Test output remains local under `temp/motionbricks/`.

Interaction tests first reject out-of-range and backward-facing requests and check
that selection does not teleport. They then walk from two model metres behind the
entry until the actual runtime query offers the interaction, and execute it.
The sitting case also stands back up. Free-jump tests complete two jumps without
an object; stationary-turn tests check the resulting heading and return to idle.

The ordinary control regression drives HID key states through idle, walk, turn, run, stop and reset for 600
render frames, runs actual TensorRT inference and DX12 rendering, and checks
directional displacement, stop drift, root continuity, contact-point heights and
the absence of animation clips. Local screenshots and `trace.csv` are written to
`temp/motionbricks/runtime/`. Foot landmark checks do not constitute mesh collision
testing. Inference time includes transfers and stream synchronization,
not total frame cost. Third-party asset licenses still apply; see the data document.

Before the Smart Locomotion change, validated on RTX 5080 / MSVC 2026 / TensorRT 11.4: 600 control frames passed,
36 generations before reset, 2.15 mm drift during the final second of the stop test,
and 5.82 ms median warmed inference including transfers (one run, not a benchmark).
The ordinary viewer's skeletal-animation recording smoke test also passed. Its full
UI journey currently fails the `visible:3` assertion; the same failure was reproduced
with the original HEAD viewer/camera sources. Logs are under `temp/motionbricks/`.

Camera follow uses the interpolated, retargeted display root in character/world
space, not the latest 30 Hz SOMA sample. Mixing these times caused alternating
camera-relative motion even though skin poses were interpolated. The presentation
check compares the actual ECS pelvis world position after scene evaluation with
the camera center each rendered frame and records `presentation.csv`. Both the
60 Hz test and alternating 120/40 Hz simulated frame intervals must keep horizontal
camera-relative step below 0.1 mm. Camera synchronization adds no extra smoothing.

Orbit zoom no longer stops at 10% of the framed distance. It retains only a 1e-6
numerical zoom guard to avoid a zero view direction, and the near clipping plane
shrinks with distance. The existing zoom-out limit is unchanged. Test mode injects
wheel input beyond the old limit and checks continued approach and valid clipping.

High-heel calibration uses the actual heel-bottom and forefoot-bottom mesh patches
in Blender, and aligns the source idle foot reference with the authored shoe pose.
The shoe forefoot retains its bind rotation instead of copying barefoot toe bends.
`prepared/shoe_calibration.json` records the landmarks and correction; regenerate
it with `tools/motionbricks/prepare_runtime.py`. The previous bone-axis calibration
raised the neutral left forefoot 34.5 mm above the heel.

Runtime contact solving tracks heel and forefoot independently, preserving single
contact heel strikes and toe-offs. Double contact aligns both support points with
the ground; a bounded pelvis lowering step keeps ankle targets within leg reach.
This is a flat-ground shoe adaptation, not a learned high-heel gait or full-body
collision solver. The 60 Hz, variable-rate and idle regressions require double
support samples and less than 5 mm support-point height error; observed maximum
error was below 0.007 mm. These landmark checks do not cover every mesh vertex.

Pelvis compression is limited to 3 cm for this shoe calibration. If the generated
root is higher than a straight target leg can reach, that vertical excess is first
removed for fully planted support; the 3 cm budget applies beyond that correction.
This prevents floating feet without expanding the stale-anchor crouch budget.
When an old contact
anchor falls outside the leg's reachable horizontal range, both shoe landmarks
move together to that boundary instead of forcing a deep crouch. This deliberately
allows support sliding when predicted contacts conflict with root movement; it
does not guarantee zero foot slip. The previous 15 cm lowering limit was reached
in sustained walking. `--test-walk` adds 24 seconds of continuous walking plus stop
and reset, checking support height, knee direction and maximum pelvis correction.

Leg IK takes its bend plane from the incoming hip/knee/ankle pose before moving
the ankle target. Near extension it blends toward body-relative forward, and
constrains the pole to a forward 120-degree cone. Using the knee's offset from
the corrected ankle axis previously selected a backward solution (48.8 mm behind
the body-relative leg plane in the control regression). Tests now check the signed
knee offset for both legs throughout idle, turns, walk and jog, including variable
frame intervals. This locomotion constraint is not intended for arbitrary acrobatics.
