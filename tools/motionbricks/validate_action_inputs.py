"""Validate captured model target windows against the authored clip, allowing one rigid placement."""
import json,math,sys
from pathlib import Path
root=Path(__file__).resolve().parents[2]
lines=(root/'assets/motionbricks/prepared/runtime.txt').read_text().splitlines()
mapping={int(line.split()[1]):i for i,line in enumerate(lines[1:78]) if int(line.split()[1])>=0}
source=iter((root/'assets/motionbricks/prepared/actions.txt').read_text().splitlines());next(source);clips=[]
for _ in range(5):
 n=int(next(source).split()[1]);frames=[]
 for f in range(n):
  next(source);frames.append([list(map(float,next(source).split())) for j in range(77)])
 clips.append(frames)
def mul(a,b):
 x,y,z,w=a;X,Y,Z,W=b
 return [w*X+x*W+y*Z-z*Y,w*Y-x*Z+y*W+z*X,w*Z+x*Y-y*X+z*W,w*W-x*X-y*Y-z*Z]
def inv(q):return [-q[0],-q[1],-q[2],q[3]]
def rot(q,p):return mul(mul(q,[*p,0]),inv(q))[:3]
max_position=max_rotation=0;count=0
for line in Path(sys.argv[1]).read_text().splitlines():
 d=json.loads(line)
 if d['ability']<0:continue
 count+=1;first=d['reference_first_frame'];clip=clips[d['reference_clip']]
 expected=clip[first][mapping[0]];actual=d['guide_world_model_joints'][120]
 rotation=mul(actual[3:],inv(expected[3:]));shift=[a-b for a,b in zip(actual[:3],rot(rotation,expected[:3]))]
 assert d['pose_mask']==[1]*8 and d['root_mask']==[1]*8
 for f in range(4):
  for j in range(30):
   e=clip[first+f][mapping[j]];a=d['guide_world_model_joints'][(4+f)*30+j]
   position=[v+t for v,t in zip(rot(rotation,e[:3]),shift)]
   max_position=max(max_position,math.dist(position,a[:3]))
   q=mul(rotation,e[3:]);max_rotation=max(max_rotation,1-abs(sum(v*w for v,w in zip(q,a[3:]))))
assert count>=1 and max_position<.0001 and max_rotation<.0001,(count,max_position,max_rotation)
print('MODEL_TARGET_WINDOWS_OK',count,'max_position_m=',max_position,'quaternion_error=',max_rotation)
