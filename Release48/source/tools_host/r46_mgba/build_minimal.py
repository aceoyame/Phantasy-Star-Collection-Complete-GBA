#!/usr/bin/env python3
"""Offline GCC-only diagnostic bridge build when CMake is unavailable.

Uses the bundled, unmodified upstream mGBA 0.10.5 sources and source lists.
No PNG/GL/frontends/network/archive/script support; raw states/PCM/software
video and debugger remain. Never linked into the GBA release.
"""
import argparse, concurrent.futures, re, subprocess, tarfile, tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
parser=argparse.ArgumentParser();parser.add_argument('--out',type=Path);args=parser.parse_args()
WORK=args.out.resolve() if args.out else Path(tempfile.mkdtemp(prefix='r46-mgba-gcc-'))
WORK.mkdir(parents=True,exist_ok=True)
with tarfile.open(HERE / 'mgba-0.10.5.tar.gz') as archive:
    archive.extractall(WORK, filter='data')
SRC = WORK / 'mgba-0.10.5'
INC = WORK / 'include'
(INC / 'mgba').mkdir(parents=True,exist_ok=True)
enabled = {'M_CORE_GBA', 'USE_DEBUGGERS', 'USE_PTHREADS', 'USE_ZLIB',
           'HAVE_CRC32', 'HAVE_LOCALTIME_R', 'HAVE_STRDUP', 'HAVE_STRTOF_L', 'HAVE_LOCALE'}
template = (SRC / 'src/core/flags.h.in').read_text()
flags = re.sub(r'^#cmakedefine ([A-Z0-9_]+).*$',
               lambda m: '#define ' + m[1] if m[1] in enabled else '/* disabled ' + m[1] + ' */',
               template, flags=re.M)
(INC / 'mgba/flags.h').write_text(flags)
version = (SRC / 'src/core/version.c.in').read_text()
values = {'GIT_COMMIT': '', 'GIT_COMMIT_SHORT': '', 'GIT_BRANCH': '',
          'GIT_REV': '0', 'BINARY_NAME': 'mgba', 'PROJECT_NAME': 'mGBA',
          'VERSION_STRING': '0.10.5'}
for name, value in values.items():
    version = version.replace('${' + name + '}', value)
(WORK / 'version.c').write_text(version)

def sources(directory, variable='SOURCE_FILES'):
    text = (SRC / directory / 'CMakeLists.txt').read_text()
    block = re.search(r'set\(' + variable + r'\s+(.*?)\)', text, re.S)
    assert block, (directory, variable)
    out = []
    for entry in block[1].split():
        if entry.startswith('${'):
            out.extend(sources(directory, entry[2:-1]))
        elif entry.endswith('.c'):
            out.append((SRC / directory / entry).resolve())
    return out

files = sources('src/core') + sources('src/arm') + sources('src/gba') + sources('src/util')
files += sources('src/debugger') + sources('src/arm', 'DEBUGGER_FILES') + sources('src/gba', 'DEBUGGER_FILES')
files += sources('src/gba', 'SIO_FILES') + sources('src/gba', 'EXTRA_FILES') + sources('src/feature')
files += list((SRC / 'src/platform/posix').glob('*.c'))
files += [SRC / ('src/util/vfs/' + x) for x in ['vfs-mem.c', 'vfs-fifo.c', 'vfs-fd.c', 'vfs-dirent.c']]
files += list((SRC / 'src/third-party/inih').glob('*.c'))
files += [SRC / 'src/third-party/blip_buf/blip_buf.c', WORK / 'version.c']
common = ['gcc', '-O2', '-std=gnu11', '-fPIC', '-D_GNU_SOURCE', '-DMGBA_DLL',
          *['-D' + name for name in sorted(enabled)],
          '-I' + str(INC), '-I' + str(SRC / 'include'), '-I' + str(SRC / 'src')]

def compile_one(item):
    index, source = item
    obj = WORK / (str(index) + '.o')
    run = subprocess.run(common + ['-c', str(source), '-o', str(obj)], capture_output=True, text=True)
    if run.returncode:
        raise RuntimeError(str(source) + '\n' + run.stderr)
    return obj

with concurrent.futures.ThreadPoolExecutor(max_workers=6) as pool:
    objects = list(pool.map(compile_one, enumerate(files)))
subprocess.run(['gcc', '-shared', '-Wl,--no-undefined', *map(str, objects),
                '-lm', '-lpthread', '-lz', '-o', str(WORK / 'libmgba.so')], check=True)
subprocess.run(common + ['-shared', str(HERE / 'probe.c'), '-L' + str(WORK),
                        '-Wl,-rpath,' + str(WORK), '-lmgba', '-o', str(HERE / 'libprobe.so')], check=True)
print('GCC-only upstream 0.10.5 bridge ready; keep runtime directory:', WORK, flush=True)
