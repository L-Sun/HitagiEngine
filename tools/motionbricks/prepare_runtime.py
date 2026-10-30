"""Run in Blender after blender_prepare.py: static character and runtime calibration.

No generated animation is consumed. The bank contains conditioning poses, not
playback clips. The C++ demo generates its displayed motion with TensorRT.
"""
import json
import math
from pathlib import Path
import re
import sys

import bpy
from mathutils import Matrix, Quaternion, Vector

sys.path.insert(0, str(Path(__file__).parent))
from blender_prepare import load_source, TO_MODEL

repo = Path(__file__).resolve().parents[2]
bundle = repo / "assets/motionbricks"
out = bundle / "prepared"
calibration = json.loads((out / "retarget_calibration.json").read_text(encoding="utf-8"))
bpy.ops.wm.open_mainfile(filepath=str(out / "sour_generated.blend"))
arm = next(o for o in bpy.context.scene.objects if o.type == "ARMATURE")
arm.animation_data_clear()
for bone in arm.pose.bones:
    for constraint in list(bone.constraints):
        bone.constraints.remove(constraint)
    bone.matrix_basis = Matrix.Identity(4)
bpy.context.view_layer.update()
ordered = sorted(arm.data.bones, key=lambda b: len(b.parent_recursive))
aliases = {}
for side in ("左", "右"):
    aliases.update({side+"足D": side+"足", side+"ひざD": side+"ひざ", side+"足首D": side+"足首",
                    side+"足先EX": side+"つま先", side+"腕W": side+"腕", side+"ひじW": side+"ひじ",
                    side+"手首W": side+"手首"})
# Include helper drivers as dependencies, not just skeletal parents.
pending, ordered, emitted = ordered, [], set()
while pending:
    ready = [b for b in pending if (b.parent is None or b.parent.name in emitted)
             and (b.name not in aliases or aliases[b.name] in emitted)]
    assert ready, "Cyclic runtime helper dependencies"
    for bone in ready:
        ordered.append(bone)
        emitted.add(bone.name)
        pending.remove(bone)
target_names = [b.name for b in ordered]
mapping = {m["target"]: m for m in calibration["mappings"]}
clips = [
    "locomotion/idle/idle_loop_001__A268.bvh",
    "locomotion/bs_1d_linear/Neutral_walk_forward_002__A057.bvh",
    "locomotion/bs_1d_linear/neutral_jog_ff_180_R_001__A534.bvh",
]
sources = [load_source(bundle, bundle / "upstream/assets/motions/soma" / c) for c in clips]
manifest, names, parents, rest, bind, _ = sources[0]
model_names = manifest["joint_names"]

def steady_cycle(frames, mode):
    """Choose a same-phase stride entirely inside straight, sustained locomotion.

    Keep three following samples for the four-frame token crossing the seam.
    The runtime reanchors each token, so root translation need not loop to zero.
    """
    import numpy as np
    roots = np.array([tuple(f[0].translation) for f in frames])
    model_indices = [names.index(n) for n in model_names]
    headings, features = [], []
    for frame in frames:
        across = frame[model_indices[26]].translation - frame[model_indices[22]].translation
        forward = Vector((0, 1, 0)).cross(across)
        heading = math.atan2(forward.x, forward.z)
        headings.append(heading)
        turn = Quaternion((0, 1, 0), -heading)
        features.append([tuple(turn @ (frame[j].translation - frame[0].translation)) for j in model_indices])
    features = np.array(features)
    speeds = np.linalg.norm((roots[3:] - roots[:-3])[:, [0, 2]], axis=1) * 10
    headings = np.unwrap(headings)
    minimum_speed = .7 if mode == 1 else 1.3
    period_range = range(24, 49) if mode == 1 else range(16, 37)
    best = None
    for start in range(len(frames) - 40):
        for period in period_range:
            end = start + period
            if end + 3 >= len(frames): continue
            if speeds[start:end+1].min() < minimum_speed: continue
            if np.ptp(headings[start:end+3]) > .15: continue
            # Match both pose and local velocity across the four-frame seam.
            a, b = features[start:start+4], features[end:end+4]
            score = float(np.mean((a-b)**2) + np.mean((np.diff(a,axis=0)-np.diff(b,axis=0))**2))
            if best is None or score < best[0]: best = (score, start, end)
    if best is None: raise RuntimeError(f"No steady gait cycle for {clips[mode]}")
    score, start, end = best
    report = dict(clip=clips[mode], start_frame=start, end_frame_exclusive=end,
                  period_frames=end-start, fps=30, seam_position_rms_m=math.sqrt(score),
                  minimum_token_speed_mps=float(speeds[start:end].min()),
                  maximum_token_speed_mps=float(speeds[start:end].max()))
    print("Conditioning cycle:", report)
    return frames[start:end+3], report

