"""Compare a captured 4x GPU resolve to a dense CPU tent reconstruction.

Run check_mcp.py with the live viewer at scale=4 first, then pass its output
directory. Uses actual RenderGraph images; does not re-render or edit assets.
"""
import argparse
import json
from pathlib import Path

import numpy as np
from PIL import Image


def check(directory):
    metadata = json.loads((directory / 'manifest.json').read_text())
    assert metadata['quality']['scale'] == 4, 'Capture must use scale=4'
    source = np.asarray(Image.open(directory / 'outline.png'))[:, :, :3]
    final = np.asarray(Image.open(directory / 'final.png'))[:, :, :3]
    height, width = final.shape[:2]
    assert source.shape[:2] == (height * 4, width * 4)
    # Dense separable convolution on all 8x8 source samples. This independently
    # checks the GPU's bilinear pair compression, phase, borders and weights.
    weights = np.array([1, 3, 5, 7, 7, 5, 3, 1], dtype=np.float32) / 32
    horizontal = np.zeros((height * 4, width, 3), dtype=np.float32)
    x = np.arange(width) * 4 - 2
    for i, weight in enumerate(weights):
        horizontal += source[:, np.clip(x + i, 0, width * 4 - 1)] * weight
    reference = np.zeros(final.shape, dtype=np.float32)
    y = np.arange(height) * 4 - 2
    for i, weight in enumerate(weights):
        reference += horizontal[np.clip(y + i, 0, height * 4 - 1)] * weight
    error = np.abs(final.astype(np.float32) - reference)
    # Allow output UNORM rounding and hardware bilinear interpolation precision.
    assert error.max() <= 2, f'Resolve mismatch: {error.max()}'
    assert error.mean() < .4, f'Resolve phase/weight mismatch: {error.mean()}'
    stats = {'ok': True, 'source_size': [width * 4, height * 4],
             'output_size': [width, height], 'max_channel_error': float(error.max()),
             'mean_channel_error': float(error.mean())}
    (directory / 'resolve-checks.json').write_text(json.dumps(stats, indent=2))
    print(json.dumps(stats, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('capture', type=Path)
    check(parser.parse_args().capture.resolve())
