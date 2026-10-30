"""Validate real --test model.jsonl tensors against the Smart Locomotion contract.

Run after motionbricks-demo --test (or --test-walk/--test-variable-rate).
Uses only the Python standard library; does not substitute a mock model.
"""
import argparse
import json
import math
from pathlib import Path


def validate(path):
    counts = set()
    plans = 0
    moving = 0
    previous = None
    for line in path.read_text().splitlines():
        data = json.loads(line)
        if not data["predicted_tokens"]:
            continue  # reset has no inference yet
        plans += 1
        roots = [data["roots"][i:i + 5] for i in range(0, 40, 5)]
        velocity = data["velocities"][16:20]
        count=sum(data['pose_mask'])-4
        assert 1 <= count <= 4
        assert data['root_mask'] == data['pose_mask'] == [1]*(4+count)+[0]*(4-count)
        assert data['velocity_mask'] == [1,1,1,0]+[1]*(count-1)+[0]*(5-count)
        token_count = data["predicted_tokens"]
        assert 6 <= token_count <= 16 and data['allowed_token_mask'][token_count-6]
        counts.add(token_count)
        frames = 4 * token_count - 6
        assert len(data["raw_root_world"]) == len(data["corrected_root_world"]) == frames
        assert abs(data["target_time_seconds"] - (frames - 1) / 30) < 1e-6
        speed = math.hypot(velocity[1], velocity[2])
        assert math.isfinite(speed)
        moving += speed > 0.1
        # Equation (6), evaluated in the same aligned coordinates as the inputs.
        command_x,command_z=velocity[1],velocity[2]
        if 'command_velocity_world' in data:
            guides=data['guide_world_model_joints']
            right,left=guides[3*30+26],guides[3*30+22]
            h=math.atan2(right[2]-left[2],-(right[0]-left[0]))
            angle=math.pi/2-h
            vx,_,vz=data['command_velocity_world']
            command_x=math.cos(angle)*vx+math.sin(angle)*vz
            command_z=-math.sin(angle)*vx+math.cos(angle)*vz
        for axis, commanded in (() if data.get('action') else ((0, command_x), (2, command_z))):
            p0 = roots[3][axis]
            v0 = (p0 - roots[2][axis]) * 30
            target = p0 + commanded
            expected = target + (-commanded + v0 - 4 * commanded) * math.exp(-4)
            assert abs(roots[4][axis] - expected) < 2e-5, (plans, axis, roots[4][axis], expected)
        if speed > 0.1 and not data.get('action') and data.get('preset') in (0,3):
            heading = lambda r: math.atan2(r[4], r[3])
            wrap = lambda a: math.atan2(math.sin(a), math.cos(a))
            h0 = heading(roots[3])
            omega = wrap(h0 - heading(roots[2])) * 30
            command = math.atan2(command_x, command_z)
            error = -wrap(command - h0)
            expected = h0 - error + (error + omega + 4 * error) * math.exp(-4)
            assert abs(wrap(heading(roots[4]) - expected)) < 2e-5
        if data.get('phase_cycle',-1)>=0:
            period=data['phase_period']
            assert 0<=data['phase']<period
            assert abs(data['phase_adjustment'])<=.250001
            if previous and previous.get('phase_cycle')==data['phase_cycle'] and data['inference']==previous['inference']+1:
                advance=(data['phase_clock']-previous['phase_clock'])*period if 'phase_clock' in data else data['motion_step']-previous['motion_step']
                expected=(previous['phase']+advance+data['phase_adjustment'])%period
                difference=(data['phase']-expected+period/2)%period-period/2
                assert abs(difference)<2e-4,(data['inference'],difference)
        previous=data
        guides = data["guide_world_model_joints"]
        for actual, expected in ((data["corrected_root_world"][0], guides[3 * 30][:3]),
                                 (data["corrected_root_world"][-1], guides[4 * 30][:3])):
            assert math.dist(actual, expected) < 2e-5
    assert plans > 10 and moving > 5, "Use a moving control test, not an idle-only trace"
    assert len(counts) > 1, "This control sequence did not exercise learned timing changes"
    print(f"SMART_INPUT_OK plans={plans} moving={moving} chosen_tokens={sorted(counts)}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("path", nargs="?", type=Path, default=Path("temp/motionbricks/runtime/model.jsonl"))
    validate(parser.parse_args().path)
