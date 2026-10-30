"""Blender: retarget CC0 Quaternius actions, author forward jump variants, bake SOMA actions.
Source GLB is preserved. Output .blend has editable SOMA actions and stage markers.
"""
import bpy, math, json, sys
from pathlib import Path
from mathutils import Matrix, Vector, Quaternion
sys.path.insert(0,str(Path(__file__).parent))
from blender_prepare import load_source, TO_MODEL, TO_BLENDER
root=Path(__file__).resolve().parents[2];bundle=root/'assets/motionbricks';out=bundle/'actions';out.mkdir(exist_ok=True)
bpy.ops.wm.open_mainfile(filepath=str(out/'source/quaternius-import.blend'))
original=next(o for o in bpy.data.objects if o.type=='ARMATURE')
for track in list(original.animation_data.nla_tracks): original.animation_data.nla_tracks.remove(track)
source_actions={a.name:a for a in bpy.data.actions}
_,names,parents,rest,bind,idle=load_source(bundle,bundle/'upstream/assets/motions/soma/locomotion/idle/idle_loop_001__A268.bvh',[0])
lookup={n:i for i,n in enumerate(names)}
map_={'Hips':'pelvis','Spine1':'spine_01','Spine2':'spine_02','Chest':'spine_03','Neck1':'neck_01','Neck2':'neck_01','Head':'Head'}
for side,suffix in [('Left','l'),('Right','r')]:
 for a,b in [('Shoulder','clavicle'),('Arm','upperarm'),('ForeArm','lowerarm'),('Hand','hand'),('Leg','thigh'),('Shin','calf'),('Foot','foot'),('ToeBase','ball')]:map_[side+a]=b+'_'+suffix
 for finger in ['Thumb','Index','Middle','Ring','Pinky']:
  for j in range(1,4):map_[f'{side}Hand{finger}{j}']=f'{finger.lower()}_{j:02}_{suffix}'
corrections={n:(TO_MODEL@original.data.bones[b].matrix_local).to_quaternion().inverted()@bind[lookup[n]].to_quaternion() for n,b in map_.items() if n in lookup and b in original.data.bones}
scale=bind[0].translation.y/original.data.bones['pelvis'].head_local.z

def localize(world):return [world[parents[i]].inverted()@m if parents[i]>=0 else m.copy() for i,m in enumerate(world)]
def worldize(local):
 w=[]
 for i,m in enumerate(local):w.append(w[parents[i]]@m if parents[i]>=0 else m.copy())
 return w

def heading(w):
 v=w[lookup['RightLeg']].translation-w[lookup['LeftLeg']].translation;return math.atan2(v.z,-v.x)
def canonical(w):
 h=heading(w);p=w[0].translation
 a=Matrix.Translation(Vector((p.x,0,p.z)))@Matrix.Rotation(h,4,'Y')
 return [a.inverted()@m for m in w]
idle=localize(canonical(idle[0]))
cache={}
def sample(action,u):
 key=(action,round(u,7))
 if key in cache:return [m.copy() for m in cache[key]]
 a=source_actions[action];original.animation_data.action=a;original.animation_data.action_slot=a.slots[0]
 f=a.frame_range[0]+max(0,min(1,u))*(a.frame_range[1]-a.frame_range[0]);bpy.context.scene.frame_set(int(f),subframe=f-int(f));bpy.context.view_layer.update()
 w=[]
 for i,n in enumerate(names):
  l=rest[i].copy()
  if n in corrections:
   m=TO_MODEL@original.pose.bones[map_[n]].matrix;q=m.to_quaternion()@corrections[n]
   l=Matrix.LocRotScale(l.translation,w[parents[i]].to_quaternion().inverted()@q if parents[i]>=0 else q,Vector((1,1,1)))
   if i==0:l.translation=m.translation*scale
  w.append(w[parents[i]]@l if parents[i]>=0 else l)
 result=localize(w);cache[key]=result
 return [m.copy() for m in result]
def blend(a,b,t):
 t=max(0,min(1,t));t=t*t*(3-2*t)
 return [Matrix.LocRotScale(x.translation.lerp(y.translation,t),x.to_quaternion().slerp(y.to_quaternion(),t),Vector((1,1,1))) for x,y in zip(a,b)]
def smooth(x):x=max(0,min(1,x));return x*x*(3-2*x)

