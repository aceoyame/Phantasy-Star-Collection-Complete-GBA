#!/usr/bin/env python3
"""Source-header SFX ownership, priority, PCM and snapshot restoration oracle.

Uses the actual C stream and accepted PCM bank. Only MMIO and the music synth
are host stubs; this does not certify audible timbre or the complete transport.
"""
from pathlib import Path
import argparse,json,os,subprocess,sys
R=Path(__file__).resolve().parents[1];O=R.parent/'build_artifacts'
ap=argparse.ArgumentParser();ap.add_argument('--label',required=True)
ap.add_argument('--source',type=Path);ap.add_argument('--expect-baseline',action='store_true');a=ap.parse_args()
sys.path.insert(0,str(R/'tools'))
from generate_source_sfx_chip_headers import parse_one,be32
rom=(R/'references/roms/ps4built.bin').read_bytes();records=[]
for sid in range(0xb5,0xfb):
 table=0xde4b6 if sid<0xf8 else 0xde5c2;first=0xb5 if sid<0xf8 else 0xf8
 records.append(parse_one(rom,sid,be32(rom,table+4*(sid-first))))
channels={0:2,1:4,2:8,4:16,5:32,0x80:64,0xa0:128,0xc0:256,0xe0:256}
masks=[sum(set(channels[t['channel']] for t in rec['tracks'])) for rec in records]
assert all(rec['tracks'][0]['channel']==4 and len(rec['tracks'])==1 for rec in records[-3:])
asm=(R/'references/disassembly/ps4disasm/ps4disasm/sound/ps4.sound_driver.asm').read_text()
for region,needles in [('PlaySpecialSFX:', ['bset\t#2, $100(a6)']),('loc_D088C:', ['bset\t#2, $370(a6)']),('loc_D09A8:', ['cmpi.b\t#4, d3','lea\t$340(a6), a5']),('StopSpcSFX:', ['btst\t#2, (a5)','lea\t$100(a6), a5'])]:
 start=asm.index(region)
 for needle in needles:assert needle in asm[start:start+1600],(region,needle)
