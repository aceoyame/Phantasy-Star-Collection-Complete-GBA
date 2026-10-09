#!/usr/bin/env python3
"""Select the preserved waves and matching player files, never unknown edits."""
import argparse,json,shutil,zlib,gzip
from pathlib import Path
R=Path(__file__).resolve().parents[2]
ap=argparse.ArgumentParser();ap.add_argument('--bank',choices=['release03','reference04','reference05'],required=True);ap.add_argument('--apply',action='store_true',help='Actually copy the five verified profile files. Default only checks and reports.');a=ap.parse_args()
manifest=json.loads((R/'AUDIO_BANK_PROFILES.json').read_text());selected=manifest['profiles'][a.bank]
def payload(record):
 data=(R/record['file']).read_bytes()
 return gzip.decompress(data) if record.get('compression')=='gzip' else data
for target,record in selected['files'].items():
 src=R/record['file'];dst=R/target
 assert src.is_file() and dst.is_file(),target
 assert len(payload(record))==record['bytes'] and f'{zlib.crc32(payload(record)):08x}'==record['crc32'],('profile integrity',target)
 current=f'{zlib.crc32(dst.read_bytes()):08x}'
 allowed={profile['files'][target]['crc32'] for profile in manifest['profiles'].values()}
 assert current in allowed,('Unrecognized audio edits; refusing to overwrite',target,current)
if a.apply:
 for target,record in selected['files'].items():(R/target).write_bytes(payload(record))
 for target,record in selected['files'].items():assert f'{zlib.crc32((R/target).read_bytes()):08x}'==record['crc32']
print(json.dumps({'bank':a.bank,'applied':a.apply,'verified_files':list(selected['files']),'next':'Rebuild with python3 source/build_r46.py after applying; boot fresh or use a ROM-CRC-matched state. Do not restore an old state into a newly built ROM.'},indent=2))