clips=[]
for kind,name,total,core,active,air in [(0,'Punch',30,24,(10,15),(-1,-1)),(1,'ForwardJump',48,41,(-1,-1),(6,32)),(2,'JumpAttack',48,41,(16,22),(6,32)),(3,'ForwardRoll',60,53,(-1,-1),(10,44)),(4,'Jab',30,24,(9,14),(-1,-1))]:
 frames=[]
 for f in range(total):
  t=f/30
  if kind in (0,4):p=sample('Punch_Jab' if kind==4 else 'Punch_Cross',min(1,max(0,(f-3)/21)))
  elif kind==3:p=sample('Roll',min(1,max(0,(f-4)/48)))
  else:
   start=sample('Jump_Start',min(1,f/6))
   floating=sample('Jump_Loop',max(0,min(1,(f-6)/60)))
   land=sample('Jump_Land',max(0,min(1,(f-30)/12)))
   p=blend(start,floating,(f-5)/3) if f<28 else blend(floating,land,(f-27)/7)
   if kind==2:
    strike=sample('Punch_Cross',max(0,min(1,(f-9)/19)))
    weight=smooth((f-7)/5)*(1-smooth((f-25)/7))
    mix=blend(p,strike,weight)
    for i,n in enumerate(names):
     if n.startswith(('Spine','Chest','Neck','Head','LeftArm','RightArm','LeftFore','RightFore','LeftShoulder','RightShoulder','LeftHand','RightHand')):p[i]=mix[i]
  p=blend(idle,p,f/3)
  p=blend(p,idle,(f-(core-4))/5)
  p[0].translation.x=0;p[0].translation.z=0
  w=worldize(p)
  body_indices=[lookup[n] for n in ['Head','Hips','Chest','LeftHand','RightHand','LeftLeg','RightLeg','LeftShin','RightShin','LeftFoot','RightFoot','LeftToeBase','RightToeBase']]
  floor=min(w[i].translation.y for i in body_indices if kind==3 or names[i] in ('LeftFoot','RightFoot','LeftToeBase','RightToeBase'))
  clearance=.045+(.075*smooth((f-5)/5)*(1-smooth((f-44)/5)) if kind==3 else 0)
  lift=0;travel=0
  if kind in (1,2):
   u=max(0,min(1,(f-6)/26));lift=4*.65*u*(1-u);travel=1.65*u
  elif kind==3:travel=2.0*smooth((f-4)/47)
  required_y=p[0].translation.y+clearance-floor
  # Airborne root follows the authored ballistic arc; tucked feet do not lift the pelvis again.
  p[0].translation.y=max(required_y,idle[0].translation.y+lift) if kind in (1,2) else required_y
  p[0].translation.z=travel
  if f>=core:
   # Four exact idle targets retain their own short-window temporal consistency.
   p=[m.copy() for m in idle];p[0].translation.z=travel
  contacts=0 if air[0]<=f<=air[1] and air[0]>=0 else 1
  frames.append((p,contacts))
 if kind in (1,2):
  # A clearance envelope anticipates landing compression, bounded to 2.4 m/s vertically.
  heights=[p[0].translation.y for p,_ in frames]
  for i,(p,_) in enumerate(frames):
   p[0].translation.y=max(y-.08*abs(i-j) for j,y in enumerate(heights))
 clips.append(dict(name=name,frames=frames,core=core,active=active,air=air))

# Editable SOMA rig and baked actions in Blender.
bpy.ops.object.select_all(action='DESELECT')
arm_data=bpy.data.armatures.new('SOMA_Actions');arm=bpy.data.objects.new('SOMA_Actions',arm_data);bpy.context.collection.objects.link(arm);arm.select_set(True);bpy.context.view_layer.objects.active=arm
bpy.ops.object.mode_set(mode='EDIT')
for i,n in enumerate(names):
 b=arm_data.edit_bones.new(n);b.matrix=TO_BLENDER@bind[i];b.length=.08
 if parents[i]>=0:b.parent=arm_data.edit_bones[names[parents[i]]]
bpy.ops.object.mode_set(mode='OBJECT');arm.show_in_front=True;arm.animation_data_create();bpy.context.scene.render.fps=30
lines=['HMA1 5'];meta=[]
for c in clips:
 a=bpy.data.actions.new(c['name']);arm.animation_data.action=a;a.use_fake_user=True
 lines.append(f'"{c["name"]}" {len(c["frames"])} {c["core"]} {c["active"][0]} {c["active"][1]} {c["air"][0]} {c["air"][1]}')
 for f,(p,contact) in enumerate(c['frames']):
  w=worldize(p);lines.append(' '.join([str(contact)]*4))
  for i,m in enumerate(w):
   q=m.to_quaternion().normalized();lines.append(' '.join(f'{v:.9g}' for v in (*m.translation,q.x,q.y,q.z,q.w)))
   b=arm.pose.bones[names[i]];b.rotation_mode='QUATERNION'
   # Compute basis explicitly: evaluated parent matrices may still belong to the previous frame.
   rest_local=b.bone.parent.matrix_local.inverted()@b.bone.matrix_local if b.parent else b.bone.matrix_local
   pose_local=w[parents[i]].inverted()@m if parents[i]>=0 else TO_BLENDER@m
   b.matrix_basis=rest_local.inverted()@pose_local
   b.keyframe_insert('location',frame=f);b.keyframe_insert('rotation_quaternion',frame=f)
  bpy.context.view_layer.update()
 for label,frame in [('core_end',c['core']),('active_start',c['active'][0]),('active_end',c['active'][1]),('takeoff',c['air'][0]),('land',c['air'][1])]:
  if frame>=0:a.pose_markers.new(label).frame=frame
 meta.append({k:v for k,v in c.items() if k!='frames'}|{'frames':len(c['frames']),'fps':30})
original.hide_set(True)
for o in bpy.data.objects:
 if o.type=='MESH':o.hide_set(True)
bpy.context.scene.frame_start=0;bpy.context.scene.frame_end=44
bpy.ops.wm.save_as_mainfile(filepath=str(out/'action-library.blend'))
(bundle/'prepared/actions.txt').write_text('\n'.join(lines)+'\n')
(out/'manifest.json').write_text(json.dumps(meta,indent=2))
print('ACTION_LIBRARY_OK',json.dumps(meta))
