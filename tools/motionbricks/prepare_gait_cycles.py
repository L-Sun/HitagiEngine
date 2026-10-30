"""Blender: export phase conditioning banks for forward gait and six styles.

Keeps root displacement intact. Eight trailing samples allow phase-aligned
resampling of adjacent gait banks across the cycle end without interpolating
the last root back to the first root. No rendered animation is baked here.
"""
from pathlib import Path
import json, math, sys
import numpy as np
from mathutils import Vector, Quaternion

sys.path.insert(0, str(Path(__file__).parent))
from blender_prepare import load_source

root=Path(__file__).resolve().parents[2]
bundle=root/'assets/motionbricks'
manifest=json.loads((bundle/'prepared/presets_manifest.json').read_text())
targets=[x for x in manifest if x['preset'] in ('locomotion_bs1_linear','locomotion_style_variations_bs1_linear') and x['node']!='Idle']
lines=['HMG2 9']; reports=[]
for target,period_range in zip(targets,[(28,50),(16,40),(14,36)]+[(20,90)]*6):
    lo,hi=period_range
    data=load_source(bundle,bundle/target['clip'],[target['time']+f/30 for f in range(hi+9)])
    names,frames=data[1],data[-1]
    joints=[names.index(n) for n in ['LeftLeg','LeftUpLeg','LeftFoot','RightLeg','RightUpLeg','RightFoot'] if n in names]
    features=[]
    for pose in frames:
        across=pose[names.index('RightLeg')].translation-pose[names.index('LeftLeg')].translation
        forward=Vector((0,1,0)).cross(across)
        turn=Quaternion((0,1,0),-math.atan2(forward.x,forward.z))
        features.append([tuple(turn@(pose[j].translation-pose[0].translation)) for j in joints])
    features=np.array(features)
    def seam(period):
        return float(np.mean((features[:4]-features[period:period+4])**2)+np.mean((np.diff(features[:4],axis=0)-np.diff(features[period:period+4],axis=0))**2))
    period=min(range(lo,hi+1),key=seam)
    displacement=frames[period][0].translation-frames[0][0].translation
    speed=math.hypot(displacement.x,displacement.z)*30/period
    if speed<.05 or not math.isfinite(speed): raise ValueError(f'Invalid locomotion speed: {target["node"]}')
    origin=max(range(period),key=lambda f: features[f,2,2])
    lines.append(f'{period} {speed:.9g} {origin}')
    for pose in frames[:period+8]:
        for m in pose:
            q=m.to_quaternion().normalized()
            lines.append(' '.join(f'{v:.9g}' for v in (*m.translation,q.x,q.y,q.z,q.w)))
    reports.append(dict(name=target['node'],clip=target['clip'],start_seconds=target['time'],period_frames=period,speed_mps=speed,phase_origin=origin,seam_rms_m=math.sqrt(seam(period))))
out=bundle/'prepared'
(out/'gait_cycles.txt').write_text('\n'.join(lines)+'\n')
(out/'gait_cycles.json').write_text(json.dumps(reports,indent=2)+'\n')
print('GAIT_CYCLES_OK',reports,flush=True)
