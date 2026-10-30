"""Blender batch preparation: SOMA sampling, Sour calibration, preview and USD.

Run from the repository root with Blender --background --python-exit-code 1.
The original PMX is never edited. Outputs are local evaluation assets.
"""
import argparse
import json
import math
from pathlib import Path
import sys

import bpy
import numpy as np
from mathutils import Matrix, Quaternion, Vector
from pxr import Usd, UsdGeom, UsdSkel

sys.path.insert(0, str(Path(__file__).parent))
from prepare_data import inspect_bvh, write_json, sha256

TO_BLENDER = Matrix.Rotation(math.pi / 2, 4, "X")
TO_MODEL = TO_BLENDER.inverted()


def matrix_json(matrix):
    return [list(row) for row in matrix]


def load_source(bundle, clip, sample_times=None):
    manifest = json.loads((bundle / "upstream/motionbricks/assets/onnx/soma/soma_n1.json").read_text())
    stage = Usd.Stage.Open(str(bundle / "upstream/motionbricks/assets/human/soma_base_skel_minimal_no_root.usd"))
    skeleton = next(UsdSkel.Skeleton(p) for p in stage.Traverse() if p.IsA(UsdSkel.Skeleton))
    names = [str(p).split("/")[-1] for p in skeleton.GetJointsAttr().Get()]
    paths = [str(p) for p in skeleton.GetJointsAttr().Get()]
    scale = UsdGeom.GetStageMetersPerUnit(stage)
    assert UsdGeom.GetStageUpAxis(stage) == "Y" and scale == .01
    rest = []
    for m in skeleton.GetRestTransformsAttr().Get():
        matrix = Matrix(m).transposed()
        matrix.translation *= scale
        rest.append(matrix)
    parents = [paths.index(p.rsplit("/", 1)[0]) if "/" in p else -1 for p in paths]
    bind = []
    for i, local in enumerate(rest):
        bind.append(bind[parents[i]] @ local if parents[i] >= 0 else local.copy())
    info = inspect_bvh(clip, manifest["joint_names"])
    text = clip.read_text(encoding="utf-8").split("MOTION", 1)[1].splitlines()
    start = next(i for i, line in enumerate(text) if "Frame Time:" in line) + 1
    samples = np.loadtxt(text[start:])
    source_names = [j["name"] for j in info["joints"]]
    channels = np.cumsum([0] + [len(j["channels"]) for j in info["joints"]])
    local_frames = []
    # Decode every frame before quaternion interpolation; never interpolate Euler angles.
    for row in samples:
        frame = {}
        for i, joint in enumerate(info["joints"]):
            p = Vector(joint["offset_m"])
            q = Quaternion()
            for channel, value in zip(joint["channels"], row[channels[i]:channels[i + 1]]):
                axis = "XYZ".index(channel[0])
                if channel.endswith("position"):
                    p[axis] = float(value) * .01
                else:
                    v = Vector((0, 0, 0)); v[axis] = 1
                    q = q @ Quaternion(v, math.radians(float(value)))
            frame[joint["name"]] = (p, q.normalized())
        # Match upstream name remapping. Its Root is not part of the model skeleton.
        root_p, root_q = frame["Root"]
        assert root_p.length < 1e-5 and abs(root_q.w) > .99999, "Nonidentity BVH Root needs explicit composition"
        local_frames.append(frame)
    frames = []
    times = sample_times if sample_times is not None else (f / 30 for f in range(math.floor(info["duration_seconds"] * 30) + 1))
    for seconds in times:
        position = min(seconds * info["fps"], len(samples) - 1)
        a, b = int(position), min(int(position) + 1, len(samples) - 1)
        t = position - a
        world = []
        for i, name in enumerate(names):
            if name in source_names:
                pa, qa = local_frames[a][name]; pb, qb = local_frames[b][name]
                local = Matrix.LocRotScale(pa.lerp(pb, t), qa.slerp(qb, t), Vector((1, 1, 1)))
            else:
                local = rest[i]
            world.append(world[parents[i]] @ local if parents[i] >= 0 else local.copy())
        frames.append(world)
    return manifest, names, parents, rest, bind, frames


