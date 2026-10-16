"""Exercise the real stdio MCP server against a running native viewer."""
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
    params = StdioServerParameters(command=sys.executable, args=[str(Path(__file__).with_name("mcp_server.py")), "--bridge", str(args.bridge.resolve())], cwd=str(ROOT))
    async with stdio_client(params) as (read, write):
        async with ClientSession(read, write) as client:
            await client.initialize()
            tools = await client.list_tools()
            assert {t.name for t in tools.tools} == {"viewer_capture", "viewer_diagnostics", "viewer_set_camera", "viewer_set_lighting", "viewer_set_quality", "viewer_open_usd"}
            if args.scene:
                opened = await client.call_tool("viewer_open_usd", {"path": str(args.scene.resolve())})
                assert not opened.isError, opened
            if args.light:
                lighting = await client.call_tool("viewer_set_lighting", {"direction": args.light, "outline": 1})
                assert not lighting.isError, lighting
            if args.yaw is not None:
                camera = await client.call_tool("viewer_set_camera", {"yaw":args.yaw, "pitch":args.pitch,"distance":args.distance})
                assert not camera.isError, camera
            result = await client.call_tool("viewer_capture", {"stage":"all"})
            assert not result.isError, result
            # First text block is the game's same-frame manifest, followed by stage labels/images.
            metadata = json.loads(result.content[0].text)
            assert metadata["triangles"] > 0 and metadata["draws_per_geometry_pass"] > 0
            assert metadata["render_graph_frame"] > 0
            assert metadata["material_binding_errors"] == []
            contracts = metadata["material_contracts"]
            assert contracts["AnimeForward"]["draws"] == metadata["draws_per_geometry_pass"]
            assert contracts["AnimeForward"]["material_bytes"] > 0
            assert {"AnimeForward", "AnimeAlbedo", "AnimeNormals", "AnimeShadowCaster"} <= contracts.keys()
            graph = Path(metadata["render_graph_dot"]).read_text(encoding="utf-8")
            assert "digraph" in graph and "toon" in graph and "outline" in graph
            (output / "manifest.json").write_text(json.dumps(metadata,indent=2),encoding="utf-8")
            name = None
            count = 0
            for block in result.content[1:]:
                if block.type == "text":
                    name = block.text
                elif block.type == "image":
                    assert name in metadata["images"] and block.mimeType == "image/png"
                    data = base64.b64decode(block.data)
                    assert data.startswith(b"\x89PNG")
                    (output / f"{name}.png").write_bytes(data)
                    count += 1
            assert count == len(metadata["stages"]) == 15, count
            albedo = np.asarray(Image.open(output / "albedo.png"))
            normals = np.asarray(Image.open(output / "normals.png"))
            final = np.asarray(Image.open(output / "final.png"))
            world = np.asarray(Image.open(output / "world.png"))
            mask = albedo[:,:,3] > 0
            assert 0.005 < mask.mean() < 0.8, "Character is empty or incorrectly framed"
            assert np.array_equal(mask, normals[:,:,3] > 0), "Geometry stages disagree on visibility"
            n = normals[:,:,:3][mask].astype(float)/255*2-1
            normal_error = float(np.abs(np.linalg.norm(n,axis=1)-1).mean())
            assert normal_error < 0.025, "Normals/CPU-shader ABI are invalid"
            raw_depth = Path(metadata["images"]["depth"]).with_suffix(".f32")
            depth = np.fromfile(raw_depth,dtype="<f4").reshape(mask.shape)
            assert np.isfinite(depth).all() and np.all((depth[mask]>0)&(depth[mask]<1))
            assert np.all(depth[~mask] == 1), "Depth clear/cutout does not match albedo"
            final_size = (final.shape[1],final.shape[0])
            resolved_world = np.asarray(Image.fromarray(world).resize(final_size,Image.Resampling.BOX))
            resolved_mask = np.asarray(Image.fromarray(mask).resize(final_size,Image.Resampling.NEAREST))
            assert np.abs(final[:,:,:3].astype(float)-resolved_world[:,:,:3])[resolved_mask].mean() > 10
            assert metadata["image_sizes"]["final"] == list(final_size)
            assert albedo.shape[1] == final.shape[1]*metadata["quality"]["scale"]
            stats = {"coverage":float(mask.mean()),"normal_length_error":normal_error,"depth_min":float(depth.min()),"finite_depth":True}
            (output/"image-checks.json").write_text(json.dumps(stats,indent=2),encoding="utf-8")
            invalid = await client.call_tool("viewer_capture", {"stage":"missing"})
            assert invalid.isError
            before = await client.call_tool("viewer_diagnostics", {})
            invalid = await client.call_tool("viewer_set_camera", {"yaw":2,"pitch":0,"distance":2,"target":[0,0]})
            assert invalid.isError
            after = await client.call_tool("viewer_diagnostics", {})
            assert json.loads(before.content[0].text)["camera"] == json.loads(after.content[0].text)["camera"], "Rejected camera update mutated state"
            invalid = await client.call_tool("viewer_set_quality", {"scale": 3})
            assert invalid.isError
            after_quality = await client.call_tool("viewer_diagnostics", {})
            assert json.loads(after_quality.content[0].text)["quality"] == json.loads(after.content[0].text)["quality"]
            empty_stage = output / "empty.usda"
            empty_stage.write_text('#usda 1.0\ndef Xform "Empty" {}\n',encoding="utf-8")
            invalid = await client.call_tool("viewer_open_usd", {"path":str(empty_stage)})
            assert invalid.isError
            after = await client.call_tool("viewer_diagnostics", {})
            state = json.loads(after.content[0].text)
            assert state["scene"] == metadata["scene"] and state["triangles"] == metadata["triangles"], "Failed USD import replaced the live scene"
            invalid = await client.call_tool("viewer_set_camera", {"yaw":0,"pitch":0,"distance":-1})
            assert invalid.isError
            print(json.dumps({"ok":True,"frame":metadata["frame"],"images":count,"triangles":metadata["triangles"],"output":str(output)},indent=2))


if __name__ == "__main__":
    parser=argparse.ArgumentParser()
    parser.add_argument("--bridge",type=Path,default=ROOT/"temp/usd-viewer/live")
    parser.add_argument("--output",type=Path,default=ROOT/"temp/usd-viewer/mcp-check")
    parser.add_argument("--yaw",type=float)
    parser.add_argument("--scene",type=Path)
    parser.add_argument("--light",type=float,nargs=3)
    parser.add_argument("--pitch",type=float,default=0.08)
    parser.add_argument("--distance",type=float,default=3.15)
    asyncio.run(check(parser.parse_args()))