conditioning_frames = [sources[0][-1]]
cycle_reports = []
for mode in (1, 2):
    frames, report = steady_cycle(sources[mode][-1], mode)
    conditioning_frames.append(frames)
    cycle_reports.append(report)
(out / "conditioning_cycles.json").write_text(json.dumps(cycle_reports, indent=2), encoding="utf-8")

# Calibrate shoes from their actual rear/forefoot sole geometry. A high-heel
# ankle-to-toe bone axis is not the sole support plane.
shoe_report = {}
runtime_marks = {}
for side, source_side in (("左", "Left"), ("右", "Right")):
    foot = arm.data.bones[side+"足首"]
    vertices = []
    for mesh in (o for o in bpy.context.scene.objects if o.type == "MESH"):
        groups = {g.index for g in mesh.vertex_groups if g.name in
                  (side+"足首", side+"足首D", side+"つま先", side+"足先EX")}
        vertices.extend(arm.matrix_world.inverted() @ mesh.matrix_world @ v.co for v in mesh.data.vertices
                        if sum(g.weight for g in v.groups if g.group in groups) > .5)
    assert vertices, "Missing shoe geometry"
    middle = (min(v.y for v in vertices)+max(v.y for v in vertices))/2
    points = []
    for region in ([v for v in vertices if v.y > middle], [v for v in vertices if v.y <= middle]):
        low = min(v.z for v in region)
        patch = [v for v in region if v.z < low+.001]
        point = sum(patch, Vector())/len(patch)
        point.z = low
        points.append(point)
    runtime_marks[side] = {key: list(foot.matrix_local.inverted() @ point) for key,point in zip(("heel","toe"),points)}
    rotations = []
    for frame in sources[0][5][:30]:
        across = frame[names.index("RightLeg")].translation-frame[names.index("LeftLeg")].translation
        forward = Vector((0,1,0)).cross(across)
        heading = math.atan2(forward.x,forward.z)
        q = Quaternion((0,1,0),-heading) @ frame[names.index(source_side+"Foot")].to_quaternion()
        if rotations and q.dot(rotations[0]) < 0: q.negate()
        rotations.append(q)
    reference = Quaternion(tuple(sum(q[i] for q in rotations) for i in range(4))).normalized()
    desired = (TO_MODEL @ foot.matrix_local).to_quaternion()
    old = Quaternion(mapping[side+"足首"]["source_to_target_rotation"])
    old_rotation = reference @ old
    old_span = old_rotation @ Vector(runtime_marks[side]["toe"])-old_rotation @ Vector(runtime_marks[side]["heel"])
    correction = reference.inverted() @ desired
    mapping[side+"足首"]["source_to_target_rotation"] = list(correction)
    # The shoe forefoot is rigid in this demo; preserve its authored relation to
    # the ankle instead of imposing bare-foot toe articulation on the shoe.
    mapping.pop(side+"つま先")
    shoe_report[side] = dict(heel_armature=list(points[0]),forefoot_armature=list(points[1]),
        heel_local=runtime_marks[side]["heel"],forefoot_local=runtime_marks[side]["toe"],
        previous_neutral_forefoot_above_heel_m=old_span.y,
        source_to_shoe_rotation_wxyz=list(correction))