def export_inputs(manifest, names, frames, out):
    """One deterministic conditioning fixture, matching upstream soma/kernels.py."""
    selected = frames[:4] + frames[-4:]
    lookup = {n: i for i, n in enumerate(names)}
    def heading(pose):
        across = pose[lookup["RightLeg"]].translation - pose[lookup["LeftLeg"]].translation
        across.y = 0
        forward = Vector((0, 1, 0)).cross(across.normalized())
        return math.atan2(forward.x, forward.z)
    last = selected[3][0].translation
    # Upstream default guide heading is +X.
    alignment = Matrix.Translation(Vector((last.x, 0, last.z))) @ Matrix.Rotation(heading(selected[3]) - math.pi / 2, 4, "Y")
    aligned = [[alignment.inverted() @ m for m in pose] for pose in selected]
    inputs = {s["name"]: np.zeros(s["shape"], dtype=s["dtype"]) for s in manifest["tensor_specs"] if s["mode"] == "input"}
    for f, pose in enumerate(aligned):
        nxt = aligned[f if f in (3, 7) else f + 1]
        p, np_ = pose[0].translation, nxt[0].translation
        h, nh = heading(pose), heading(nxt)
        inputs["global_root_values"][0, f] = [*p, math.cos(h), math.sin(h)]
        inputs["local_root_values"][0, f] = [math.atan2(math.sin(nh-h), math.cos(nh-h))*30, (np_.x-p.x)*30, (np_.z-p.z)*30, p.y]
        for j, name in enumerate(manifest["joint_names"]):
            m = pose[lookup[name]]
            if j:
                inputs["local_poses"][0, f, (j-1)*3:j*3] = m.translation - Vector((p.x, 0, p.z))
            inputs["local_poses"][0, f, 87+j*6:87+(j+1)*6] = [*m.to_3x3().col[0], *m.to_3x3().col[1]]
    for key in ("has_global_root_values", "has_local_root_values", "has_local_poses"):
        inputs[key].fill(1)
    inputs["allowed_pred_num_tokens"].fill(1)
    inputs["has_local_root_values"][0, [3, 7]] = 0
    out.mkdir(parents=True, exist_ok=True)
    for name, values in inputs.items():
        assert np.isfinite(values).all()
        values.tofile(out / f"{name}.bin")
    write_json(out / "fixture.json", dict(alignment_model_from_aligned=matrix_json(alignment),
        input_shapes={k: list(v.shape) for k, v in inputs.items()}, frames=[0,1,2,3,len(frames)-4,len(frames)-3,len(frames)-2,len(frames)-1]))


def prepare_character(repo, bundle):
    sys.path.insert(0, str(repo / "temp/blender_mmd_tools"))
    import mmd_tools
    mmd_tools.register()
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    bpy.ops.mmd_tools.import_model(filepath=str(bundle / "character/source/Black.pmx"),
        types={"MESH", "ARMATURE", "DISPLAY"}, scale=.08, rename_bones=False,
        clean_model=False, remove_doubles=False)
    arm = next(o for o in bpy.context.scene.objects if o.type == "ARMATURE")
    # Replace leg control, retaining deformation grants and sleeve helper IK.
    replaced = {side + suffix for side in ("左", "右") for suffix in ("ひざ", "足首", "つま先")}
    for bone in arm.pose.bones:
        if bone.name in replaced or "キューブ" in bone.name or bone.name.startswith("腰キャンセル"):
            for constraint in list(bone.constraints):
                bone.constraints.remove(constraint)
        bone.rotation_mode = "QUATERNION"
        bone.matrix_basis = Matrix.Identity(4)
    arm.animation_data_clear()
    meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
    # Hitagi's current USD importer binds materials per mesh, not GeomSubset.
    for mesh in meshes:
        bpy.ops.object.select_all(action="DESELECT")
        mesh.select_set(True);bpy.context.view_layer.objects.active=mesh
        bpy.ops.object.mode_set(mode="EDIT")
        bpy.ops.mesh.select_all(action="SELECT")
        bpy.ops.mesh.separate(type="MATERIAL")
        bpy.ops.object.mode_set(mode="OBJECT")
    meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
    bpy.context.view_layer.update()
    return arm, meshes


