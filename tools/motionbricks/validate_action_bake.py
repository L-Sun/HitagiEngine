import bpy,sys
from pathlib import Path
from mathutils import Vector,Quaternion
sys.path.insert(0,str(Path('tools/motionbricks').resolve()))
from blender_prepare import TO_MODEL
bpy.ops.wm.open_mainfile(filepath=str(Path('assets/motionbricks/actions/action-library.blend').resolve()))
a=bpy.data.objects['SOMA_Actions'];lines=iter(Path('assets/motionbricks/prepared/actions.txt').read_text().splitlines());next(lines)
error=0
for _ in range(5):
 h=next(lines).split();name=h[0].strip('"');n=int(h[1]);act=bpy.data.actions[name];a.animation_data.action=act;a.animation_data.action_slot=act.slots[0]
 for f in range(n):
  next(lines);expected=[list(map(float,next(lines).split())) for _ in range(77)]
  bpy.context.scene.frame_set(f);bpy.context.view_layer.update()
  for b,v in zip(a.pose.bones,expected):
   m=TO_MODEL@b.matrix
   error=max(error,(m.translation-Vector(v[:3])).length)
print('BLENDER_BAKE_MAX_ERROR',error)
if error>.001:raise RuntimeError('Bake does not match exported motion')
