"""Validate actual walk/run target speed ramps and camera-relative input traces."""
import argparse,csv,json,math
from pathlib import Path

def validate(directory):
    plans=[json.loads(line) for line in (directory/'model.jsonl').read_text().splitlines()]
    accelerations=[]; decelerations=[]; retreat=[]
    for a,b in zip(plans,plans[1:]):
        if not (0<=a.get('phase_cycle',-1)<3 and 0<=b.get('phase_cycle',-1)<3):continue
        dt=(b['motion_step']-a['motion_step'])/30
        if dt<=0:continue
        va,vb=a['command_velocity_world'],b['command_velocity_world']
        sa,sb=math.sqrt(sum(x*x for x in va)),math.sqrt(sum(x*x for x in vb))
        rate=(sb-sa)/dt
        assert -5.001<=rate<=4.001,(a['inference'],b['inference'],rate)
        if rate>.01:accelerations.append(rate)
        if rate<-.01:
            decelerations.append(rate)
            # Compare target distance relative to current root, in command direction.
            def ahead(p,v,s):
                guides=p['guide_world_model_joints']
                return sum((guides[120][i]-guides[90][i])*v[i]/s for i in [0,2])
            if sum(x*y for x,y in zip(va,vb))/(sa*sb)>.99:
                retreat.append(ahead(a,va,sa)-ahead(b,vb,sb))
    assert len(accelerations)>=5 and len(decelerations)>=5,'Need both sustained walk/run transitions'
    assert max(retreat)<.65,('Target teleported toward character',max(retreat))
    print(f'SPEED_TRANSITION_OK acceleration={max(accelerations):.4f} deceleration={min(decelerations):.4f} max_target_retreat_m={max(retreat):.4f}')
    if directory.name=='test-camera-movement':
        rows={int(r['frame']):r for r in csv.DictReader((directory/'trace.csv').open())}
        for frame,expected in [(120,(0,-1)),(660,(0,-1)),(780,(1,0)),(1020,(-1,0))]:
            r=rows[frame];assert math.dist((float(r['input_x']),float(r['input_z'])),expected)<1e-5,(frame,r)
        print('CAMERA_INPUT_OK front W; right-view D/S/W')

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('directory',type=Path)
    validate(p.parse_args().directory)