def decode_prediction(path, fixture, manifest, names, parents, rest):
    data = {item["name"]: item["values"] for item in json.loads(path.read_text())}
    tokens = int(data["pred_num_tokens"][0])
    assert 6 <= tokens <= 16
    raw = np.asarray(data["pred_global_motions"], dtype=np.float32).reshape(64, 366)
    assert np.isfinite(raw).all()
    alignment = Matrix(json.loads(fixture.read_text())["alignment_model_from_aligned"])
    model_lookup = {name: i for i,name in enumerate(manifest["joint_names"])}
    frames = []
    for row in raw[3:tokens*4-3]:
        pose = []
        for i,name in enumerate(names):
            parent = parents[i]
            if name in model_lookup:
                j=model_lookup[name]; six=row[92+j*6:92+(j+1)*6]
                x=Vector(six[:3]).normalized(); z=x.cross(Vector(six[3:])).normalized(); y=z.cross(x)
                assert z.length > .99
                rotation=alignment.to_quaternion() @ Matrix((x,y,z)).transposed().to_quaternion()
                p=alignment @ Vector(row[:3]) if parent < 0 else pose[parent] @ rest[i].translation
                pose.append(Matrix.LocRotScale(p,rotation,Vector((1,1,1))))
            else:
                pose.append(pose[parent] @ rest[i] if parent >= 0 else alignment @ rest[i])
        frames.append(pose)
    return frames, np.clip(raw[3:tokens*4-3,-4:],0,1)


