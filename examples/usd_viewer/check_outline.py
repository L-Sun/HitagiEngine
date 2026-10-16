"""Check fractional outline widths and stage isolation through the live MCP.

Uses the current scene/camera; restores quality and lighting after the check.
Image checks cover continuity and isolation, not artistic quality.
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
        args=[str(Path(__file__).with_name('mcp_server.py')), '--bridge', str(args.bridge.resolve())],
        cwd=str(ROOT))
    async with stdio_client(params) as (read, write):
        async with ClientSession(read, write) as client:
            await client.initialize()

            async def call(name, arguments):
                result = await client.call_tool(name, arguments)
                assert not result.isError, result
                return result

            async def capture(name, stage='all'):
                result = await call('viewer_capture', {'stage': stage})
                dest = output / name
                dest.mkdir(exist_ok=True)
                metadata = json.loads(result.content[0].text)
                (dest / 'manifest.json').write_text(json.dumps(metadata, indent=2))
                images = {}
                key = None
                for block in result.content[1:]:
                    if block.type == 'text':
                        key = block.text
                    elif block.type == 'image':
                        path = dest / f'{key}.png'
                        path.write_bytes(base64.b64decode(block.data))
                        images[key] = np.asarray(Image.open(path))
                assert len(images) == (15 if stage == 'all' else 1)
                print(f'Captured {name}', flush=True)
                return images

            state = json.loads((await call('viewer_diagnostics', {})).content[0].text)
            stats = {}
            try:
                for scale in (1, 2):
                    await call('viewer_set_quality', {**state['quality'], 'scale': scale})
                    await call('viewer_set_lighting', {'direction': state['light'], 'outline': 0})
                    base = await capture(f'{scale}x-zero')
                    composed = np.where(base['toon'][:, :, 3:] > 0,
                                        base['toon'][:, :, :3], base['world'][:, :, :3])
                    assert np.array_equal(composed, base['outline'][:, :, :3]), 'Zero width changed composition'
                    energy = []
                    for width in (.25, .5, .75, 1):
                        await call('viewer_set_lighting', {'direction': state['light'], 'outline': width})
                        images = await capture(f'{scale}x-{width}', 'all' if width == 1 else 'outline')
                        difference = np.abs(images['outline'][:, :, :3].astype(float)
                                            - base['outline'][:, :, :3])
                        energy.append(float(difference.sum()))
                    assert energy[0] > 0, 'Subpixel width vanished'
                    assert all(b > a * 1.01 for a, b in zip(energy, energy[1:])), 'Widths still quantized'
                    for stage in base.keys() - {'outline', 'final'}:
                        assert np.array_equal(base[stage], images[stage]), f'Outline changed {stage}'
                    repeated = await capture(f'{scale}x-repeat', 'final')
                    assert np.array_equal(images['final'], repeated['final']), 'Static edges are unstable'
                    stats[f'{scale}x'] = {'outline_difference_sum': energy,
                                          'widths': [.25, .5, .75, 1], 'stable': True}
            finally:
                await call('viewer_set_quality', state['quality'])
                await call('viewer_set_lighting', {'direction': state['light'], 'outline': state['outline']})
            (output / 'checks.json').write_text(json.dumps({'ok': True, **stats}, indent=2))
            print(json.dumps({'ok': True, **stats}), flush=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bridge', type=Path, default=ROOT / 'temp/usd-viewer/live')
    parser.add_argument('--output', type=Path, default=ROOT / 'temp/usd-viewer/outline-validation')
    asyncio.run(check(parser.parse_args()))
