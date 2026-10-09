#!/usr/bin/env python3
"""Restore the pinned Ubuntu LLVM18 toolchain without changing system packages.
Only dependency archives are hashed. Project audio/assets are never regenerated.
Downloads use bounded ranges; regular files extract in 1MiB writes and are
size-checked to detect interrupted/truncated extraction.
"""
import argparse,concurrent.futures,hashlib,json,os,subprocess,tarfile,time,urllib.request
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--out',type=Path,required=True);p.add_argument('--archives',type=Path);p.add_argument('--size-only',action='store_true');a=p.parse_args();root=a.out.resolve();root.mkdir(parents=True,exist_ok=True);(root/'bin').mkdir(exist_ok=True)
def valid(path,info):
 return path.exists() and path.stat().st_size==info['size'] and (a.size_only or hashlib.sha256(path.read_bytes()).hexdigest()==info['sha256'])
for info in json.loads(Path(__file__).with_name('clang18_packages.json').read_text()):
 dest=root/Path(info['filename']).name;size=info['size'];
 if a.archives:
  import shutil
  candidates=list(a.archives.glob(info['package']+'_*.deb'))
  for candidate in candidates:
   if valid(candidate,info):shutil.copyfile(candidate,dest);break
 url='https://snapshot.ubuntu.com/ubuntu/20260828T000000Z/'+info['filename']
 if not valid(dest,info):
  def chunk(start):
   end=min(start+1048575,size-1)
   for attempt in range(5):
    try:
     req=urllib.request.Request(url+'?chunk='+str(start),headers={'Range':f'bytes={start}-{end}'})
     with urllib.request.urlopen(req,timeout=30) as response:data=response.read()
     assert len(data) in (end-start+1,size)
     return start,data
    except Exception:
     if attempt==4:raise
     time.sleep(attempt+1)
  with dest.open('wb') as out:
   with concurrent.futures.ThreadPoolExecutor(max_workers=6) as pool:
    for start,data in pool.map(chunk,range(0,size,1048576)):
     out.seek(0 if len(data)==size else start);out.write(data)
  assert valid(dest,info)
 proc=subprocess.Popen(['dpkg-deb','--fsys-tarfile',str(dest)],stdout=subprocess.PIPE)
 with tarfile.open(fileobj=proc.stdout,mode='r|') as z:
  for item in z:
   name=Path(item.name);assert not name.is_absolute() and '..' not in name.parts
   target=root/'root'/name
   if item.isdir():target.mkdir(parents=True,exist_ok=True)
   elif item.isfile():
    target.parent.mkdir(parents=True,exist_ok=True);target.unlink(missing_ok=True)
    with z.extractfile(item) as source,target.open('wb') as out:
     while data:=source.read(1048576):out.write(data)
    assert target.stat().st_size==item.size,(target,item.size);target.chmod(item.mode)
   elif item.issym():
    target.parent.mkdir(parents=True,exist_ok=True);target.unlink(missing_ok=True);target.symlink_to(item.linkname)
   else:assert False,('unexpected dependency archive entry',item.name,item.type)
 assert proc.wait()==0
 print('restored',info['package'],flush=True)
for name in ['clang','clang++','ld.lld','llvm-objcopy']:
 target=root/'bin'/name;target.unlink(missing_ok=True);target.symlink_to(root/'root/usr/lib/llvm-18/bin'/name)
env=dict(os.environ,LD_LIBRARY_PATH=str(root/'root/usr/lib/x86_64-linux-gnu'))
subprocess.run([str(root/'bin/clang'),'--version'],check=True,env=env)
smoke=root/'smoke.c';smoke.write_text('int add(int value){return value+2;}\n')
subprocess.run([str(root/'bin/clang'),'--target=arm-none-eabi','-mcpu=arm7tdmi','-marm','-Os','-c',str(smoke),'-o',str(root/'smoke.o')],check=True,env=env)
print('PASS LLVM18 ARM smoke build; add bin to PATH and root/usr/lib/x86_64-linux-gnu to LD_LIBRARY_PATH')
