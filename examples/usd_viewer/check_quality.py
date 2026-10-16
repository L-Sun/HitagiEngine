"""Capture close-up lighting ablations through MCP and verify their isolation.

Requires a live viewer and the prepared Miku USD. Saves unmodified GPU images;
image statistics supplement, rather than replace, visual inspection.
"""
import argparse
import asyncio
import base64
import json
from pathlib import Path
import sys

from mcp import ClientSession, StdioServerParameters
from mcp.client.stdio import stdio_client
import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]


async def check(args):
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    params = StdioServerParameters(command=sys.executable,
        args=[str(Path(__file__).with_name("mcp_server.py")), "--bridge", str(args.bridge.resolve())], cwd=str(ROOT))
    async with stdio_client(params) as (read, write):
        async with ClientSession(read, write) as client:
            await client.initialize()

            async def call(name, arguments):
                result = await client.call_tool(name, arguments)
                assert not result.isError, result
                return result

            async def capture(name, stage="all"):
                result = await call("viewer_capture", {"stage": stage})
                dest = output / name
                dest.mkdir(exist_ok=True)
                metadata = json.loads(result.content[0].text)
                (dest / "manifest.json").write_text(json.dumps(metadata, indent=2))
                images = {}
                key = None
                for block in result.content[1:]:
                    if block.type == "text":
                        key = block.text
                    elif block.type == "image":
                        path = dest / f"{key}.png"
                        path.write_bytes(base64.b64decode(block.data))
                        images[key] = np.asarray(Image.open(path))
                        assert list(Image.open(path).size) == metadata["image_sizes"][key]
                assert len(images) == (15 if stage == "all" else 1)
                print(f"Captured {name}, frame {metadata['render_graph_frame']}", flush=True)
                return images, metadata

            state = json.loads((await call("viewer_diagnostics", {})).content[0].text)
            try:
                await call("viewer_open_usd", {"path": str(args.scene.resolve())})
                await call("viewer_set_camera", {"yaw": .13, "pitch": .02, "distance": .85, "target": [0, 1.49, -.06]})
                await call("viewer_set_lighting", {"direction": [-.6, .8, -.7], "outline": 1})
                await call("viewer_set_quality", {})
                reference, metadata = await capture("face")
                assert metadata["triangles"] == 22961
                ids = reference["material_id"][:, :, :3].astype(int)
                face = np.max(np.abs(ids - [255, 76, 128]), axis=2) <= 1
                hair = np.max(np.abs(ids - [26, 204, 255]), axis=2) <= 1
                mask = reference["albedo"][:, :, 3] > 0
                assert face.sum() > 1000 and hair.sum() > 1000, "Missing semantic material regions"
                assert reference["specular"][:, :, :3][hair].max() > 10, "Hair highlight is missing"
                assert np.any(reference["shadow_factor"][:, :, 0][mask] < 200), "Shadow pass is not affecting the character"
                stats = {}
                for name, toggle in [("no-shadow", "shadows"), ("no-face", "face_lighting"), ("no-specular", "specular")]:
                    await call("viewer_set_quality", {toggle: False})
                    images, _ = await capture(name)
                    assert np.array_equal(images["albedo"], reference["albedo"]), "Lighting toggle changed geometry/base color"
                    if toggle == "shadows":
                        assert np.all(images["shadow_factor"][:, :, 0][mask] == 255)
                    elif toggle == "face_lighting":
                        diff = np.abs(images["lighting_band"][:, :, 0].astype(int)-reference["lighting_band"][:, :, 0].astype(int))
                        assert diff[face].mean() > 1 and diff[~face].max() == 0, "Face control leaked to other materials"
                    else:
                        assert images["specular"][:, :, :3].max() == 0
                    stats[name + "_mean_rgb_difference"] = float(np.abs(images["final"][:, :, :3].astype(float)-reference["final"][:, :, :3]).mean())
                    del images
                if metadata.get("control_maps"):
                    assert len(metadata["control_maps"]) == 2, "Expected face and hair control maps"
                    await call("viewer_set_quality", {"asset_controls": False})
                    images, _ = await capture("no-asset-controls")
                    for stage in ("albedo", "normals", "material_id", "depth"):
                        assert np.array_equal(images[stage], reference[stage]), f"Control map toggle changed {stage}"
                    band_diff = np.abs(images["lighting_band"][:, :, 0].astype(int)-reference["lighting_band"][:, :, 0].astype(int))
                    spec_diff = np.abs(images["specular"][:, :, :3].astype(int)-reference["specular"][:, :, :3].astype(int)).mean(axis=2)
                    assert band_diff[face].mean() > .05 and band_diff[~face].max() == 0, "Face map not consumed or affects other materials"
                    assert spec_diff[hair].mean() > .05 and spec_diff[~hair].max() == 0, "Hair map not consumed or affects other materials"
                    stats["face_map_mean_band_difference"] = float(band_diff[face].mean())
                    stats["hair_map_mean_specular_difference"] = float(spec_diff[hair].mean())
                    del images
                await call("viewer_set_quality", {"scale": 1})
                await capture("face-1x", "final")
                await call("viewer_set_quality", {})
                await call("viewer_set_lighting", {"direction": [.8, .35, -.3], "outline": 1})
                await capture("face-right-light", "final")
                await call("viewer_set_lighting", {"direction": [-.6, .8, -.7], "outline": 1})
                await call("viewer_set_camera", {"yaw": .65, "pitch": .02, "distance": .85, "target": [0, 1.49, -.06]})
                await capture("face-side", "final")
                await call("viewer_set_camera", {"yaw": .13, "pitch": .02, "distance": 1.1, "target": [0, 1.05, 0]})
                await capture("torso", "final")
                await call("viewer_set_camera", {"yaw": .13, "pitch": .05, "distance": 2.8, "target": [0, .85, 0]})
                await capture("full", "final")

                raw_shadow = Path(metadata["images"]["shadow_depth"]).with_suffix(".f32")
                depth = np.fromfile(raw_shadow, dtype="<f4").reshape(2048, 2048)
                assert np.isfinite(depth).all()
                y, x = np.where(depth < 1)
                stats["shadow_occupied_bbox"] = [int(x.max()-x.min()+1), int(y.max()-y.min()+1)]
                # Remove edges to measure only interior face shading variation.
                final_size = (reference["final"].shape[1], reference["final"].shape[0])
                interior = np.asarray(Image.fromarray(face).resize(final_size, Image.Resampling.NEAREST)).copy()
                for _ in range(8):
                    interior &= np.roll(interior, 1, 0) & np.roll(interior, -1, 0) & np.roll(interior, 1, 1) & np.roll(interior, -1, 1)
                if args.before:
                    before = np.asarray(Image.open(args.before))
                    after = reference["final"]
                    assert before.shape == after.shape, "Use identical viewport dimensions for before/after"
                    for name, pixels in [("before", before), ("after", after)]:
                        gray = pixels[:, :, :3].astype(float).mean(axis=2)
                        laplacian = np.roll(gray, 1, 0)+np.roll(gray, -1, 0)+np.roll(gray, 1, 1)+np.roll(gray, -1, 1)-4*gray
                        stats[name + "_face_interior_laplacian"] = float(np.abs(laplacian[interior]).mean())
                (output / "quality-checks.json").write_text(json.dumps(stats, indent=2))
                print(json.dumps(stats, indent=2))
            finally:
                if state["scene"] != str(args.scene.resolve()).replace("\\", "/"):
                    await call("viewer_open_usd", {"path": state["scene"]})
                await call("viewer_set_camera", state["camera"])
                await call("viewer_set_lighting", {"direction": state["light"], "outline": state["outline"]})
                await call("viewer_set_quality", state["quality"])


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--bridge", type=Path, default=ROOT / "temp/usd-viewer/live")
    parser.add_argument("--output", type=Path, default=ROOT / "temp/usd-viewer/quality-validation")
    parser.add_argument("--before", type=Path)
    parser.add_argument("--scene", type=Path, default=ROOT / "assets/anime_viewer/miku/miku.usda")
    asyncio.run(check(parser.parse_args()))
