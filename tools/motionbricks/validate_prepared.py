"""Run in Blender: validate exported joint animation and actual shoe geometry."""
import json
from pathlib import Path
import sys
import math
import re

import bpy
from mathutils import Matrix
from pxr import Usd, UsdGeom, UsdSkel

repo=Path(__file__).resolve().parents[2]
bundle=repo / "assets/motionbricks/prepared"
bpy.ops.wm.open_mainfile(filepath=str(bundle / "sour_generated.blend"))
arm=next(o for o in bpy.context.scene.objects if o.type=="ARMATURE")
stage=Usd.Stage.Open(str(bundle / "sour_generated.usdc"))
root=next(UsdSkel.Root(p) for p in stage.Traverse() if p.IsA(UsdSkel.Root))
skel=next(UsdSkel.Skeleton(p) for p in stage.Traverse() if p.IsA(UsdSkel.Skeleton))
cache=UsdSkel.Cache();cache.Populate(root,Usd.PrimDefaultPredicate)
query=cache.GetSkelQuery(skel)
names=[str(p).rsplit("/",1)[-1] for p in query.GetJointOrder()]
bone_names={re.sub(r"\W", "_", b.name): b.name for b in arm.pose.bones}
assert len(bone_names)==len(arm.pose.bones), "Ambiguous sanitized bone names"
assert len(names)==len(arm.data.bones)
assert stage.GetTimeCodesPerSecond()==30 and UsdGeom.GetStageMetersPerUnit(stage)==1
axis=Matrix.Rotation(-math.pi/2,4,"X")
position_error=0.0;rotation_error=0.0;shoe_min=1e9
frames=list(range(bpy.context.scene.frame_start,bpy.context.scene.frame_end+1))
for frame in frames:
    bpy.context.scene.frame_set(frame);bpy.context.view_layer.update()
    world=query.ComputeJointWorldTransforms(UsdGeom.XformCache(Usd.TimeCode(frame)))
    for name, matrix in zip(names,world):
        # USD joint tokens preserve Unicode names, replacing Blender punctuation.
        bone=arm.pose.bones.get(bone_names.get(name,""))
        assert bone is not None, name
        expected=axis @ arm.matrix_world @ bone.matrix
        actual=Matrix(matrix).transposed()
        position_error=max(position_error,(expected.translation-actual.translation).length)
        angle=expected.to_quaternion().rotation_difference(actual.to_quaternion()).angle
        rotation_error=max(rotation_error,min(angle,abs(2*math.pi-angle)))
    depsgraph=bpy.context.evaluated_depsgraph_get()
    for mesh in [o for o in bpy.context.scene.objects if o.type=="MESH"]:
        groups={g.index for g in mesh.vertex_groups if any(g.name==side+suffix for side in ("左","右") for suffix in ("足首","足首D","つま先","足先EX"))}
        indices=[v.index for v in mesh.data.vertices if sum(g.weight for g in v.groups if g.group in groups)>.5]
        if not indices: continue
        evaluated=mesh.evaluated_get(depsgraph)
        data=evaluated.to_mesh()
        try:
            shoe_min=min(shoe_min,min((evaluated.matrix_world @ data.vertices[i].co).z for i in indices))
        finally: evaluated.to_mesh_clear()
report=dict(frames=frames,joints=len(names),
    max_joint_position_error_m=position_error,max_joint_rotation_error_rad=rotation_error,
    minimum_actual_shoe_height_m=shoe_min,
    note="All exported frames: Blender/USD joint comparison and evaluated shoe vertices; not an all-motion collision guarantee")
(repo / "temp/motionbricks/asset-validation.json").write_text(json.dumps(report,indent=2)+"\n")
print(json.dumps(report))
assert position_error<1e-4, "USD export changed world joint positions"
assert rotation_error<.002, "USD export changed joint rotations"
assert shoe_min>=-.001, "Evaluated shoe vertices penetrate the floor by more than 1 mm"
