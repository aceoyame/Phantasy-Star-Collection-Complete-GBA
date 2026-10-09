#!/usr/bin/env python3
"""Correct the selected enemy animation tables and bank-local source art.

Reuses byte-identical ability payloads by part.reserved=1. Every other payload
is retained or re-lowered from original mappings; no audio/pose/donor changes.
"""
from pathlib import Path
import argparse,json,re
from md_compression import nemesis_decompress
from generate_enemy_attack_frames import bounds,split,legal
from generate_source_ability_effects import render_indices,pack_canvas,obj_part_meta
from audit_r46_attack_catalog import audit,rom,source_bank,R,STANDARD
from source_enemy_metadata_tables import METADATA
from generate_source_enemy_effects import art_pool,render_indices as enemy_render,mapbytes

O=R.parent/'build_artifacts'
ap=argparse.ArgumentParser();ap.add_argument('--apply',action='store_true');args=ap.parse_args()
sources,source_audit=audit();assert not source_audit['unresolved_standard_banks']
path=R/'gba/source_enemy_effects.c';text=path.read_text();old=(R/'gba/source_enemy_effects.bin').read_bytes()
ability=(R/'gba/source_ability_effects.bin').read_bytes()
def parse(text,name):
    m=re.search(r'\b'+name+r'\[[^]]*\]=\{(.*?)\n\};',text,re.S);assert m,name
    return m,[[int(q.removesuffix('u'),0) for q in re.findall(r'-?(?:0x[0-9A-Fa-f]+|\d+)u?',line)] for line in m[1].splitlines() if '{' in line]
pm,old_parts=parse(text,'ps4_source_enemy_parts');am,old_anims=parse(text,'ps4_source_enemy_anims');dm,descs=parse(text,'ps4_source_enemy_descs')
spec=json.loads((R/'tools/source_enemy_effect_specs.json').read_text())
assert len(old_anims)==622 and len(descs)==234
bank=bytearray();cache={};aliases={};parts=[];anims=[];changes=[];registry=[]
def put(data):
    data=bytes(data)
    if data in cache:return cache[data]
    while len(bank)%4:bank.append(0)
    off=len(bank);bank.extend(data);cache[data]=off;return off
def put_part(data):
    key=bytes(data)
    if key not in aliases:
        at=ability.find(key)
        while at>=0 and at%4:at=ability.find(key,at+1)
        aliases[key]=at
    at=aliases[key]
    return (at,1) if at>=0 else (put(key),0)
