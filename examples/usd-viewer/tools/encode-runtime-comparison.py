"""Encode actual Viewer GPU frames; no synthesized frames or motion interpolation."""
import json
from pathlib import Path
import subprocess
from PIL import Image

root = Path(__file__).resolve().parents[3]
recordings = root / "temp/usd-viewer-recording"
off = recordings / "miku_bullet_physics/DX12-toon-physics-off"
on = recordings / "miku_bullet_physics/DX12-toon-physics-on"
stem = "miku-jolt-physics-comparison"
output = root / "output"
output.mkdir(exist_ok=True)
width, height = Image.open(on / "frame-025.png").size
dpi = width / 1000
assert abs(height - round(700 * dpi)) <= dpi, "Unexpected recording window size"
assert Image.open(off / "frame-025.png").size == (width, height)
for folder in (off, on):
    assert all((folder / f"frame-{i:03}.png").exists() for i in range(25, 625))
    trace = [json.loads(line) for line in (folder / "record.jsonl").read_text().splitlines()]
    assert len(trace) == 624
    assert all(not entry["state"]["error"] for entry in trace)
    assert all(entry["state"]["physicsBackend"] == "Jolt" for entry in trace)
    if folder == off:
        reference = trace
    else:
        assert all(a["state"]["animationTime"] == b["state"]["animationTime"]
                   and max(abs(x-y) for x, y in zip(a["state"]["eye"], b["state"]["eye"])) < 1e-6
                   for a, b in zip(reference[24:], trace[24:]))

font = "fontfile='C\\:/Windows/Fonts/segoeui.ttf'"
def label(text, x, y, size=22, color="0xe2e8f0"):
    return f"drawtext={font}:text='{text}':x={x}:y={y}:fontsize={size}:fontcolor={color}"

crop = f"crop={round(492*dpi)}:{height-round(220*dpi)}:{round(248*dpi)}:{round(112*dpi)},scale=738:720"
graph = (
    f"[0:v]{crop}[left];[1:v]{crop}[right];"
    "[left][right]hstack=inputs=2,pad=1476:856:0:60:color=0x111827,"
    + ",".join([
        label("PHYSICS OFF / UsdSkel animation", 24, 18, 24),
        label("JOLT / Authored hair + garment rig", 762, 18, 24, "0x83e2cb"),
        label("Hitagi USD Viewer / DX12 Toon / same pose and camera / 60 fps", 24, 792, 20),
        label("Model - Sourxuan / Crypton Future Media | WAVEFILE motion - hino | No physics bake", 24, 823, 17),
    ])
)
command = ["ffmpeg", "-y", "-hide_banner", "-loglevel", "warning"]
for folder in (off, on):
    command += ["-framerate", "60", "-start_number", "25", "-i", str(folder / "frame-%03d.png")]
video = output / f"{stem}.mp4"
subprocess.run(command + ["-filter_complex", graph, "-frames:v", "600", "-c:v", "libx264",
                         "-crf", "18", "-pix_fmt", "yuv420p", "-movflags", "+faststart", str(video)], check=True)
subprocess.run(["ffmpeg", "-y", "-hide_banner", "-loglevel", "warning",
                "-ss", "2", "-i", str(video), "-frames:v", "1", "-update", "1",
                str(output / f"{stem}.png")], check=True)
print(f"Verified matching camera/time traces. Encoded {video}")
