#!/usr/bin/env python3
"""Extract source palette tables; no asset regeneration or hashes."""
from pathlib import Path
import re
R=Path(__file__).resolve().parents[1];s=(R/'references/disassembly/ps4disasm/ps4disasm/ps4.asm').read_text(errors='replace')
rows=[(0x17,'PlateSysF1CyclingPal',28,2,6,'powered'),(0x18,'PlateSysF1CyclingPal2',12,3,6,'powered'),(0x1c,'NurvusB1CyclingPal',12,3,8,'!ps4_event_flag_test(0x66u)'),(0x1d,'NurvusB1CyclingPal2',28,2,6,'!ps4_event_flag_test(0x66u)'),(0x20,'WreckageEngineCyclingPal',28,2,6,'ps4_event_flag_test(0x40u)'),(0x27,'PlateSysCyclingPal',12,2,6,'powered'),(0x28,'NurvusCyclingPal',28,3,8,'!ps4_event_flag_test(0x65u)'),(0x29,'NurvusCyclingPal2',12,2,6,'!ps4_event_flag_test(0x65u)'),(0x35,'VahFortCyclingPal',12,2,6,'!ps4_event_flag_test(0xBBu)&&ps4_event_flag_test(0xAFu)'),(0x36,'VahFortCyclingPal2',28,3,8,'!ps4_event_flag_test(0xBBu)&&ps4_event_flag_test(0xAFu)')]
extra=['GrbrkTwCyclingPal','GrbrkTwCyclingPal2','RykrosCrystalsCyclingPal','LeRoofRoomCyclingPal']
out=['/* Generated from ps4.asm; R30 palette data remains in ROM. */']
for name in [x[1] for x in rows]+extra:
 vals=[]
 for line in s.split('\n'+name+':',1)[1].splitlines():
  line=line.split(';')[0].strip()
  if not line:continue
  m=re.match(r'dc\.[wb]\s+(.*)',line)
  if not m:break
  vals.extend(int(x.strip().replace('$','0x'),0) for x in m[1].split(','))
 assert vals,name
 out.append('static const uint16_t r30_'+name+'[]={'+','.join('0x%04Xu'%x for x in vals)+'};')
out.append('__attribute__((noinline)) static void r30_palette_pair(const uint16_t* p,unsigned dst,uint16_t state,unsigned n){if(native_main_frame_count&7u)return;unsigned q=ram8r(state);if(q>=n)q=0u;field_palette_live_word(dst,p[q]);field_palette_live_word(dst+1u,p[q+6u]);ram8w(state,(uint8_t)(q+1u));}')
out.append('__attribute__((noinline)) static void r30_palette_update(unsigned id){unsigned powered=ps4_event_flag_test(0xDAu)||(!ps4_event_flag_test(0x65u)&&!ps4_event_flag_test(0x61u));switch(id){')
for i,name,dst,state,n,cond in rows:
 out.append('case 0x%02Xu:if(%s)r30_palette_pair(r30_%s,%du,0xECF%Xu,%du);%sbreak;'%(i,cond,name,dst,state,n,'else{field_palette_live_word(28u,0x860u);field_palette_live_word(29u,0x68u);}' if i==0x17 else ''))
out.extend(['case 0x23u:if(!(native_main_frame_count&3u)){unsigned q=(native_main_frame_count>>2)&3u;for(unsigned n=0u;n<3u;n++)field_palette_live_word(10u+n,r30_GrbrkTwCyclingPal[q+n*4u]);}break;',
'case 0x24u:if(!(native_main_frame_count&7u)){unsigned q=(native_main_frame_count>>3)&3u;for(unsigned n=0u;n<3u;n++)field_palette_live_word(17u+n,r30_GrbrkTwCyclingPal2[q+n*4u]);field_palette_live_word(20u,r30_GrbrkTwCyclingPal2[q+10u]);}break;',
'case 0x2Au:{uint8_t d=(uint8_t)(ram8r(0xECF2u)-1u);ram8w(0xECF2u,d);if(d){field_palette_live_word(2u,0xCECu);field_palette_live_word(18u,0xCECu);}else if(!(native_main_frame_count&3u)&&ram8r(0xECF3u)==4u){ram8w(0xECF2u,(uint8_t)ps4_rng2_next());ram8w(0xECF3u,0u);}else{if(!(native_main_frame_count&3u)){unsigned q=ram8r(0xECF3u);if(q>3u)q=0u;field_palette_live_word(2u,r30_RykrosCrystalsCyclingPal[q]);field_palette_live_word(18u,r30_RykrosCrystalsCyclingPal[q]);ram8w(0xECF3u,(uint8_t)(q+1u));}ram8w(0xECF2u,1u);}break;}',
'case 0x2Cu:if(!(native_main_frame_count&7u)){unsigned q=ram8r(0xECF2u);if(q>=14u)q=0u;field_palette_live_word(13u,r30_LeRoofRoomCyclingPal[q]);ram8w(0xECF2u,(uint8_t)(q+1u));}break;', '}}'])
(R/'gba/r30_palette_updates.h').write_text('\n'.join(out)+'\n')
print('R30 authored palette controllers:',len(rows)+len(extra))
