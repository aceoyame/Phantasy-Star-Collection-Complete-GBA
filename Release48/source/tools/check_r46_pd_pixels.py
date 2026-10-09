#!/usr/bin/env python3
"""Exact optimized PD pixels versus independent source map/nibble sampling.

Run check_r45_pd.py first to generate its production-function fixture.
"""
from pathlib import Path
import json,os,struct,subprocess
from md_compression import nemesis_decompress,kosinski_decompress
R=Path(__file__).resolve().parents[1];O=R.parent/'build_artifacts'
prefix=(O/'r45_pd_probe.c').read_text().split('int main(int argc,char**argv)')[0]
rom=(R/'references/roms/ps4built.bin').read_bytes();textures=bytearray()
for art,mp,kos in [(0x22EDD6,0x22F82C,True),(0x22B392,0x22BF98,False),(0x2274B2,0x228334,False)]:
 pixels=nemesis_decompress(rom[art:]);mapping=kosinski_decompress(rom[mp:]) if kos else rom[mp:mp+3072];words=struct.unpack('>1536H',mapping)
 for y in range(192):
  for x in range(512):
   e=words[(y//8)*64+x//8];tx=(x&7)^(7 if e&0x800 else 0);ty=(y&7)^(7 if e&0x1000 else 0);at=(e&2047)*32+ty*4+tx//2
   ci=((pixels[at]>>(0 if tx&1 else 4))&15) if at<len(pixels) else 0
   textures.append((192 if ci and e&0x2000 else 208)+ci)
(O/'r46_pd_reference_textures.bin').write_bytes(textures)
main=r'''
static uint8_t expected[38400],textures[3*512*192];
static unsigned scalar(const uint8_t*art,unsigned bytes,unsigned e,unsigned x,unsigned y){
 if(e&0x800)x=7-x;if(e&0x1000)y=7-y;unsigned at=(e&2047)*32+y*4+x/2;
 if(at>=bytes)return 0;unsigned q=art[at];return x%2?q&15:q>>4;
}
int main(int argc,char**argv){assert(argc==3);FILE*f=fopen(argv[1],"rb");assert(f&&fread(rom,1,sizeof rom,f)==sizeof rom);fclose(f);f=fopen(argv[2],"rb");assert(f&&fread(textures,1,sizeof textures,f)==sizeof textures);fclose(f);
 for(unsigned x=0;x<240;x++)src_x[x]=x*4/3;for(unsigned y=0;y<160;y++)src_y[y]=y*7/5;
 unsigned cases=0;uint64_t pixels=0;
 for(unsigned id=0x85;id<=0x87;id++)for(unsigned phase=0;phase<512;phase++)for(unsigned clip=0;clip<2;clip++){
  R45PDVisual*s=r45_pd_visual();memset(s,0,sizeof(*s));s->body_id=id;s->active=s->controller=1;r45_pd_bg_load(id);
  for(unsigned op=0;op<6;op++)battle_enemy_anim_frame[0][op]=(phase>>6)%8;
  uint8_t*aa;const uint16_t*mm;unsigned mc,mw,mh,ab=battle_enemy_assemble(0,id,&aa,&mm,&mc,&mw,&mh);
  assert(ab<=sizeof(s->body_art));
  r45_pd_body_snapshot(id);
  if(s->bg_map>2){fprintf(stderr,"Corrupt snapshot id=%u phase=%u bytes=%u bg_map=%u\n",id,phase,s->body_bytes,s->bg_map);return 2;}
  const uint32_t*nm=battle_enemy_native_meta(id);const uint16_t*m=(const uint16_t*)(const void*)(ps4_precomp_assets+PS4_PRECOMP_BATTLE_ENEMY_NATIVE_MAP_OFF+nm[4]);
  const uint8_t*bg=textures+(id-0x85)*512*192;
  for(unsigned y=0;y<192;y++){s->fg[y]=(int16_t)(phase+y*7-320);s->bg[y]=(int16_t)(phase+y*11-512);}
  battle_compose_clip_active=clip;battle_compose_clip_x0=clip?1+(phase%11):0;battle_compose_clip_x1=clip?239-(phase%13):240;
  battle_compose_clip_y0=clip?phase%19:0;battle_compose_clip_y1=clip?160-(phase%17):160;
  memset(field_pause_fb,55,38400);memset(expected,55,38400);
  unsigned x0=battle_compose_clip_x0,x1=battle_compose_clip_x1,y0=battle_compose_clip_y0,y1=battle_compose_clip_y1;
  for(unsigned y=y0;y<y1;y++)for(unsigned x=x0;x<x1;x++){
   unsigned sy=y*7/5,sx=((int)(x*4/3)-(sy<192?s->bg[sy]:0))&511;
   if(sy<192)expected[y*240+x]=bg[sy*512+sx];else expected[y*240+x]=208+scalar(s->bg_art,s->bg_bytes,0,sx&7,sy&7);
  }
  r45_pd_background();assert(!memcmp(field_pause_fb,expected,38400));pixels+=38400;cases++;
  for(unsigned y=y0;y<y1&&y*7/5<120;y++)for(unsigned x=x0;x<x1;x++){
   unsigned sy=y*7/5,sx=((int)(x*4/3)-s->fg[sy])&511;if(sx>=320)continue;
   unsigned e=m[(sy/8)*40+sx/8],ci=scalar(s->body_art,s->body_bytes,e,sx%8,sy%8);
   if(ci)expected[y*240+x]=192+ci;
  }
  r45_pd_body();assert(!memcmp(field_pause_fb,expected,38400));pixels+=38400;cases++;
  /* Exercise the replacement queue: new Plane-B map with old/partially
   * loaded art, the path that must not substitute the final row bank early. */
  unsigned newmap=(id-0x85+1)%3;s->bg_map=newmap;s->bg_id=id-0x85;
  const uint16_t*bm=(const uint16_t*)(const void*)(r45_pd_art_bank+r45_pd_bg_meta[newmap][2]);
  unsigned take=(phase%51)*128,bankbytes=r45_pd_bg_meta[newmap][1];if(take>bankbytes)take=bankbytes;
  memcpy(s->bg_art,r45_pd_art_bank+r45_pd_bg_meta[newmap][0],take);if(take>s->bg_bytes)s->bg_bytes=take;
  memset(field_pause_fb,55,38400);memset(expected,55,38400);
  for(unsigned y=y0;y<y1;y++)for(unsigned x=x0;x<x1;x++){
   unsigned sy=y*7/5,sx=((int)(x*4/3)-(sy<192?s->bg[sy]:0))&511;
   unsigned e=sy<192?bm[(sy/8)*64+sx/8]:0,ci=scalar(s->bg_art,s->bg_bytes,e,sx%8,sy%8);
   expected[y*240+x]=(ci&&(e&0x2000)?192:208)+ci;
  }
  r45_pd_background();assert(!memcmp(field_pause_fb,expected,38400));pixels+=38400;cases++;
 }
 printf("PASS %u PD background/body/partial-queue cases, %llu full-frame pixel comparisons; all 512 scroll phases, three forms, varying per-line shifts, eight art phases and odd clips\n",cases,(unsigned long long)pixels);
}
'''
c=O/'r46_pd_pixels_probe.c';c.write_text(prefix+main)
subprocess.run(['gcc','-std=c11','-O1','-g','-no-pie','-fsanitize=address,undefined','-I'+str(R/'gba'),str(c),'-o',str(O/'r46_pd_pixels_probe')],check=True)
p=subprocess.run([str(O/'r46_pd_pixels_probe'),str(R/'references/roms/ps4built.bin'),str(O/'r46_pd_reference_textures.bin')],text=True,capture_output=True,check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
(O/'R46_PD_PIXELS_EVIDENCE.json').write_text(json.dumps({'result':'PASS','output':p.stdout.strip(),'scope':__doc__,'hardware_timing_or_full_VDP_parity':False},indent=2)+'\n');print(p.stdout,end='')
