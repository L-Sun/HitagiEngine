"""Check the visible neutral-walk regression from a 30-second side-walk trace.

The reach metrics are ankle position relative to the pelvis, not a physical
contact measurement. Use only the neutral forward Walk variant; authored
injured styles intentionally have asymmetric gait.
"""
import argparse, csv, json, math, statistics
from pathlib import Path


def cycle_metrics(values):
    assert all(math.isfinite(x) for x in values)
    peaks=[i for i in range(10,len(values)-10)
           if values[i]>.05 and values[i]==max(values[i-10:i+11])]
    peaks=[p for i,p in enumerate(peaks) if i==0 or p-peaks[i-1]>15]
    cycles=[values[a:b+1] for a,b in zip(peaks,peaks[1:]) if b-a>15]
    assert len(cycles)>=15, 'Need at least 15 complete walking cycles'
    return dict(cycles=len(cycles),forward=statistics.mean(max(c) for c in cycles),
                backward=statistics.mean(-min(c) for c in cycles))


def validate(directory):
    rows=list(csv.DictReader((directory/'gait.csv').open()))
    assert len(rows)>=3800, 'Use --side-walk --gait-probe phasefix-long'
    result={}
    for stage in ['source','render']:
        sides=[cycle_metrics([float(r[stage+'_root_z'])-float(r[stage+'_foot_z'])
                             for r in rows if int(r['frame'])>120 and int(r['side'])==s]) for s in range(2)]
        assert abs(sides[0]['cycles']-sides[1]['cycles'])<=1
        for key in ['forward','backward']:
            assert abs(sides[0][key]-sides[1][key])<.025,(stage,key,sides)
        for s in sides:
            assert .4<s['forward']+s['backward']<1.2, 'Lost normal walking range'
        result[stage]=sides
    (directory/'gait-validation.json').write_text(json.dumps(result,indent=2)+'\n')
    print('GAIT_BALANCE_OK',json.dumps(result))


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory',type=Path)
    validate(parser.parse_args().directory)
