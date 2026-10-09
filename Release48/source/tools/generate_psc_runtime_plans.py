#!/usr/bin/env python3
"""Generate PSC-style native runtime plans for PSIV map managers/updates and manga panels.

This intentionally does not duplicate manager/update semantics: it compiles the authored
per-map ID lists so runtime calls the existing source-lowered dispatcher bodies without
walking Genesis sentinel tables.  Manga panel art/map projection is immutable, so panels
are fully rendered to transparent 240x160 overlay spans and palette-line patches on the
host while preserving stack/compositing behavior at runtime.
"""
from __future__ import annotations
import argparse, hashlib, json, re, struct
from pathlib import Path
from md_compression import nemesis_decompress, enigma_decompress
EXPECTED='511f35cc11f88316f8b8940e28ab298bd75a4da193672a80172884d6eb913b6a'

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--rom',required=True,type=Path)
    ap.add_argument('--runtime',default=Path('gba/title_runtime_v06.c'),type=Path)
    ap.add_argument('--out-plan-c',default=Path('gba/psc_runtime_plan.c'),type=Path)
    ap.add_argument('--out-plan-h',default=Path('gba/psc_runtime_plan.h'),type=Path)
    ap.add_argument('--out-panel-bin',default=Path('gba/psc_panel_bank.bin'),type=Path)
    ap.add_argument('--out-panel-c',default=Path('gba/psc_panel_bank.c'),type=Path)
    ap.add_argument('--out-panel-h',default=Path('gba/psc_panel_bank.h'),type=Path)
    ap.add_argument('--out-panel-s',default=Path('gba/psc_panel_bank_data.s'),type=Path)
    ap.add_argument('--report',default=Path('generated/psc_runtime_plan_report.json'),type=Path)
    a=ap.parse_args()
    r=a.rom.read_bytes(); sha=hashlib.sha256(r).hexdigest()
    if len(r)!=0x300000 or sha!=EXPECTED: raise SystemExit(f'wrong PSIV ROM {len(r):#x} {sha}')
    def u8(p): return r[p]
    def u16(p): return int.from_bytes(r[p:p+2],'big')
    def u32(p): return int.from_bytes(r[p:p+4],'big')
    def rec(p,n):
        while u16(p)!=0xffff:p+=n
        return p+2
    def ff(p):
        while u8(p)!=0xff:p+=1
        return p+1

    # Derive the actually implemented RunMapUpdates IDs from the runtime switch.
    src=a.runtime.read_text()
    anchor='static void field_apply_map_update_id' if 'static void field_apply_map_update_id' in src else 'static void field_run_map_updates'
    sec=src[src.index(anchor):src.index('static int ps4_load_field_map')]
    active_update_ids={int(x,0) for x in re.findall(r'case\s+(0x[0-9A-Fa-f]+|\d+)u:',sec)}
    active_update_ids.discard(0)

    err=u32(8); mgr_flat=[]; upd_flat=[]; event_flat=[]; trans_flat=[]; plans=[]; valid_maps=0; authored_updates=0
    authored_events=0; authored_transitions=0
    for mid in range(0x1a1):
        base=u32(0x100000+mid*4)
        if base==err or base>=len(r):
            plans.append(dict(valid=0,manager_offset=0,manager_count=0,update_offset=0,update_count=0,event_offset=0,event_count=0,trans1_offset=0,trans1_count=0,trans2_offset=0,trans2_count=0,managers=[],updates=[],authored_updates=[],events=[],trans1=[],trans2=[]));continue
        valid_maps+=1;p=base+2
        while u16(p)<0x8000:p+=6
        p+=2
        while True:
            w=u16(p)
            if w==0xfffe:p+=8;continue
            if w&0x8000:p+=2;break
            p+=6
        while u16(p)!=0xffff:p+=6
        p+=2;p+=6
        for _ in range(2):
            mode=u16(p);p+=2
            if not mode:p+=8
        while u16(p)!=0xffff:p+=4
        p+=2
        up=[]
        while u8(p)!=0xff:up.append(u8(p));p+=1
        p+=1; authored_updates+=len(up)
        transitions=[]
        for _ in range(2):
            cur=[]
            while u16(p)!=0xffff:
                cur.append((u8(p)*16,u8(p+1)*16,u16(p+2),u16(p+4),u8(p+6)*16,u8(p+7)*16,(u8(p+8)>>2)&3,u8(p+9)))
                p+=10
            p+=2;transitions.append(cur);authored_transitions+=len(cur)
        p=rec(p,10);p=rec(p,6);p=rec(p,2)
        if mid>=2:p+=8
        p+=4
        while u16(p)!=0xffff:p+=2
        p+=2
        events=[]
        while u8(p)!=0xff:events.append(u8(p));p+=1
        p+=1;authored_events+=len(events)
        p+=4;p+=4
        managers=[]
        while u16(p)!=0xffff: managers.append(u16(p));p+=2
        mo=len(mgr_flat);mgr_flat.extend(managers)
        active=[x for x in up if x in active_update_ids]
        uo=len(upd_flat);upd_flat.extend(active)
        eo=len(event_flat);event_flat.extend(events)
        t1o=len(trans_flat);trans_flat.extend(transitions[0])
        t2o=len(trans_flat);trans_flat.extend(transitions[1])
        plans.append(dict(valid=1,manager_offset=mo,manager_count=len(managers),update_offset=uo,update_count=len(active),event_offset=eo,event_count=len(events),trans1_offset=t1o,trans1_count=len(transitions[0]),trans2_offset=t2o,trans2_count=len(transitions[1]),managers=managers,updates=active,authored_updates=up,events=events,trans1=transitions[0],trans2=transitions[1]))

    a.out_plan_h.write_text(
        '#ifndef PS4_PSC_RUNTIME_PLAN_H\n#define PS4_PSC_RUNTIME_PLAN_H\n#include <stdint.h>\n'
        'typedef struct Ps4PscTransition {\n uint16_t tx,ty,kind,map_id,x,y;\n uint8_t facing,align;\n} Ps4PscTransition;\n'
        'typedef struct Ps4PscRuntimePlan {\n uint16_t manager_offset,update_offset,event_offset,trans1_offset,trans2_offset;\n'
        ' uint8_t manager_count,update_count,event_count,trans1_count,trans2_count,valid;\n} Ps4PscRuntimePlan;\n'
        'extern const uint8_t ps4_psc_manager_ids[];\nextern const uint8_t ps4_psc_update_ids[];\nextern const uint8_t ps4_psc_event_ids[];\n'
        'extern const Ps4PscTransition ps4_psc_transitions[];\nconst Ps4PscRuntimePlan* ps4_psc_runtime_plan_get(uint16_t map_id);\n#endif\n')
    lines=['/* AUTO-GENERATED PSC-style map manager/update/event/transition plans. */\n#include "psc_runtime_plan.h"']
    lines.append('const uint8_t ps4_psc_manager_ids[]={'+','.join(f'0x{x:02X}u' for x in mgr_flat)+'};')
    lines.append('const uint8_t ps4_psc_update_ids[]={'+','.join(f'0x{x:02X}u' for x in upd_flat)+'};')
    lines.append('const uint8_t ps4_psc_event_ids[]={'+','.join(f'0x{x:02X}u' for x in event_flat)+'};')
    lines.append('const Ps4PscTransition ps4_psc_transitions[]={')
    for tx,ty,kind,map_id,x,y,facing,align in trans_flat:
        lines.append(f'{{{tx}u,{ty}u,{kind}u,0x{map_id:04X}u,{x}u,{y}u,{facing}u,0x{align:02X}u}},')
    lines.append('};')
    lines.append('static const Ps4PscRuntimePlan plans[0x1A1]={')
    for mid,pn in enumerate(plans):
        lines.append(f'{{{pn["manager_offset"]}u,{pn["update_offset"]}u,{pn["event_offset"]}u,{pn["trans1_offset"]}u,{pn["trans2_offset"]}u,{pn["manager_count"]}u,{pn["update_count"]}u,{pn["event_count"]}u,{pn["trans1_count"]}u,{pn["trans2_count"]}u,{pn["valid"]}u}}, /* {mid:03X} */')
    lines.append('};\nconst Ps4PscRuntimePlan* ps4_psc_runtime_plan_get(uint16_t map_id){if(map_id>=0x1A1u||!plans[map_id].valid)return 0;return &plans[map_id];}\n')
    a.out_plan_c.write_text('\n'.join(lines))

    # Manga overlays.  The runtime's fixed projection is floor(x*320/240), floor(y*224/160).
    sx=[(x*320)//240 for x in range(240)]; sy=[(y*224)//160 for y in range(160)]
    cvt=(0,4,9,13,18,22,27,31)
    def md2gba(c):
        rr=(c>>1)&7;gg=(c>>5)&7;bb=(c>>9)&7
        return cvt[rr]|(cvt[gg]<<5)|(cvt[bb]<<10)
    stream_blob=bytearray(); pal_records=[]; panel_desc=[]; valid_panels=0; total_runs=0; total_pixels=0
    for pid in range(256):
        group=pid>>6;base=u32(0x6B2B6+group*4)
        if base>=len(r): panel_desc.append(None);continue
        q=base+(pid&63)*0x1e;dest=u16(q);artptr=u32(q+2);w=u8(q+6);h=u8(q+7);px=u8(q+8)*8;py=u8(q+9)*8
        if not w or not h or artptr>=len(r): panel_desc.append(None);continue
        try: art=nemesis_decompress(r[artptr:])
        except Exception: panel_desc.append(None);continue
        fb=bytearray(240*160); pal_lines={}
        def layer(base_tile,palptr,mapptr):
            if base_tile==0xffff or not mapptr:return
            if mapptr>=len(r) or palptr+31>=len(r):raise SystemExit(f'bad panel {pid:02X} layer ptr')
            pl=(base_tile>>13)&3
            pal_lines[pl]=[md2gba(u16(palptr+i*2)) for i in range(16)]
            cells=enigma_decompress(r[mapptr:],base_tile);need=min(len(cells),w*h);x1=px+w*8;y1=py+h*8
            for dy in range(160):
                yy=sy[dy]
                if yy<py or yy>=y1:continue
                ly=yy-py;ty=ly>>3;iy=ly&7
                row=dy*240
                for dx in range(240):
                    xx=sx[dx]
                    if xx<px or xx>=x1:continue
                    lx=xx-px;tx=lx>>3;ix=lx&7;pos=ty*w+tx
                    if pos>=need:continue
                    e=cells[pos]&0xffff;x=7-ix if e&0x0800 else ix;y=7-iy if e&0x1000 else iy;src=e&0x07ff
                    if src<dest:continue
                    off=(src-dest)*32+y*4+(x>>1)
                    if off>=len(art):continue
                    v=art[off];ci=(v&15) if x&1 else (v>>4)
                    if ci:fb[row+dx]=((e>>13)&3)*16+ci
        # Preserve runtime ordering: B first, A second; later palette writes win.
        layer(u16(q+20),u32(q+22),u32(q+26));layer(u16(q+10),u32(q+12),u32(q+16))
        so=len(stream_blob);runs=0;pix=0
        for y in range(160):
            x=0;row=y*240
            while x<240:
                while x<240 and fb[row+x]==0:x+=1
                while x<240 and fb[row+x]!=0:
                    st=x
                    while x<240 and fb[row+x]!=0 and x-st<255:x+=1
                    ln=x-st;stream_blob.extend(struct.pack('<HB',row+st,ln));stream_blob.extend(fb[row+st:row+x]);runs+=1;pix+=ln
        sl=len(stream_blob)-so;pm=0;po=len(pal_records)
        for pl in range(4):
            if pl in pal_lines:
                pm|=1<<pl;pal_records.extend(pal_lines[pl])
        panel_desc.append(dict(valid=1,stream_offset=so,stream_length=sl,palette_word_offset=po,palette_mask=pm,runs=runs,pixels=pix))
        valid_panels+=1;total_runs+=runs;total_pixels+=pix
    while len(stream_blob)&3:stream_blob.append(0)
    pal_byte_offset=len(stream_blob)
    for v in pal_records:stream_blob.extend(struct.pack('<H',v))
    while len(stream_blob)&3:stream_blob.append(0)
    a.out_panel_bin.write_bytes(stream_blob)
    a.out_panel_h.write_text('''#ifndef PS4_PSC_PANEL_BANK_H\n#define PS4_PSC_PANEL_BANK_H\n#include <stdint.h>\ntypedef struct Ps4PscPanelDesc {\n uint32_t stream_offset; uint16_t stream_length,palette_word_offset; uint8_t palette_mask,valid;\n} Ps4PscPanelDesc;\nextern const uint8_t ps4_psc_panel_bank[];\nextern const uint32_t ps4_psc_panel_palette_byte_offset;\nconst Ps4PscPanelDesc* ps4_psc_panel_get(uint16_t panel_id);\n#endif\n''')
    cl=['''/* AUTO-GENERATED PSC-style preprojected manga panel descriptors. */\n#include "psc_panel_bank.h"\nconst uint32_t ps4_psc_panel_palette_byte_offset='''+str(pal_byte_offset)+'u;\nstatic const Ps4PscPanelDesc panels[256]={']
    for pid,d in enumerate(panel_desc):
        if d is None:cl.append('{0u,0u,0u,0u,0u},')
        else:cl.append(f'{{{d["stream_offset"]}u,{d["stream_length"]}u,{d["palette_word_offset"]}u,0x{d["palette_mask"]:02X}u,1u}},')
    cl.append('};\nconst Ps4PscPanelDesc* ps4_psc_panel_get(uint16_t panel_id){if(panel_id>=256u||!panels[panel_id].valid)return 0;return &panels[panel_id];}\n')
    a.out_panel_c.write_text('\n'.join(cl))
    a.out_panel_s.write_text('''    .section .rodata.psc_panel_bank,"a",%progbits\n    .balign 4\n    .global ps4_psc_panel_bank\n    .type ps4_psc_panel_bank, %object\nps4_psc_panel_bank:\n    .incbin "gba/psc_panel_bank.bin"\n    .size ps4_psc_panel_bank, .-ps4_psc_panel_bank\n''')
    a.report.parent.mkdir(parents=True,exist_ok=True)
    report=dict(valid_maps=valid_maps,manager_entries=len(mgr_flat),authored_update_entries=authored_updates,active_update_entries=len(upd_flat),active_update_ids=sorted(active_update_ids),event_entries=authored_events,transition_entries=authored_transitions,plans=plans,valid_panels=valid_panels,panel_runs=total_runs,panel_pixels=total_pixels,panel_bank_bytes=len(stream_blob),panel_palette_byte_offset=pal_byte_offset,panel_desc=panel_desc)
    a.report.write_text(json.dumps(report,indent=2)+'\n')
    print(f'PASS PSC runtime plans: maps={valid_maps} managers={len(mgr_flat)} updates={len(upd_flat)}/{authored_updates} events={authored_events} transitions={authored_transitions} panels={valid_panels} runs={total_runs} pixels={total_pixels} panel_bank={len(stream_blob)}')
if __name__=='__main__':main()