(out / "shoe_calibration.json").write_text(json.dumps(shoe_report,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")

def values(v):
    return " ".join(format(float(x), ".9g") for x in v)

def pose(m):
    q = m.to_quaternion().normalized()
    return values([*m.translation, q.x, q.y, q.z, q.w])

lines = [f"HMB1 {len(names)} {len(ordered)} {len(clips)} {calibration['root_scale']:.9g}"]
for i, name in enumerate(names):
    lines.append(f'{parents[i]} {model_names.index(name) if name in model_names else -1} {pose(rest[i])}')
for bone in ordered:
    name = bone.name
    driver = -1
    # MMD grants and sleeve auxiliary IK are authoring constraints. Runtime uses
    # explicit body-driver bind offsets for their deformation bones.
    if name in aliases:
        driver = target_names.index(aliases[name])
    parent = target_names.index(bone.parent.name) if bone.parent else -1
    world = TO_MODEL @ bone.matrix_local
    local = (TO_MODEL @ bone.parent.matrix_local).inverted() @ world if bone.parent else world
    src = names.index(mapping[name]["source"]) if name in mapping else -1
    q = mapping[name]["source_to_target_rotation"] if name in mapping else [1, 0, 0, 0]
    token = re.sub(r"\W", "_", name)
    lines.append(f'"{token}" {parent} {driver} {src} {pose(local)} {pose(world)} {values([q[1],q[2],q[3],q[0]])}')
for side in ("左", "右"):
    indices = [target_names.index(side+s) for s in ("足", "ひざ", "足首")]
    marks = runtime_marks[side]
    lines.append(" ".join(map(str, indices)) + " " + values(marks["heel"]) + " " + values(marks["toe"]))
for frames in conditioning_frames:
    # Locomotion banks contain one steady stride plus three lookahead samples.
    lines.append(str(len(frames)))
    for frame in frames:
        lines.extend(pose(m) for m in frame)
(out / "runtime.txt").write_text("\n".join(lines)+"\n", encoding="utf-8")
bpy.ops.object.select_all(action="DESELECT")
arm.select_set(True)
for obj in bpy.context.scene.objects:
    if obj.type == "MESH":
        obj.select_set(True)
bpy.context.view_layer.objects.active = arm
# Static tiled floor provides a world-space reference for interactive movement.
bpy.ops.mesh.primitive_plane_add(size=40, location=(0,0,-.003))
floor = bpy.context.object
floor.name = "RuntimeGround"
material = bpy.data.materials.new("RuntimeGroundMaterial")
material.use_nodes = True
bsdf = material.node_tree.nodes.get("Principled BSDF")
bsdf.inputs["Roughness"].default_value = 1
image = bpy.data.images.new("runtime-ground", width=128, height=128)
pixels = []
for y in range(128):
    for x in range(128):
        c = .16 if ((x//64+y//64)%2) else .25
        pixels.extend((c,c,c,1))
image.pixels = pixels
image.filepath_raw = str(out / "runtime-ground.png")
image.file_format = "PNG"
image.save()
texture = material.node_tree.nodes.new("ShaderNodeTexImage")
texture.image = image
material.node_tree.links.new(texture.outputs["Color"],bsdf.inputs["Base Color"])
for uv in floor.data.uv_layers.active.data:
    uv.uv *= 20
floor.data.materials.append(material)
arm.select_set(True)
for obj in bpy.context.scene.objects:
    if obj.type == "MESH": obj.select_set(True)
bpy.context.view_layer.objects.active = arm
bpy.ops.wm.usd_export(filepath=str(out / "sour_runtime.usdc"), selected_objects_only=True,
    export_animation=False, export_armatures=True, only_deform_bones=False,
    export_shapekeys=False, triangulate_meshes=True, export_materials=True,
    export_textures_mode="NEW", convert_orientation=True,
    export_global_up_selection="Y", export_global_forward_selection="NEGATIVE_Z")
print("MOTIONBRICKS_RUNTIME_ASSETS_OK", len(names), len(ordered))
