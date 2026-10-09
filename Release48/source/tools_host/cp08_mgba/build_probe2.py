#!/usr/bin/env python3
"""Build the CP08 test bridge (probe2.c: probe.c + PC/cost histogram export) against an mGBA 0.10.5 runtime
directory produced by ../r46_mgba/build.py or build_minimal.py.

  python3 build_probe2.py --runtime-dir /tmp/r46-mgba-XXXX
"""
from pathlib import Path
import argparse, subprocess
HERE = Path(__file__).resolve().parent
ap = argparse.ArgumentParser(); ap.add_argument('--runtime-dir', type=Path, required=True); a = ap.parse_args()
rt = a.runtime_dir.resolve()
inc = [rt / 'include', rt / 'mgba-0.10.5/include', rt / 'mgba-0.10.5/src']
cmd = ['gcc', '-shared', '-fPIC', '-O2', *['-I' + str(i) for i in inc if i.exists()], str(HERE / 'probe2.c'),
       '-L' + str(rt), '-L' + str(rt / 'build'), '-Wl,-rpath,' + str(rt), '-Wl,-rpath,' + str(rt / 'build'), '-lmgba', '-o', str(HERE / 'libprobe2.so')]
subprocess.run(cmd, check=True)
print('built', HERE / 'libprobe2.so')
