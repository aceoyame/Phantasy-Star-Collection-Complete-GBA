#!/usr/bin/env python3
"""Inventory PSC II/III's logarithmic waves without changing instruments.
Usage: python3 source/tools/audit_psc_native_audio.py [--export-waves DIRECTORY]
Exports are decoded signed-16 samples of native assets, not Genesis reference.
"""
from pathlib import Path
import struct,json,argparse,hashlib,wave,math
R=Path(__file__).resolve().parents[2]
a=argparse.ArgumentParser();a.add_argument('--export-waves',type=Path);arg=a.parse_args()
b=(R/'donors/PSC_R15_shell_8MiB.gba').read_bytes()
# PSIII's note opcode uses the table pointer in the literal pool at 0x829A0.
iii=struct.unpack_from('<I',b,0x829A0)[0]-0x08000000
# PSII's note opcode uses 256 records of length/pointer, rather than III's
# repeat/envelope records. Its literal lives at 0x395014.
ii=struct.unpack_from('<I',b,0x395014)[0]-0x08000000
assert struct.unpack_from('<I',b,ii+4)[0]-0x08000000==ii+256*8
# Original native mixer builds this LUT with signed entries permuted to byte order.
lookup_offsets={'III':0x269af0,'II':struct.unpack_from('<I',b,0x3953ec)[0]-0x08000000}
lookups={name:[struct.unpack_from('<i',b,off+((q+128)&255)*4)[0] for q in range(256)] for name,off in lookup_offsets.items()}
rows=[];payloads={};unique={};games=[]
for name,off,stride in [('II',ii,8),('III',iii,60)]:
 game=[]
 for i in range(256):
  if name=='III':
   count,ptr,phase=struct.unpack_from('<III',b,off+i*stride);length=(phase>>14)*count
  else:
   count,ptr=struct.unpack_from('<II',b,off+i*stride);phase=None;length=count
  ptr-=0x08000000
  if count>=0x80000000:
   rows.append(dict(game=name,index=i,record_offset=hex(off+i*stride),wave_offset=hex(ptr),bytes=None,encoding='unused sentinel' if count==0xFFFFFFFF else 'special encoded sample; decoder still needs audit'));continue
  data=b[ptr:ptr+length];key=hashlib.sha256(data).hexdigest();unique.setdefault(key,data)
  row=dict(game=name,index=i,record_offset=hex(off+i*stride),wave_offset=hex(ptr),bytes=length,repeat_count=count,loop_phase_q14=phase,duplicate_of=payloads.get(key))
  payloads.setdefault(key,f'{name}:{i}');rows.append(row);game.append((ptr,length))
  if arg.export_waves:
   dest=arg.export_waves/name;dest.mkdir(parents=True,exist_ok=True)
   with wave.open(str(dest/f'wave_{i:03}.wav'),'wb') as w:
    w.setnchannels(1);w.setsampwidth(2);w.setframerate(21024)
    w.writeframes(struct.pack('<'+str(length)+'h',*(lookups[name][v] for v in data)))
 games.append({'game':name,'table_offset':hex(off),'records':256,'record_stride_bytes':stride,'wave_start':hex(min(p for p,n in game)),'wave_end_exclusive':hex(max(p+n for p,n in game)),'logical_wave_bytes':sum(n for p,n in game)})
report={'scope':'Native PSC positive-count logarithmic assets and note-handler layout; unused/special encoded rows excluded from decoding. Not a Genesis reference render; no source data/pointers mutated.','encoding':'signed logarithmic byte; attenuation subtracts magnitude before signed exponential lookup','six_voices':True,'phase_format':'Q14, nearest sample in native kernel','record_stride_bytes':{'II':8,'III':60},'games':games,'wave_rows':rows,'identical_payloads_across_all_rows':sum('duplicate_of' in x for x in rows)-len(unique),'candidate_duplicate_payload_bytes':sum(x['bytes'] for x in rows if x.get('duplicate_of')),'dedup_allocations_not_reclaimed':True,'lookup_rom_offset':{k:hex(v) for k,v in lookup_offsets.items()},'lookup_iwram':'0x03002000','native_pcm_buffer_iwram':'0x03000840','III_native_arm_mix_rom':'0x08083140','III_native_arm_phase_rom':'0x080831C8','II_native_arm_mix_rom':'0x08395530','II_native_arm_phase_rom':'0x083955B8'}
(R/'build_artifacts/RELEASE48_NATIVE_AUDIO_INVENTORY.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS native wave inventory',games,'candidate duplicate bytes',report['candidate_duplicate_payload_bytes'])