metadata=[];raw_changes=[]
arts={}
for pi,(pair,d) in enumerate(zip(spec['pairs'],descs)):
    old_first,n=d[:2];assert n==len(pair['sequences']);d[0]=len(anims)
    for si,s in enumerate(pair['sequences']):
        a=old_anims[old_first+si];first,np,nf,hoff,table=a
        assert table==s['source_table']
        native_holds=old[hoff:hoff+nf];first_new=len(parts)
        if table in METADATA:
            anims.append([first_new,0,1,put(bytes([1])),table])
            metadata.append({'enemy':pair['enemy'],'ability':pair['ability'],'sequence':si,'table':f'{table:06X}','data_kind':METADATA[table],'old_parts':np})
            continue
        if (pi,si) in sources:
            pointer,maps,holds=source_bank(table,s['art_plcs'])
            if pointer not in arts:arts[pointer]=nemesis_decompress(rom.tail(pointer))
            art=arts[pointer];vis=[m for m in maps if m]
            bs=[bounds(m) for m in vis];u=(min(b[0] for b in bs),min(b[1] for b in bs),max(b[2] for b in bs),max(b[3] for b in bs))
            regs=split(u);live=0
            for reg in regs:
                w,h=legal(reg[2]-reg[0],reg[3]-reg[1]);live+=w*h//2
                cx,cy=(reg[0]+reg[2])//2,(reg[1]+reg[3])//2
                geom,ox,oy=obj_part_meta(w,h,cx,cy)
                frames=[]
                for m in maps:
                    if m is None:frames.append(bytes(w*h//2));continue
                    pix,sw,sh=render_indices(art,m,(ox,oy,ox+w,oy+h));frames.append(pack_canvas(pix,w,h,w,h))
                off,shared=put_part(b''.join(frames));cx,cy=(reg[0]+reg[2])//2,(reg[1]+reg[3])//2
                geom,ox,oy=obj_part_meta(w,h,cx,cy);parts.append([off,w*h//2,geom,shared,ox,oy,0])
            assert live<=0x4000 and len(regs)*(5 if d[3]==2 else 1)<=48,(pi,si,live)
            hbytes=bytes(max(1,h) for h in holds)
            changes.append({'enemy':f'{pair["enemy"]:02X}','ability':f'{pair["ability"]:02X}','sequence':si,'table':f'{table:06X}','old_frames':nf,'new_frames':len(maps),'old_ticks':sum(native_holds),'new_ticks':sum(hbytes),'nemesis':f'{pointer:06X}'})
            registry.append({'pair':pi,'sequence':si,'table':f'{table:06X}','source_nemesis':f'{pointer:06X}'})
            anims.append([first_new,len(regs),len(maps),put(hbytes),table])
        else:
            art=art_pool(rom,pair['enemy'],s);maps=[None if fp is None else mapbytes(rom,fp) for fp in s['frames']]
            for p in old_parts[first:first+np]:
                original=(ability if p[3]==1 else old)[p[0]:p[0]+p[1]*nf]
                geom,ox,oy=p[2],p[4],p[5]
                w,h=[[(8,8),(16,16),(32,32),(64,64)],[(16,8),(32,8),(32,16),(64,32)],[(8,16),(8,32),(16,32),(32,64)]][geom&3][geom>>2&3]
                frames=[]
                for m in maps:
                    if m is None:frames.append(bytes(p[1]));continue
                    pix,sw,sh=enemy_render(art,m,(ox,oy,ox+w,oy+h),s['tile_base']);frames.append(pack_canvas(pix,w,h,w,h))
                payload=b''.join(frames);assert len(payload)==len(original)
                if payload!=original:raw_changes.append({'enemy':pair['enemy'],'ability':pair['ability'],'sequence':si,'table':f'{table:06X}','part':len(parts),'reason':'Align exact original pixels to emitted OBJ offsets'})
                off,shared=put_part(payload);q=p[:];q[0]=off;q[3]=shared;parts.append(q)
            anims.append([first_new,np,nf,put(native_holds),table])
        assert len(anims)==old_first+si+1
    assert d[1]==len(anims)-d[0]
assert len(anims)==622
report={'result':'DRY_RUN' if not args.apply else 'PASS','old_bytes':len(old),'new_bytes':len(bank),
        'animation_records':len(anims),'part_records':len(parts),'source_table_sequences_corrected':len(changes),
        'one_tick_table_errors_corrected':sum(q['old_frames']==1 and q['new_frames']>1 for q in changes),
        'parts_reusing_ability_bank':sum(p[3]==1 for p in parts),'source_registry':registry,'changes':changes,'non_sprite_metadata_removed':metadata,'raw_part_alignment_changes':raw_changes,
        'scope':'Selected standard enemy tables use signed offsets, original holds and their own Nemesis bank. Raw sprite pixels align to their original mappings; controller metadata is excluded from sprite rendering. Controller spawn/target/overlap timing is separately audited.'}
if args.apply:
    bodies=[(pm,'\n'+'\n'.join('{%du,%du,%du,%du,%d,%d,%du},'%tuple(p) for p in parts)),
            (am,'\n'+'\n'.join('{%du,%du,%du,%du,0x%06Xu},'%tuple(a) for a in anims)),
            (dm,'\n'+'\n'.join('{'+','.join(str(v)+'u' for v in d)+'},' for d in descs))]
    for m,b in sorted(bodies,key=lambda q:q[0].start(1),reverse=True):text=text[:m.start(1)]+b+text[m.end(1):]
    path.write_text(text);(R/'gba/source_enemy_effects.bin').write_bytes(bank)
    (O/'R46_ENEMY_TABLE_REPAIR.json').write_text(json.dumps(report,indent=2)+'\n')
else:(O/'R46_ENEMY_TABLE_REPAIR_DRY_RUN.json').write_text(json.dumps(report,indent=2)+'\n')
print('Enemy table repair:',len(changes),'sequences;',len(old),'->',len(bank),'bytes;',report['parts_reusing_ability_bank'],'shared parts')
