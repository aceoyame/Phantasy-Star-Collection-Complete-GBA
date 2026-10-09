#!/usr/bin/env python3
"""Build the bundled upstream mGBA and this test-only ctypes bridge offline."""
from pathlib import Path
import argparse,subprocess,tarfile,tempfile
HERE=Path(__file__).resolve().parent
ap=argparse.ArgumentParser();ap.add_argument('--work-dir',type=Path);ap.add_argument('--cmake',default='cmake');a=ap.parse_args()
work=a.work_dir.resolve() if a.work_dir else Path(tempfile.mkdtemp(prefix='r46-mgba-'))
work.mkdir(parents=True,exist_ok=True)
with tarfile.open(HERE/'mgba-0.10.5.tar.gz') as t:t.extractall(work,filter='data')
src=work/'mgba-0.10.5';build=work/'build'
subprocess.run([a.cmake,'-S',str(src),'-B',str(build),'-DCMAKE_POLICY_VERSION_MINIMUM=3.5','-DBUILD_QT=OFF','-DBUILD_SDL=OFF','-DBUILD_LIBRETRO=OFF','-DBUILD_STATIC=OFF','-DBUILD_SHARED=ON','-DUSE_FFMPEG=OFF','-DUSE_SQLITE3=OFF','-DUSE_LUA=OFF','-DUSE_LIBZIP=OFF','-DM_CORE_GB=OFF','-DBUILD_TEST=OFF','-DCMAKE_BUILD_TYPE=Release'],check=True)
subprocess.run([a.cmake,'--build',str(build),'-j6'],check=True)
subprocess.run(['gcc','-shared','-fPIC','-O2','-I'+str(build/'include'),'-I'+str(src/'include'),str(HERE/'probe.c'),'-L'+str(build),'-Wl,-rpath,'+str(build),'-lmgba','-o',str(HERE/'libprobe.so')],check=True)
print('Bridge ready. Keep its runtime build directory:',work)
