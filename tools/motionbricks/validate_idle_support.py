"""Check support handoff and steady idle from --test-long-idle idle.csv.
Columns: frame,looping,loop_frame,side; source root/knee/ankle;
render hip/knee/ankle; heel/toe anchors; heel/toe weights (XYZ triples).
The stance tolerance is specific to the calibrated Miku/SOMA pair.
"""
import csv, math, pathlib, sys
rows = [list(map(float, r)) for r in csv.reader(pathlib.Path(sys.argv[1]).open())]
assert rows and all(len(r) == 30 for r in rows)
references = {}
max_drift = max_stance_error = max_step = 0.0
comparisons = 0
previous = {}
frames = {}
for r in rows:
    frame, looping, loop_frame, side = r[:4]
    if not looping:
        previous.pop(side, None)
        continue
    if side in previous and frame == previous[side][0] + 1 and loop_frame >= 2:
        max_step = max(max_step, math.dist(r[19:22], previous[side][19:22]))
    previous[side] = r
    if loop_frame < 30:
        continue
    key = (side, *[round(v, 6) for v in r[4:13]])
    if key in references:
        a = references[key]
        for start in (16, 19, 22, 25):
            max_drift = max(max_drift, math.dist(r[start:start+3], a[start:start+3]))
        comparisons += 1
    else:
        references[key] = r
    frames.setdefault(frame, {})[side] = r
for pair in frames.values():
    if len(pair) != 2:
        continue
    l, r = pair[0], pair[1]
    dx, dz = r[13]-l[13], r[15]-l[15]
    n = math.hypot(dx, dz)
    forward = (dz/n, -dx/n)
    source = ((r[10]-l[10])*forward[0] + (r[12]-l[12])*forward[1])*1.00829573
    rendered = (r[19]-l[19])*forward[0] + (r[21]-l[21])*forward[1]
    max_stance_error = max(max_stance_error, abs(rendered-source))
print(f'IDLE_SUPPORT comparisons={comparisons} drift_m={max_drift:.8f} stance_error_m={max_stance_error:.8f} max_ankle_step_m={max_step:.8f}')
assert comparisons > 1000, 'Insufficient repeated idle poses'
assert max_drift < .001, 'Support or IK drift with identical source pose'
assert max_stance_error < .06, 'Stale foot anchors changed the intended stance'
assert max_step < .03, 'Idle support handoff moved an ankle more than 3 cm per rendered frame'
