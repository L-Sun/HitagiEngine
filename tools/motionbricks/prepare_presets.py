"""Blender: export official SOMA Smart Primitive targets and a local test arena.

Preserves authored hierarchy, sample times, target counts, timing ranges, early
transition frames and IK flags. Original upstream assets are never rewritten.
"""
import json
import math
import sys
from pathlib import Path
from collections import defaultdict
from mathutils import Matrix, Quaternion, Vector
from pxr import Usd, UsdGeom, UsdShade, Gf, Sdf

sys.path.insert(0, str(Path(__file__).parent))
from blender_prepare import load_source, TO_BLENDER, TO_MODEL

repo = Path(__file__).resolve().parents[2]
bundle = repo / 'assets/motionbricks'
out = bundle / 'prepared'
directory = bundle / 'upstream/assets/smart_primitives/soma'
names = ['locomotion_bs1_linear', 'locomotion_bs2_directional',
         'locomotion_bs2_directional_crouch', 'locomotion_style_variations_bs1_linear',
         'avoidance', 'jump_0_5m', 'jump_0_75m', 'sit']
documents = []
requests = defaultdict(list)

def transform(value):
    x,y,z,w = value['rotation_xyzw']
    return Matrix.LocRotScale(Vector(value['translation']), Quaternion((w,x,y,z)), Vector((1,1,1)))

def visit(node, parent, targets, boxes):
    world = parent @ transform(node['local_transform'])
    for component in node.get('components', []):
        if component['type'] == 'target_pose':
            target = dict(component, node_id=node['id'], name=node['name'], matrix=world)
            target['path'] = (directory / component['clip']).resolve()
            # sample_time_seconds is kept synchronized with frame_index by upstream.
            target['offset'] = len(requests[target['path']])
            requests[target['path']].extend(component['sample_time_seconds'] + f/30 for f in range(4))
            targets.append(target)
        elif component['type'] == 'interaction_box':
            boxes.append((world, component))
    for child in node.get('children', []):
        visit(child, world, targets, boxes)

for name in names:
    document = json.loads((directory / (name + '.smart_primitive.json')).read_text())
    targets, boxes = [], []
    visit(document['root'], Matrix.Identity(4), targets, boxes)
    documents.append((document, targets, boxes))

sampled = {}
for path, times in requests.items():
    sampled[path] = load_source(bundle, path, times)[-1]
    print('SAMPLED', path.name, len(times), flush=True)

def pose(matrix):
    p=matrix.translation; q=matrix.to_quaternion().normalized()
    return ' '.join(f'{v:.9g}' for v in (*p,q.x,q.y,q.z,q.w))

lines=['HMP1 8']
manifest=[]
for index,(document,targets,boxes) in enumerate(documents):
    sequences = document['sequences']
    lines.append(f'{json.dumps(names[index])} {len(targets)} {len(sequences)}')
    for target in targets:
        lines.append(f'{json.dumps(target["name"])} {target["num_poses_to_use"]} {target["transition_early_by_frames"]} {target["min_tokens"]} {target["max_tokens"]} {target["ik_pin_mask"]}')
        frames=sampled[target['path']][target['offset']:target['offset']+4]
        shift=Matrix.Identity(4)
        if target['hips_constrained']:
            p=frames[0][0].translation
            shift.translation=Vector((-p.x,0,-p.z))
        placement=TO_MODEL @ target['matrix'] @ TO_BLENDER @ shift
        for frame in frames:
            lines.extend(pose(placement @ joint) for joint in frame)
        manifest.append({'preset':names[index], 'node':target['name'], 'clip':str(target['path'].relative_to(bundle)),
                         'time':target['sample_time_seconds'], 'poses':target['num_poses_to_use'], 'early':target['transition_early_by_frames'],
                         'tokens':[target['min_tokens'],target['max_tokens']], 'ik_pins':target['ik_pin_mask']})
    for sequence in sequences:
        indices=[next(i for i,t in enumerate(targets) if t['node_id']==node) for node in sequence['node_ids']]
        lines.append(f'{json.dumps(sequence["name"])} {len(indices)} ' + ' '.join(map(str,indices)))
# Idle/crouch/seated cycles are runtime states, as in upstream IdleState/SittingBehavior.
loop_paths=['locomotion/idle/idle_loop_001__A268.bvh','locomotion/idle/crouch_idle_001__A529.bvh','sitting/sit_on_chair_loop.bvh']
lines.append('3')
for path in loop_paths:
    frames=load_source(bundle,bundle/'upstream/assets/motions/soma'/path)[-1]
    lines.append(str(len(frames)))
    for frame in frames: lines.extend(pose(j) for j in frame)
(out/'presets.txt').write_text('\n'.join(lines)+'\n')
(out/'presets_manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')

# Separate arena layer, leaving the static character calibration intact.
scale=float((out/'runtime.txt').read_text().splitlines()[0].split()[-1])
stage=Usd.Stage.CreateNew(str(out/'sour_presets.usda'))
stage.GetRootLayer().subLayerPaths=['sour_runtime.usdc']
UsdGeom.SetStageUpAxis(stage,'Y'); UsdGeom.SetStageMetersPerUnit(stage,1)
material=UsdShade.Material.Define(stage,'/PresetMaterial')
shader=UsdShade.Shader.Define(stage,'/PresetMaterial/Shader');shader.CreateIdAttr('UsdPreviewSurface')
shader.CreateInput('diffuseColor',Sdf.ValueTypeNames.Color3f).Set(Gf.Vec3f(.16,.32,.38))
shader.CreateInput('roughness',Sdf.ValueTypeNames.Float).Set(.85)
material.CreateSurfaceOutput().ConnectToSource(shader.ConnectableAPI(),'surface')
for index,(_,_,boxes) in enumerate(documents):
    if index<4: continue
    for box_index,(world,box) in enumerate(boxes):
        placement=TO_MODEL@world
        center=placement.translation*scale+Vector(((index-4)*4-6,0,0))
        dims=Vector((box['width'],box['depth'],box['height']))*.5
        verts=[]
        for z in (-1,1):
            for y in (-1,1):
                for x in (-1,1):
                    p=placement.to_3x3()@Vector((dims.x*x,dims.y*y,dims.z*z))*scale+center
                    verts.append(Gf.Vec3f(*p))
        mesh=UsdGeom.Mesh.Define(stage,f'/PresetObjects/Object{index}_{box_index}')
        mesh.CreatePointsAttr(verts);mesh.CreateFaceVertexCountsAttr([4]*6)
        mesh.CreateFaceVertexIndicesAttr([0,2,3,1,4,5,7,6,0,1,5,4,2,6,7,3,0,4,6,2,1,3,7,5])
        mesh.CreateSubdivisionSchemeAttr('none');mesh.CreateDoubleSidedAttr(True)
        UsdShade.MaterialBindingAPI.Apply(mesh.GetPrim()).Bind(material)
stage.GetRootLayer().Save()
print('PRESETS_OK',len(documents),len(manifest),flush=True)
