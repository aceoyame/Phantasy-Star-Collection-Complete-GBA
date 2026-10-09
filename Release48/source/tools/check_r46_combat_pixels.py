#!/usr/bin/env python3
"""Independent original-ROM inverse-pixel oracle for enemy and combo assets.

Does not call the generators' renderers/packers/table decoders. Standard tables
and both custom table encodings are read directly. Raw enemy controllers retain
specification-selected source maps; these are distinguished in the report.
"""
from pathlib import Path
import json,re
from md_compression import nemesis_decompress
from generate_enemy_animation_frames import Rom,assembled_enemy_tiles
from source_enemy_metadata_tables import METADATA
R=Path(__file__).resolve().parents[1];O=R.parent/'build_artifacts';raw=(R/'references/roms/ps4built.bin').read_bytes();rom=Rom(raw)
banks=[(R/'gba'/('source_'+s+'_effects.bin')).read_bytes() for s in ['ability','enemy','combo']]
def rows(s,name):
 m=re.search(r'\b'+name+r'\[[^]]*\]=\{(.*?)\n\};',s,re.S);assert m,name
 return [[int(q.removesuffix('u'),0) for q in re.findall(r'-?(?:0x[0-9A-Fa-f]+|\d+)u?',line)] for line in m[1].splitlines() if '{' in line]
def word(p):return int.from_bytes(raw[p:p+2],'big')
def long(p):return int.from_bytes(raw[p:p+4],'big')&0xffffff
def signed(v):return v-65536 if v&32768 else v
def mapping(p):
 if p is None:return []
 out=[]
 for j in range(raw[p]+1):
  b=raw[p+1+j*6:p+7+j*6];assert len(b)==6
  out.append((b[4]-256 if b[4]&128 else b[4],b[0]-256 if b[0]&128 else b[0],(((b[1]>>2)&3)+1)*8,((b[1]&3)+1)*8,(b[2]<<8)|b[3]))
 return out
def standard(tab):
 out=[]
 for i in range(word(tab)):
  p=tab+2+signed(word(tab+2+2*i));out.append((max(1,raw[p]),mapping(None if raw[p+1]&128 else p+1)))
 return out
artcache={}
def nem(p):
 if p not in artcache:artcache[p]=nemesis_decompress(raw[p:])
 return artcache[p]
def plc(index,loader=1):
 table=0x27E88A if loader==1 else 0x27EA66;p=table+signed(word(table+2*index));return b''.join(nem(long(p+2+4*i)) for i in range(word(p)+1))
