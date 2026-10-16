"""MCP adapter for the running native USD game, using an atomic local mailbox.

GPU/scene work stays on the game's render thread. Only completed GPU readbacks
are returned as MCP ImageContent; this never takes desktop/window screenshots.
"""
import argparse
import asyncio
import json
from pathlib import Path
import uuid

from mcp.server.fastmcp import FastMCP, Image

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser()
parser.add_argument("--bridge", type=Path, default=ROOT / "temp/usd-viewer/live")
options = parser.parse_args()
BRIDGE = options.bridge.resolve()
mcp = FastMCP("Hitagi USD Anime Viewer")
lock = asyncio.Lock()


async def request(tool: str, **arguments) -> dict:
    async with lock:
        identifier = str(uuid.uuid4())
        requests = BRIDGE / "requests"
        requests.mkdir(parents=True, exist_ok=True)
        path = requests / f"{identifier}.json"
        temporary = path.with_suffix(".tmp")
        temporary.write_text(json.dumps({"tool": tool, **arguments}, allow_nan=False), encoding="utf-8")
        temporary.replace(path)
        response = BRIDGE / "responses" / path.name
        try:
            async with asyncio.timeout(60):
                while not response.exists():
                    await asyncio.sleep(0.05)
            result = json.loads(response.read_text(encoding="utf-8"))
            response.unlink()
            if not result.get("ok"):
                raise ValueError(result.get("error", "Viewer request failed"))
            return result
        except TimeoutError:
            raise RuntimeError("Native viewer did not respond within 60 seconds. Start usd-viewer-game with the same --bridge directory and restore its window if minimized.") from None
        finally:
            path.unlink(missing_ok=True)
            temporary.unlink(missing_ok=True)


@mcp.tool()
async def viewer_diagnostics() -> dict:
    """Get live scene counts, frame, backend, camera, lighting and available graph stages."""
    return await request("diagnostics")


@mcp.tool()
async def viewer_capture(stage: str = "final") -> list:
    """Capture a fresh GPU frame, including material_id, shadow_factor, lighting_band, specular and hair_shadow_depth diagnostics.

    Returns frame metadata and PNG image content. 'all' captures every stage in
    the same frame. Depth PNGs are visualizations; adjacent .f32 files contain
    raw float32 device depth. Captures remain in temp/usd-viewer for inspection.
    """
    if stage not in {"all", "world", "shadow", "shadow_depth", "hair_shadow", "hair_shadow_depth", "albedo", "normals", "depth", "toon", "outline", "final", "material_id", "shadow_factor", "lighting_band", "specular"}:
        raise ValueError("Unknown Render Graph stage")
    result = await request("capture", stage=stage)
    content = [json.dumps(result, ensure_ascii=False)]
    for name, path in result["images"].items():
        image = Path(path).resolve()
        if not image.is_relative_to(BRIDGE / "captures"):
            raise ValueError("Capture path escaped the viewer capture directory")
        content.extend([name, Image(path=image, format="png")])
    return content


@mcp.tool()
async def viewer_set_camera(yaw: float, pitch: float, distance: float, target: list[float] | None = None) -> dict:
    """Set orbit camera in radians, distance in scene units, optional [x,y,z] target. Pitch [-0.7,1.3], distance [0.3,30]."""
    args = {"yaw": yaw, "pitch": pitch, "distance": distance}
    if target is not None:
        args["target"] = target
    return await request("camera", **args)


@mcp.tool()
async def viewer_set_lighting(direction: list[float], outline: float = 1.0) -> dict:
    """Set direction toward the sun [x,y,z] and outline width [0,4] pixels; triggers shadow and toon updates."""
    return await request("lighting", direction=direction, outline=outline)


@mcp.tool()
async def viewer_set_quality(scale: int = 2, shadows: bool = True, face_lighting: bool = True, specular: bool = True, asset_controls: bool = True) -> dict:
    """Set 1x/2x/4x per-axis render scale and independent lighting toggles for visual A/B diagnosis.

    scale=2 renders four subpixels per output pixel; scale=4 renders sixteen for
    fine distant geometry, with four times the internal pixels of scale=2.
    Final PNG uses the viewport
    size; other screen stages use the internal size reported in image_sizes.
    asset_controls=False bypasses Blender-authored face/flow maps while keeping
    the same mesh, normals and albedo; analytic lighting remains available.
    """
    if scale not in (1, 2, 4):
        raise ValueError("Render scale must be 1, 2 or 4")
    return await request("quality", scale=scale, shadows=shadows, face_lighting=face_lighting, specular=specular, asset_controls=asset_controls)


@mcp.tool()
async def viewer_open_usd(path: str) -> dict:
    """Open a local USD/USDA/USDC/USDZ stage in the native viewer. Static UsdGeomMesh/UsdPreviewSurface subset."""
    source = Path(path).resolve()
    if not source.is_file():
        raise ValueError("USD file does not exist")
    return await request("open", path=str(source))


if __name__ == "__main__":
    mcp.run(transport="stdio")