production=(a.source or R/'gba/ps4_audio_stream.c').read_text()
production=production.replace('#define REG16(a) (*(volatile uint16_t*)(a))','#define REG16(a) hw16[((a)-0x04000000u)/2u]')
production=production.replace('#define REG32(a) (*(volatile uint32_t*)(a))','#define REG32(a) hw32[((a)-0x04000000u)/4u]')
prefix='''#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
static volatile uint16_t hw16[0x400];static volatile uint32_t hw32[0x200];
'''
main=r'''
static uint16_t observed_mask;static unsigned music_live,music_ticks;
int ps4_audio_fallback_play(uint8_t id){(void)id;music_live=1;return 1;}
void ps4_audio_fallback_stop(void){music_live=0;}
int ps4_audio_fallback_playing(void){return music_live;}
uint8_t ps4_audio_fallback_current(void){return 0x95;}
void ps4_audio_fallback_prepare_528(int8_t*out){memset(out,0,528);}
void ps4_audio_fallback_commit(void){}
void ps4_audio_fallback_discard(void){}
void ps4_audio_fallback_render_224(int8_t*out){memset(out,0,224);music_ticks++;}
void ps4_audio_fallback_set_mute_mask(uint16_t mask){observed_mask=mask;}
void ps4_audio_irq_entry(void){}
void ps4_irq_cpu_enable(void){}
static const unsigned source_masks[70]={MASKS};
static void command(unsigned id){assert(audio_sound_index_locked(id));}
static unsigned normal_owner(unsigned id){const ps4_sfx_desc_t*d=sfx_desc(id);return d->length&&!(d->flags&PS4_SFX_FLAG_SILENT)?source_masks[id-0xb5]:0;}
static unsigned cases,frames,words;
static void fresh(void){initialized=irq_mode=1;stop_all_state();command(0x95);assert(observed_mask==0);}
static void frame(unsigned nid,unsigned sid,unsigned start){
 struct {int8_t lead[8],samples[528],tail[8];} out;memset(&out,0x5a,sizeof(out));
 const ps4_sfx_desc_t*n=sfx_desc(nid),*s=sfx_desc(sid);unsigned ownership=normal_owner(nid);
 int8_t expected[528];
 for(unsigned j=0;j<528;j++){
  int q=ps4_sfx_bank_start[s->offset+(start+j)%s->length];
  int p=ownership&&j<n->length?ps4_sfx_bank_start[n->offset+j]:0;
  expected[j]=(int8_t)(ownership&16?p:ownership?((p>>1)+(q>>1)):q);
 }
 music_active=0;special_pos=start;ps4_audio_mix_one_frame(out.samples);
 assert(!memcmp(out.samples,expected,528));
 assert(special_pos==(start+528)%s->length);
 for(unsigned j=0;j<8;j++)assert(out.lead[j]==0x5a&&out.tail[j]==0x5a);
 frames++;words+=528;
}
int main(void){
 initialized=irq_mode=1;
 if(BASELINE){
  fresh();command(0xf9);unsigned lost_owner=observed_mask!=16;
  command(0xbd);music_active=0;int8_t out[528];ps4_audio_mix_one_frame(out);
  const ps4_sfx_desc_t*n=sfx_desc(0xbd);unsigned bad=0;
  for(unsigned j=0;j<528;j++)bad+=out[j]!=ps4_sfx_bank_start[n->offset+j];
  assert(lost_owner&&bad);printf("Baseline defects reproduced: special mask=%u, overlapping PCM mismatch=%u/528\n",lost_owner?0:16,bad);return 0;
 }
 for(unsigned sid=0xf8;sid<=0xfa;sid++)for(unsigned nid=0xb5;nid<=0xf7;nid++)for(unsigned order=0;order<2;order++){
  fresh();unsigned mask=normal_owner(nid);
  if(order){command(nid);command(sid);}else{command(sid);command(nid);}
  assert(observed_mask==(16|mask));assert(current_music_id==0x95&&music_active);
  assert(source_masks[nid-0xb5]==ps4_sfx_music_mute_mask[nid-0xb5]);
  frame(nid,sid,sfx_desc(sid)->length-211);
  audio_state_t saved;audio_state_capture(&saved);
  stop_normal_state();stop_special_state();assert(observed_mask==0);
  audio_state_restore(&saved);assert(observed_mask==(16|mask));
  assert(special_id==sid&&special_pos==317);
  command(0);command(0x80);assert(observed_mask==(16|mask));
  command(0xfc);assert(!normal_active&&special_active&&observed_mask==16);
  command(nid);assert(observed_mask==(16|mask));
  command(0xfd);assert(!special_active&&observed_mask==mask);
  command(0xfc);assert(observed_mask==0);
  command(sid);command(nid);command(0x84);
  assert(!normal_active&&!special_active&&observed_mask==0&&current_music_id==0x84);
  command(sid);command(nid);command(0xfb);
  assert(!normal_active&&!special_active&&observed_mask==0&&fade_active);
  command(0xfe);assert(!music_active&&!normal_active&&!special_active&&observed_mask==0);
  if(mask){
   fresh();command(sid);command(nid);music_active=0;
   normal_pos=normal_len-7;special_pos=special_len-3;int8_t out[528];ps4_audio_mix_one_frame(out);
   assert(!normal_active&&special_active&&observed_mask==16&&special_pos==525);
   const ps4_sfx_desc_t*s=sfx_desc(sid);int8_t expected[528];
   for(unsigned j=0;j<528;j++)expected[j]=ps4_sfx_bank_start[s->offset+525+j];
   ps4_audio_mix_one_frame(out);assert(!memcmp(out,expected,528));
   assert(special_pos==1053&&observed_mask==16);frames+=2;words+=1056;
  }
  cases++;
 }
 printf("PASS %u source ownership/priority/stop/restore cases, %u PCM blocks, %u samples\n",cases,frames,words);
}
'''.replace('MASKS',','.join(map(str,masks))).replace('BASELINE','1' if a.expect_baseline else '0')
probe=O/f'R46_{a.label}_SFX_OWNERSHIP.c';probe.write_text(prefix+production+main)
bank=O/f'R46_{a.label}_SFX_BANK.S';bank.write_text('.section .rodata\n.global ps4_sfx_bank_start\nps4_sfx_bank_start:\n.incbin "'+str((R/'gba/pcm_sfx/ps4_sfx_bank.pcm').resolve())+'"\n.global ps4_sfx_bank_end\nps4_sfx_bank_end:\n.section .note.GNU-stack,"",@progbits\n')
exe=O/f'R46_{a.label}_SFX_OWNERSHIP'
subprocess.run(['gcc','-std=c11','-O1','-g','-fno-omit-frame-pointer','-fsanitize=address,undefined','-no-pie','-Wno-pointer-to-int-cast','-I'+str(R/'gba'),str(probe),str(bank),'-o',str(exe)],check=True)
run=subprocess.run([str(exe)],capture_output=True,text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'});print(run.stdout,end='');print(run.stderr,end='',file=sys.stderr)
assert run.returncode==0,run.returncode
report={'source_headers':70,'special_channel':'all three original headers use hardware FM4','source_priority':'PlaySFX / StopSFX / StopSpcSFX assembly branches','baseline_defects_reproduced':a.expect_baseline,'sanitizers':'ASan/UBSan','cases':0 if a.expect_baseline else 402,'evidence':run.stdout.strip(),'scope':'actual C stream and unchanged accepted PCM; music/MMIO stubs; full audio transport, audible quality and hardware remain unverified'}
(O/f'R46_{a.label}_SFX_OWNERSHIP.json').write_text(json.dumps(report,indent=2)+'\n')