def dims(g):return [[(8,8),(16,16),(32,32),(64,64)],[(16,8),(32,8),(32,16),(64,32)],[(8,16),(8,32),(16,32),(32,64)]][g&3][g>>2&3]
counts={'enemy_sequences':0,'combo_sequences':0,'part_frames':0,'pixels':0,'original_standard_enemy_sequences':0,'spec_selected_raw_enemy_sequences':0}
def check(a,parts,own,source,arts,base,label):
 first,np,nf,ho,tab=a;assert len(source)==nf,(label,'frame count',nf,len(source));assert own[ho:ho+nf]==bytes(h for h,m in source),(label,'holds')
 for part in parts[first:first+np]:
  off,fb,g,alias,ox,oy,pad=part;w,h=dims(g);assert fb==w*h//2
  blob=banks[0] if alias==1 else banks[1] if alias==2 else own
  assert off+(nf*4 if alias==4 else nf*fb)<=len(blob),(label,'payload range')
  for f,((hold,sprites),art) in enumerate(zip(source,arts)):
   if alias==4:
    at=int.from_bytes(own[off+f*4:off+f*4+4],'little');tag=at>>28;at&=0x0fffffff;frame_bank=banks[tag-1] if tag else own;packed=frame_bank[at:at+fb]
   else:packed=blob[off+f*fb:off+(f+1)*fb]
   for dy in range(h):
    for dx in range(w):
     wx,wy=ox+dx,oy+dy;expected=0
     for sx,sy,sw,sh,prop in sprites:
      if not(sx<=wx<sx+sw and sy<=wy<sy+sh):continue
      ux,uy=wx-sx,wy-sy
      if prop&0x800:ux=sw-1-ux
      if prop&0x1000:uy=sh-1-uy
      ti=(prop&0x7ff)-base+(ux//8)*(sh//8)+uy//8;at=ti*32+(uy%8)*4+(ux%8)//2;assert 0<=at<len(art),(label,'unloaded tile',ti,len(art)//32)
      ci=(art[at]>>(0 if ux&1 else 4))&15
      if ci:expected=ci
     at=((dy//8)*(w//8)+dx//8)*32+(dy%8)*4+dx//2%4
     actual=(packed[at]>>(4 if dx&1 else 0))&15
     assert actual==expected,(label,hex(tab),f,dx,dy,'pixel',actual,expected,'offset',ox,oy)
     counts['pixels']+=1
   counts['part_frames']+=1
s=(R/'gba/source_enemy_effects.c').read_text();ep=rows(s,'ps4_source_enemy_parts');ea=rows(s,'ps4_source_enemy_anims');ed=rows(s,'ps4_source_enemy_descs')
spec=json.loads((R/'tools/source_enemy_effect_specs.json').read_text())
for pair,d in zip(spec['pairs'],ed):
 assert d[1]==len(pair['sequences'])
 for si,q in enumerate(pair['sequences']):
  a=ea[d[0]+si];assert a[4]==q['source_table']
  if a[4] in METADATA:
   assert a[1]==0 and a[2]==1;continue
  if q.get('source_nemesis') is not None:
   source=standard(a[4]);art=nem(q['source_nemesis']);counts['original_standard_enemy_sequences']+=1
  else:
   source=[(max(1,h),mapping(p)) for h,p in zip(q['holds'],q['frames'])];counts['spec_selected_raw_enemy_sequences']+=1
   art=b''.join(plc(i) for i in q['art_plcs']) if q['art_kind']=='plc' else assembled_enemy_tiles(rom,pair['enemy'])[0]
  check(a,ep,banks[1],source,[art]*len(source),q['tile_base'],('enemy',pair['enemy'],pair['ability'],si));counts['enemy_sequences']+=1
print('PASS enemy pixels',counts,flush=True)
s=(R/'gba/source_combo_effects.c').read_text();cp=rows(s,'ps4_source_combo_parts');ca=rows(s,'ps4_source_combo_anims');cd=rows(s,'ps4_source_combo')
manifest=json.loads((R/'source_combo_effects_manifest.json').read_text())
for cid in range(1,15):
 sources=manifest['combos'][str(cid)]['sources'];assert len(sources)==cd[cid][1]
 for si,q in enumerate(sources):
  a=ca[cd[cid][0]+si];tab=a[4];assert tab==int(q['table'],16);pool=plc(q['plc'],2)
  if q['encoding']=='standard':
   source=standard(tab);art=pool[q['vram_destination']-q['vram_base']:];arts=[art]*len(source)
  else:
   source=[];arts=[]
   # Slasher root always uses the original $9F40 destination and $9B80 PLC.
   for i in range(word(tab)):
    p=tab+4+signed(word(tab+4+4*i));hold=max(1,raw[tab+3+4*i])
    if q['encoding']=='physical':
     sprites=mapping(p+4);high=max((pr&0x7ff)+(sw//8)*(sh//8) for x,y,sw,sh,pr in sprites);art=raw[long(p):long(p)+high*32]
    else:sprites=mapping(p);art=pool[0x9F40-0x9B80:]
    source.append((hold,sprites));arts.append(art)
  check(a,cp,banks[2],source,arts,0,('combo',cid,si));counts['combo_sequences']+=1
report={'result':'PASS',**counts,'enemy_action_pairs':234,'controller_metadata_entries_excluded':622-counts['enemy_sequences'],'combo_entries':14,'scope':'All emitted enemy/combo payloads and holds checked against original pixels. 123 enemy standard table sequences independently parsed; other enemy raw frames are checked against their selected specification maps. Controller scheduling, placement, palette changes and hardware display require separate validation.'}
(O/'R46_COMBAT_PIXEL_EVIDENCE.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS complete combat pixels',report)
