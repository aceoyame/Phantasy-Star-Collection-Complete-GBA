#!/usr/bin/env python3
"""Read-only Academy screenshot geometry test. Requires Pillow.
Usage: python verify_r235_geometry.py PSC_R235_NATURAL_PLAYTHROUGH_VISUAL_QA_20261121_V2.zip
"""
from collections import defaultdict
from io import BytesIO
from pathlib import Path
import sys, zipfile
from PIL import Image

def validate(archive: Path) -> None:
    with zipfile.ZipFile(archive) as z:
        original = Image.open(BytesIO(z.read('genesis_academy_11400.png'))).convert('RGB')
        r235 = Image.open(BytesIO(z.read('gba_academy_12137.png'))).convert('RGB')
    assert original.size == (320, 224)
    assert r235.size == (240, 160)
    mapping = defaultdict(set)
    for y in range(55):
        for x in range(180):
            mapping[original.getpixel((x, y))].add(r235.getpixel((x, y)))
    violations = sum(len(v) != 1 for v in mapping.values())
    assert not violations, f"{violations} color mapping violations"
    assert len(mapping) == 11, f"Expected 11 colors; found {len(mapping)}"
    print(f"PASS: 9900 Academy pixels, {len(mapping)} colors, {violations} mapping violations")
if __name__ == '__main__':
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    validate(Path(sys.argv[1]))
