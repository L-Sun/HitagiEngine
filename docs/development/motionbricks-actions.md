# MotionBricks action conditioning

Source: Quaternius Universal Animation Library Standard (CC0),
https://quaternius.itch.io/universal-animation-library . Original archive, license and
SHA-256 provenance are in `assets/motionbricks/actions/source/`.

## Data and authoring

`tools/motionbricks/prepare_actions.py` runs in Blender 5.2, retargets Quaternius rotations
onto SOMA, and composes Punch_Cross, Jump_Start / Jump_Loop / Jump_Land, and Roll; Punch_Jab supplies the alternating combo hit.
JumpAttack overlays the punch on the jump's upper body. Source trajectories define a
forward jump and roll with no initial backward root displacement.

Outputs are `assets/motionbricks/actions/action-library.blend` (editable actions and
markers), `assets/motionbricks/actions/manifest.json`, and
`assets/motionbricks/prepared/actions.txt` (HMA1, 77-joint world poses at 30 Hz).
`validate_action_bake.py` checks Blender evaluation against the exported poses.

## Runtime: reference clip -> four targets -> model -> character

All four actions, including their preparation and core, use MotionBricks inference.
`PlacedAbilityFrame` is called only by `AbilityTarget`; it never writes the character pose.
Each simulation tick in `AdvanceAbility` samples decoded `generated` frames. Attack phases time-resample that model output; they never sample the source clip for display.
Existing model endpoint correction, character retargeting and foot IK remain downstream.
Yellow skeletons display the four input targets. Cyan shows the generated SOMA pose.

The state machine selects these first-frame indices (zero-based):

| Reference | Consecutive four-frame windows start at |
|---|---|
| Punch / Jab | 4, 8, 18, 26 |
| ForwardJump | 12, 22, 34, 44 |
| JumpAttack | 12, 20, 34, 44 |
| ForwardRoll | 12, 24, 36, 56 |

A window beginning at 12 contains 12,13,14,15. All four poses use one placement captured
at action start; their spatial differences and 1/30-second sampling intervals are preserved.
The model receives the last four actual generated history poses and the next four target
poses. Each interval currently constrains the model to six tokens (its supported minimum),
which produces 17 new simulation steps / 0.567 seconds in this runtime. Attack phases then resample the decoded interval into 6/4/6/8 simulation steps for preparation, extension, withdrawal and final recovery (0.8 seconds total). Other actions retain model timing. Consequently,
the executed timing differs from the source clip and the raw model duration. The reference phase advances
through each interval, driving stage labels and strike markers; it does not drive pose playback.

F attacks; J jumps; early-air F branches to JumpAttack and replans from generated history;
Ctrl rolls. Held WASD chooses the action heading using camera-relative input. R resets.
Held action buttons trigger once. A new F press during preparation, extension or withdrawal buffers one next hit. Cross -> Jab -> Cross chains up to three hits, preserving generated history and world placement; intermediate idle recovery is skipped. The full three-hit chain takes 56 simulation steps (1.867 seconds). The overlay displays combo index and buffered input. Other action commands are ignored until completion.
The final target window returns to idle, after which locomotion control resumes.

Strike markers are scheduled events; they do not prove physical contact. This prototype
uses a flat floor and timed airborne stages. Enemy hits/damage, obstacle collision, arbitrary
terrain landing and hair/skirt collision remain outside the implementation. Model output
can deviate from the reference between target windows. Endpoint correction is retained
as in the existing MotionBricks runtime, so displayed results include that postprocessing.

## Validation

Windows x64 Release, isolated XMAKE_CONFIGDIR `temp/motionbricks/compare-config`:

```powershell
xmake build -j 8 motionbricks-demo
xmake r motionbricks-demo --test-actions --compare-out temp/motionbricks/action-library/model-test-actions
python tools/motionbricks/validate_action_inputs.py temp/motionbricks/action-library/model-test-actions/model.jsonl
```

The key replay tests held attack, rejected roll during attack, jump, aerial attack and roll.
Model input logs include ability, stage, source frame index, all eight guide poses and raw /
corrected output root trajectories. The input validator checks all 30 model joints across
all four target frames against the original exported reference, allowing one rigid transform.

2026-10-31 generated-action replay: 17 target-window inferences, 240 executed model-derived frames,
completions 1/1/1/1, two strike markers, four recovery windows; maximum root step 0.118996 m.
Target-window validation: maximum position error 0.00000173 m; quaternion error 3.2e-7.
Blender bake/export maximum position error: 0.000001996 m.

The earlier 160-action soak tested the previous hybrid implementation; it is not evidence
for this revised all-generated action path. The unrelated PocketJS invalid-string-length
failure was not reproduced during that soak, and its root cause remains unresolved.

Combo replay: F at render frames 60/80/112 produces one three-hit chain; a held F at
700..759 produces one additional single hit. Total: two completed chains, four strike
events, 14 model target windows, 80 model-derived simulation frames. There is no idle
frame inside the three-hit chain. Maximum root step was 0.039434 m. Captured
model target windows still match the authored reference within 0.0000018 m.
Hand trajectories and frame strips were inspected; this verifies the added sequencing
and visible extension/retraction, not fighting-game animation quality or collision accuracy.
