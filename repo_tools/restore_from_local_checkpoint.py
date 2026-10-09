#!/usr/bin/env python3
"""Restore ignored binary inputs locally from user's own complete CP58 checkpoint.

The original full ZIP stays outside Git. The restored content is not committed.
"""
import argparse
from pathlib import Path, PurePosixPath
import shutil
import zipfile

root = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('checkpoint', type=Path)
p.add_argument('--include-built', action='store_true')
p.add_argument('--include-toolchain', action='store_true')
a = p.parse_args()
with zipfile.ZipFile(a.checkpoint) as z:
    n = 0
    for item in z.infolist():
        if item.is_dir():
            continue
        name = item.filename
        parsed = PurePosixPath(name)
        if parsed.is_absolute() or '..' in parsed.parts or '\\' in name:
            raise ValueError(f'Invalid path: {name}')
        accept = (name.startswith('Release48/additional_roms/') or
                  name.startswith('Release48/binaries/') or
                  (name.startswith('Release48/source/gba/') and name.endswith('.bin')) or
                  (name.startswith('Release48/source/psc8/') and name.endswith(('.bin', '.sms', '.gg'))))
        if a.include_built:
            accept |= (name.startswith('Release48/source/additional/build/') or
                       name.startswith('native58/armcheck/') or
                       name.startswith('Release48/build_artifacts/'))
        if a.include_toolchain:
            accept |= (name == 'SUPPORT/R48_LLVM18_1_3_MINIMAL_REBUILD_TOOLCHAIN.tar.gz')
        if not accept:
            continue
        target = root.joinpath(*parsed.parts)
        target.parent.mkdir(parents=True, exist_ok=True)
        with z.open(item) as src, target.open('wb') as dest:
            shutil.copyfileobj(src, dest)
        n += 1
print(f'Locally restored {n} ignored inputs. Do not force-add them to Git.')