def foot_landmarks(arm, meshes):
    result={}
    for side in ("左","右"):
        foot=arm.data.bones[side+"足首"]
        vertices=[]
        for mesh in meshes:
            groups={g.index for g in mesh.vertex_groups if g.name in (side+"足首",side+"足首D",side+"つま先",side+"足先EX")}
            for v in mesh.data.vertices:
                if sum(g.weight for g in v.groups if g.group in groups) > .5:
                    vertices.append(arm.matrix_world.inverted() @ mesh.matrix_world @ v.co)
        assert vertices, "No shoe vertices"
        floor=min(v.z for v in vertices)
        sole=[v for v in vertices if v.z < floor+.012]
        # Native PMX import faces -Y. Keep the actual shoe-bottom height.
        heel=max(sole,key=lambda v:v.y).copy(); toe=min(sole,key=lambda v:v.y).copy()
        heel.z=floor;toe.z=floor
        result[side]=dict(heel=list(foot.matrix_local.inverted() @ heel),
                          toe=list(foot.matrix_local.inverted() @ toe), floor=floor,
                          selection="skin weight > 0.5; bottom 12 mm Y extrema projected onto lowest sole plane; review required")
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--clip", default="locomotion/bs_1d_linear/Neutral_walk_forward_002__A057.bvh")
    parser.add_argument("--max-frames", type=int, default=150)
    parser.add_argument("--prediction", type=Path)
    args = parser.parse_args(sys.argv[sys.argv.index("--")+1:] if "--" in sys.argv else [])
    repo = args.repo.resolve(); bundle = repo / "assets/motionbricks"
    out = bundle / "prepared"; out.mkdir(parents=True, exist_ok=True)
    logs = repo / "temp/motionbricks"; logs.mkdir(parents=True, exist_ok=True)
    clip = bundle / "upstream/assets/motions/soma" / args.clip
    manifest, names, parents, rest, bind, frames = load_source(bundle, clip)
    frames = frames[:args.max_frames]
    assert len(frames) >= 8
    lookup = {n: i for i, n in enumerate(names)}
    write_json(out / "soma_skeleton.json", dict(coordinates="right-handed Y-up metres; column-vector matrices",
        joint_names=names, parents=parents, rest_local=[matrix_json(m) for m in rest],
        bind_world=[matrix_json(m) for m in bind], model_to_skeleton=[lookup[n] for n in manifest["joint_names"]]))
    np.savez(out / "source_motion.npz", world=np.array([[matrix_json(m) for m in p] for p in frames], dtype=np.float32), fps=30)
    # Prediction must use the alignment from the inputs actually sent to TensorRT.
    # Do not overwrite that fixture while importing a generated result.
    if not args.prediction:
        export_inputs(manifest, names, frames, out / "inference_input")
    contacts=None
    if args.prediction:
        frames, contacts=decode_prediction(args.prediction, out / "inference_input/fixture.json", manifest,names,parents,rest)
    arm, meshes = prepare_character(repo, bundle)
    landmarks=foot_landmarks(arm,meshes)
    controls={}; anchors={}; weights={side:0.0 for side in ("左","右")}
    for side in ("左","右"):
        control=bpy.data.objects.new(side+"FootContact",None)
        bpy.context.scene.collection.objects.link(control)
        control.rotation_mode="QUATERNION"
        ik=arm.pose.bones[side+"ひざ"].constraints.new("IK")
        ik.name="MotionBricksFootContact";ik.target=control;ik.chain_count=2;ik.use_stretch=False;ik.influence=0
        orient=arm.pose.bones[side+"足首"].constraints.new("COPY_ROTATION")
        orient.target=control;orient.influence=0
        controls[side]=(control,ik,orient)
    target = {b.name: b for b in arm.pose.bones}
    mapping = {"腰":"Hips", "上半身":"Spine1", "上半身2":"Chest", "首":"Neck1", "頭":"Head"}
    for jp, side in [("左", "Left"), ("右", "Right")]:
        for dst, src in [("肩","Shoulder"),("腕","Arm"),("ひじ","ForeArm"),("手首","Hand"),
                         ("足","Leg"),("ひざ","Shin"),("足首","Foot"),("つま先","ToeBase")]:
            mapping[jp+dst] = side+src
    assert all(n in target for n in mapping)
    target_rest = {b.name: b.bone.matrix_local.copy() for b in arm.pose.bones}
    # Align each mapped limb's outgoing segment to the source bind direction.
    next_source = {"Shoulder":"Arm", "Arm":"ForeArm", "ForeArm":"Hand", "Leg":"Shin", "Shin":"Foot", "Foot":"ToeBase"}
    next_target = {"肩":"腕", "腕":"ひじ", "ひじ":"手首", "足":"ひざ", "ひざ":"足首", "足首":"つま先",
                   "足D":"ひざD", "ひざD":"足首D", "足首D":"足先EX"}
    corrections = {}
    for name, source_name in mapping.items():
        source_bind = TO_BLENDER @ bind[lookup[source_name]]
        rotation = target_rest[name].to_quaternion()
        for jp, side in [("左", "Left"), ("右", "Right")]:
            suffix = name[len(jp):] if name.startswith(jp) else ""
            src_suffix = source_name[len(side):] if source_name.startswith(side) else ""
            if suffix in next_target and src_suffix in next_source:
                td = target_rest[jp+next_target[suffix]].translation - target_rest[name].translation
                sd = TO_BLENDER.to_3x3() @ (bind[lookup[side+next_source[src_suffix]]].translation-bind[lookup[source_name]].translation)
                rotation = td.rotation_difference(sd) @ rotation
        corrections[name] = source_bind.to_quaternion().inverted() @ rotation
    target_height = (target_rest["左足"].translation.z + target_rest["右足"].translation.z) / 2
    source_height = (bind[lookup["LeftLeg"]].translation.y + bind[lookup["RightLeg"]].translation.y) / 2
    root_scale = target_height / source_height
    write_json(out / "retarget_calibration.json", dict(status="offline_baseline_with_foot_ik",
        source_pmx_sha256=sha256(bundle / "character/source/Black.pmx"), pmx_import_scale=.08,
        coordinates="Blender Z-up metres; quaternions WXYZ", root_scale=root_scale,
        target_bind_world={n: matrix_json(m) for n,m in target_rest.items()},
        mappings=[dict(target=n, source=s, source_to_target_rotation=list(corrections[n])) for n,s in mapping.items()],
        foot_landmarks=landmarks,
        native_constraints="leg IK replaced; sleeve helper IK and D deformation grants retained",
        limitations=["automatic shoe landmarks require review", "cloth/hair physics not evaluated", "full body collision solving not implemented"]))
    scene=bpy.context.scene; scene.render.fps=30; scene.frame_start=1; scene.frame_end=len(frames)
    ordered=sorted(arm.pose.bones,key=lambda b: len(b.parent_recursive))
    first_root=frames[0][0].translation.copy()
    measurements=[]
    for f, world in enumerate(frames,1):
        scene.frame_set(f)
        for control,ik,orient in controls.values(): ik.influence=0;orient.influence=0
        for bone in ordered:
            if bone.name not in mapping:
                bone.matrix_basis=Matrix.Identity(4)
                continue
            n=bone.name
            q=(TO_BLENDER @ world[lookup[mapping[n]]]).to_quaternion() @ corrections[n]
            # Translation follows the target rest hierarchy; only pelvis owns locomotion.
            local_rest = (target_rest[bone.parent.name].inverted() @ target_rest[n]) if bone.parent else target_rest[n]
            p = (bone.parent.matrix @ local_rest).translation if bone.parent else local_rest.translation
            if n=="腰":
                d=world[0].translation-first_root
                d.y=world[0].translation.y-bind[0].translation.y
                p=target_rest[n].translation + TO_BLENDER.to_3x3() @ (d*root_scale)
            m=Matrix.LocRotScale(p,q,Vector((1,1,1)))
            bone.matrix=m
            bpy.context.view_layer.update()
            bone.keyframe_insert("location",frame=f)
            bone.keyframe_insert("rotation_quaternion",frame=f)
        for side_index,side in enumerate(("左","右")):
            bone=arm.pose.bones[side+"足首"]
            m=bone.matrix.copy(); p=m.translation.copy();q=m.to_quaternion()
            heel=m @ Vector(landmarks[side]["heel"]);toe=m @ Vector(landmarks[side]["toe"])
            # For fixed clips this is a geometric heuristic, not ground truth.
            confidence=float(max(contacts[f-1,side_index*2:side_index*2+2])) if contacts is not None else float(min(heel.z,toe.z)<.025)
            planted=confidence > (.35 if side in anchors else .65)
            if planted and side not in anchors:
                point=heel if heel.z<toe.z else toe
                local=Vector(landmarks[side]["heel" if heel.z<toe.z else "toe"])
                anchors[side]=(Vector((point.x,point.y,0)),local)
            if not planted: anchors.pop(side,None)
            weights[side]+=max(-.25,min(.25,(1.0 if planted else 0.0)-weights[side]))
            desired=p.copy()
            if side in anchors:
                anchor,local=anchors[side];desired=anchor-q @ local
            desired.z+=max(0,-min((desired+q @ Vector(landmarks[side]["heel"])).z,
                                  (desired+q @ Vector(landmarks[side]["toe"])).z))
            control,ik,orient=controls[side]
            control.location=desired;control.rotation_quaternion=q
            ik.influence=max(weights[side],float(min(heel.z,toe.z)<0));orient.influence=ik.influence
            control.keyframe_insert("location",frame=f);control.keyframe_insert("rotation_quaternion",frame=f)
            ik.keyframe_insert("influence",frame=f);orient.keyframe_insert("influence",frame=f)
        bpy.context.view_layer.update()
        measurements.append(dict(frame=f,feet={side:dict(
            heel=list(arm.pose.bones[side+"足首"].matrix @ Vector(landmarks[side]["heel"])),
            toe=list(arm.pose.bones[side+"足首"].matrix @ Vector(landmarks[side]["toe"])),
            weight=weights[side]) for side in ("左","右")}))
    write_json(out / "contact_measurements.json", measurements)
    scene.frame_set(1); bpy.context.view_layer.update()
    # Simplify MMD material nodes to portable PreviewSurface inputs without touching images.
    original_model=json.loads((repo / "temp/motion-sour/model.json").read_text(encoding="utf-8"))
    original_materials={m["name"]: m for m in original_model["materials"]}
    for mat in bpy.data.materials:
        if not mat.use_nodes:
            continue
        original=original_materials.get(mat.name)
        if original is None:
            continue
        image=None
        if original["textureIndex"] >= 0:
            image_path=bundle / "character/source" / original_model["textures"][original["textureIndex"]].replace("\\","/")
            image=bpy.data.images.load(str(image_path),check_existing=True)
        mat.node_tree.nodes.clear()
        output=mat.node_tree.nodes.new("ShaderNodeOutputMaterial")
        bsdf=mat.node_tree.nodes.new("ShaderNodeBsdfPrincipled")
        bsdf.inputs["Base Color"].default_value=original["diffuse"]
        bsdf.inputs["Alpha"].default_value=original["diffuse"][3]
        bsdf.inputs["Roughness"].default_value=.8
        mat.node_tree.links.new(bsdf.outputs["BSDF"],output.inputs["Surface"])
        if image:
            tex=mat.node_tree.nodes.new("ShaderNodeTexImage");tex.image=image
            mat.node_tree.nodes.active=tex
            mat.node_tree.links.new(tex.outputs["Color"],bsdf.inputs["Base Color"])
            if original["diffuse"][3] > 0:
                mat.node_tree.links.new(tex.outputs["Alpha"],bsdf.inputs["Alpha"])
    bpy.ops.object.select_all(action="DESELECT")
    arm.select_set(True)
    for mesh in meshes: mesh.select_set(True)
    bpy.context.view_layer.objects.active=arm
    label="sour_generated" if args.prediction else "sour_walk"
    bpy.ops.wm.usd_export(filepath=str(out / (label+".usdc")), selected_objects_only=True,
        export_animation=True, export_armatures=True, only_deform_bones=False,
        export_shapekeys=False, triangulate_meshes=True,
        export_materials=True, export_textures_mode="NEW", convert_orientation=True,
        export_global_up_selection="Y", export_global_forward_selection="NEGATIVE_Z")
    # Keep an editable Blender source and deterministic local preview evidence.
    scene.render.engine="BLENDER_WORKBENCH"
    scene.render.resolution_x=800;scene.render.resolution_y=800;scene.render.resolution_percentage=100
    scene.display.shading.light="STUDIO";scene.display.shading.color_type="TEXTURE"
    scene.display.shading.show_shadows=True;scene.display.shading.show_cavity=True
    scene.world.color=(.08,.08,.08)
    camera=bpy.data.objects.new("MotionBricksPreview",bpy.data.cameras.new("MotionBricksPreview"))
    scene.collection.objects.link(camera);scene.camera=camera
    camera.data.type="ORTHO";camera.data.ortho_scale=2.0
    for frame in [1, min(30,len(frames)),min(60,len(frames))]:
        scene.frame_set(frame);bpy.context.view_layer.update()
        center=arm.matrix_world @ arm.pose.bones["腰"].matrix.translation
        camera.location=center+Vector((2,-5,.5))
        camera.rotation_euler=(center-camera.location).to_track_quat("-Z","Y").to_euler()
        scene.render.filepath=str(logs / f"blender-{label}-{frame:03}.png")
        bpy.ops.render.render(write_still=True)
    scene.frame_set(1)
    bpy.ops.wm.save_as_mainfile(filepath=str(out / (label+".blend")))
    write_json(out / "preparation.json",dict(blender=bpy.app.version_string,source_clip=args.clip,
        frames=len(frames),fps=30,source_joints=len(names),target_joints=len(target),
        usd=label+".usdc",calibration="retarget_calibration.json",predicted=args.prediction is not None))
    print("MOTIONBRICKS_PREPARATION_OK",len(frames))


if __name__=="__main__":
    main()
