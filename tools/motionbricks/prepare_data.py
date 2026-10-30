"""Collect a pinned, local MotionBricks/Sour-Miku evaluation data bundle.

Python standard library only. Run after fetching the selected upstream LFS files.
This inventories source data; it does not calibrate or retarget the character.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import shutil
import subprocess

REVISION = "54fc563aeaaf819a753d72cc9fdc6713989cdd43"


def sha256(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def write_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def inspect_bvh(path, required_names):
    joints, stack = [], []
    end_site = False
    with path.open(encoding="utf-8") as stream:
        for line in stream:
            t = line.split()
            if not t:
                continue
            if t[0] == "MOTION":
                break
            if t[0] in ("ROOT", "JOINT"):
                joints.append(dict(name=t[1].rsplit(":", 1)[-1], parent=stack[-1] if stack else -1,
                                   channels=[], offset_m=None))
                stack.append(len(joints) - 1)
            elif t[:2] == ["End", "Site"]:
                end_site = True
            elif t[0] == "OFFSET" and not end_site:
                joints[stack[-1]]["offset_m"] = [float(x) * .01 for x in t[1:4]]
            elif t[0] == "CHANNELS":
                assert int(t[1]) == len(t[2:]), path
                joints[stack[-1]]["channels"] = t[2:]
            elif t[0] == "}":
                if end_site:
                    end_site = False
                else:
                    stack.pop()
        assert not stack and joints, path
        frame_count = int(next(stream).split(":")[1])
        frame_time = float(next(stream).split(":")[1])
        assert frame_time > 0 and math.isfinite(frame_time), path
        channels = sum(len(j["channels"]) for j in joints)
        rows = 0
        for line in stream:
            if not line.strip():
                continue
            values = [float(x) for x in line.split()]
            assert len(values) == channels and all(map(math.isfinite, values)), (path, rows)
            rows += 1
        assert rows == frame_count and rows > 0, path
    names = [j["name"] for j in joints]
    assert len(names) == len(set(names)), path
    assert all(n in names for n in required_names), path
    return dict(frames=rows, fps=1 / frame_time, duration_seconds=(rows - 1) * frame_time,
                channel_count=channels, joints=joints,
                model_joint_to_bvh_index=[names.index(n) for n in required_names])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--upstream", type=Path, required=True)
    parser.add_argument("--repo", type=Path, default=Path(__file__).resolve().parents[2])
    args = parser.parse_args()
    source, repo = args.upstream.resolve(), args.repo.resolve()
    revision = subprocess.check_output(["git", "-C", str(source), "rev-parse", "HEAD"], text=True).strip()
    assert revision == REVISION, "Upstream revision changed; review the data contract first."
    dest = repo / "assets/motionbricks"
    manifest = json.loads((source / "motionbricks/assets/onnx/soma/soma_n1.json").read_text())
    assert len(manifest["joint_names"]) == 30 and manifest["sample_rate"] == 30
    patterns = ["motionbricks/assets/onnx/soma/*", "motionbricks/assets/human/*",
                "assets/motions/soma/**/*.bvh", "assets/smart_primitives/soma/*.json",
                "assets/scenes/soma_gym.scene.json", "assets/motions/LICENSE.txt",
                "assets/motions/README.md", "motionbricks/assets/onnx/LICENSE.md", "LICENSE",
                "docs/soma_model.md", "docs/runtime_api.md", "docs/onnx_inference.md",
                "motionbricks/io/bvh.py", "motionbricks/io/usd.py",
                "motionbricks/animation/foot_contacts.py"]
    records, clips = [], []
    selected = sorted({p for pattern in patterns for p in source.glob(pattern) if p.is_file()})
    for original in selected:
        relative = original.relative_to(source).as_posix()
        digest = sha256(original)
        git_blob = subprocess.check_output(["git", "-C", str(source), "show", f"{revision}:{relative}"])
        if git_blob.startswith(b"version https://git-lfs.github.com/spec/v1"):
            pointer = git_blob.decode().splitlines()
            expected = next(x.split(":", 1)[1] for x in pointer if x.startswith("oid "))
            size = int(next(x.split()[1] for x in pointer if x.startswith("size ")))
            assert digest == expected and original.stat().st_size == size, f"LFS missing/corrupt: {relative}"
        else:
            # Git checkout may convert text line endings. Keep the pinned blob as provenance.
            expected = hashlib.sha256(git_blob).hexdigest()
        target = dest / "upstream" / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(original, target)
        records.append(dict(path=target.relative_to(dest).as_posix(), bytes=target.stat().st_size,
                            sha256=digest, upstream_blob_sha256=expected))
        if original.suffix == ".bvh":
            info = inspect_bvh(original, manifest["joint_names"])
            clips.append(dict(path=target.relative_to(dest).as_posix(), **info))
    assert len(clips) == 38, f"Expected 38 SOMA clips, got {len(clips)}"
    write_json(dest / "derived/clip_inventory.json", clips)
    miku_source = repo / "temp/motion-sour/source"
    model = json.loads((repo / "temp/motion-sour/model.json").read_text(encoding="utf-8"))
    assert sha256(miku_source / "Black.pmx") == model["source_sha256"], "Stale Miku parse"
    shutil.copytree(miku_source, dest / "character/source", dirs_exist_ok=True)
    usage = [0.0] * len(model["bones"])
    for vertex in model["vertices"]:
        for index, weight in zip(vertex["skinIndices"], vertex["skinWeights"]):
            if weight > 0:
                assert 0 <= index < len(usage)
                usage[index] += weight
    write_json(dest / "character/skeleton.json", dict(
        source_sha256=model["source_sha256"], coordinate_space="MMDParser.parsePmx(buffer, true); raw MMD units",
        meters_per_unit=None, calibration_status="pending", bones=model["bones"],
        skin_weight_sum=usage, rigid_bodies=model["rigidBodies"], constraints=model["constraints"],
        vertex_count=len(model["vertices"])))
    shutil.copy2(repo / "temp/motion-miku/sour/provenance.json", dest / "character/provenance.json")
    for p in sorted((dest / "character").rglob("*")):
        if p.is_file():
            records.append(dict(path=p.relative_to(dest).as_posix(), bytes=p.stat().st_size, sha256=sha256(p)))
    # Correspondences are deliberately candidates: MMD control/deformation bones differ.
    pairs = {"Hips": ["腰", "下半身"], "Spine1": ["上半身"], "Chest": ["上半身2"],
             "Neck1": ["首"], "Head": ["頭"]}
    for side, jp in [("Left", "左"), ("Right", "右")]:
        for name, names in {"Shoulder": ["肩"], "Arm": ["腕"], "ForeArm": ["ひじ"], "Hand": ["手首"],
                            "Leg": ["足", "足D"], "Shin": ["ひざ", "ひざD"],
                            "Foot": ["足首", "足首D"], "ToeBase": ["つま先", "足先EX"]}.items():
            pairs[side + name] = [jp + n for n in names]
    write_json(dest / "derived/retarget_candidates.json", dict(status="not_runtime_ready",
        source_order=manifest["joint_names"], joints=[dict(soma=name,
            candidates=[dict(index=i, name=b["name"], skin_weight_sum=usage[i])
                        for i, b in enumerate(model["bones"]) if b["name"] in pairs.get(name, [])])
            for name in manifest["joint_names"]],
        unresolved=["reference rotation bases", "MMD unit scale", "spine/neck chain resampling",
                    "control vs deformation bones and grants", "shoe heel/toe contact offsets",
                    "joint limits", "collision proxies", "round-trip and temporal validation"]))
    write_json(dest / "manifest.json", dict(schema_version=1, upstream="https://github.com/NVlabs/motionbricks",
        revision=revision, files=records, clip_count=len(clips), total_frames=sum(c["frames"] for c in clips),
        validation="SHA-256/LFS, BVH rows/channels/finite values, joint-name coverage, Miku source hash",
        not_validated=["ONNX execution on TensorRT 11.4", "retarget calibration", "visual quality"],
        licenses=dict(code="Apache-2.0", models="NVIDIA Open Model Agreement",
                      sample_motion="NVIDIA Sample Data Evaluation License", character="Sour original ReadMe.txt")))
    print(json.dumps(dict(bundle=str(dest), files=len(records), clips=len(clips),
                          frames=sum(c["frames"] for c in clips), miku_joints=len(usage)), ensure_ascii=False))


if __name__ == "__main__":
    main()
