/* Preserve per-instruction traces in host checks, including stale stack RAM.
 * The release needs only flags that remain live at the next native entry. */
#if defined(MD_HOST) && !defined(SND_NATIVE_RELEASE)
#define NOTE_TRACE(pc,dt) do { \
 if(!snd_pccount[0xfff]++)atexit(pchist_dump);snd_pccount[(pc)&0xfff]++; \
 trace_rec(pc,cyc+(dt),A,F,BC>>8,BC&255,native_note.de>>8,native_note.de&255, \
 HL>>8,HL&255,SP,native_note.ix,native_note.iy); \
}while(0)
#else
#define NOTE_TRACE(pc,dt) ((void)0)
#endif
static inline u32 note_read(u32 a,u32 ts){return a<0x4000u?md_zram[a&0x1fffu]:ard(a,ts);}
SND_ARM __attribute__((noinline)) static void native_note_run(u32 tend){
 u32 A=native_note.a,F=native_note.f,BC=native_note.bc,HL=native_note.hl;
 u32 SP=native_note.sp,cyc=native_note.cyc,R=native_note.r,pc=0xf94;
 const u32 scratch=(native_note.ix+0x17u)&0xffffu,originalHL=HL,originalSP=SP;
 do{
  NOTE_TRACE(0xf94,0);md_zram[scratch&0x1fffu]=(u8)A;
  NOTE_TRACE(0xf97,19);SP-=2;md_zram[(SP+1)&0x1fffu]=(u8)(HL>>8);md_zram[SP&0x1fffu]=(u8)HL;
  NOTE_TRACE(0xf98,30);BC=(BC&0xff00u)|md_zram[scratch&0x1fffu];
  NOTE_TRACE(0xf9b,49);SP-=2;md_zram[(SP+1)&0x1fffu]=0x0f;md_zram[SP&0x1fffu]=0x9e;
  NOTE_TRACE(0x481,66);BC&=255;NOTE_TRACE(0x483,73);
#if defined(MD_HOST) && !defined(SND_NATIVE_RELEASE)
  OP_ADD16(HL,BC);
#else
  F=(HL+BC)>>16;HL=(HL+BC)&0xffffu;
#endif
  NOTE_TRACE(0x484,84);BC=(BC&0xff00u)|(HL&255);NOTE_TRACE(0x485,88);BC=HL;
  NOTE_TRACE(0x486,92);A=note_read(BC,cyc+92);NOTE_TRACE(0x487,99);SP+=2;
  NOTE_TRACE(0xf9e,109);HL=originalHL;SP=originalSP;
  NOTE_TRACE(0xf9f,119);OP_BIT(A,7);NOTE_TRACE(0xfa1,127);
  if(!(A&0x80u)){pc=0xfc4;cyc+=139;R+=16;break;}
  NOTE_TRACE(0xfa3,134);OP_CP(0x83);NOTE_TRACE(0xfa5,141);
  if(A==0x83u){pc=0xfb3;cyc+=153;R+=18;break;}
  NOTE_TRACE(0xfa7,148);OP_CP(0x81);NOTE_TRACE(0xfa9,155);
  if(A==0x81u){pc=0xfbe;cyc+=167;R+=20;break;}
  NOTE_TRACE(0xfab,162);OP_CP(0x80);NOTE_TRACE(0xfad,169);
  if(A==0x80u){pc=0xfbb;cyc+=181;R+=22;break;}
  NOTE_TRACE(0xfaf,176);BC=(BC+1)&0xffffu;
  NOTE_TRACE(0xfb0,182);A=note_read(BC,cyc+182);NOTE_TRACE(0xfb1,189);cyc+=201;R+=25;
 }while((s32)(cyc+201u-tend)<0);
 native_note.a=A;native_note.f=F;native_note.bc=BC;native_note.cyc=cyc;native_note.r=R;native_note.pc=pc;
}
/* Native RST 10h / RST 18h: original YM timers, flags, write timestamps,
 * refresh count and nested return-address writes are retained. */
static HOT SND_ARM __attribute__((noinline)) void native_ym_run(void){
 u32 A=native_note.a,F=native_note.f,BC=native_note.bc;
 u32 SP=native_note.sp,cyc=native_note.cyc,R=native_note.r;
#ifdef MD_HOST
 u32 HL=native_note.hl;
#endif
 u32 ix1=(native_note.ix+1)&0xffffu,ix0=native_note.ix;
 if(native_note.pc==0x18)goto port0;
 NOTE_TRACE(0x0010,0);{u32 v=note_read(ix1,cyc);OP_BITX(v,7,ix1);}NOTE_TRACE(0x0014,20);
 if(!(F&ZF)){cyc+=31;R+=3;goto done;}
 NOTE_TRACE(0x0015,25);NOTE_TRACE(0x04a2,35);{u32 v=note_read(ix1,cyc+35);OP_BITX(v,2,ix1);}NOTE_TRACE(0x04a6,55);
 if(F&ZF){
  NOTE_TRACE(0x04a8,62);{u32 v=note_read(ix0,cyc+62);OP_BITX(v,2,ix0);}NOTE_TRACE(0x04ac,82);
  if(!(F&ZF)){cyc+=93;R+=10;goto done;}
  NOTE_TRACE(0x04ad,87);OP_ADD(note_read(ix1,cyc+87),0);NOTE_TRACE(0x04b0,106);SP-=2;
  md_zram[(SP+1)&0x1fffu]=0x04;md_zram[SP&0x1fffu]=0xb1;cyc+=117;R+=13;
port0:
  NOTE_TRACE(0x0018,0);ym_addr[0]=(u8)A;NOTE_TRACE(0x001b,13);SP-=2;
  md_zram[(SP+1)&0x1fffu]=0;md_zram[SP&0x1fffu]=0x1c;
  NOTE_TRACE(0x0008,24);timers_sync(cyc+24);A=ym_status;
  NOTE_TRACE(0x000b,37);OP_BIT(A,7);NOTE_TRACE(0x000d,45);NOTE_TRACE(0x000f,52);SP+=2;
  NOTE_TRACE(0x001c,62);A=BC&255;NOTE_TRACE(0x001d,66);
  NOTE_TRACE(0x04b2,76);ym_data(0,ym_addr[0],A,cyc+76,cyc+89);
  NOTE_TRACE(0x04b5,89);cyc+=99;R+=11;
  if(native_note.pc==0x10){SP+=2;NOTE_TRACE(0x04b1,0);cyc+=10;R++;}
 }else{
  NOTE_TRACE(0x04b6,67);{u32 v=note_read(ix0,cyc+67);OP_BITX(v,2,ix0);}NOTE_TRACE(0x04ba,87);
  if(!(F&ZF)){cyc+=98;R+=10;goto done;}
  NOTE_TRACE(0x04bb,92);OP_ADD(note_read(ix1,cyc+92),0);NOTE_TRACE(0x04be,111);OP_SUB(4,0);
  NOTE_TRACE(0x04c0,118);ym_addr[1]=(u8)A;NOTE_TRACE(0x04c3,131);SP-=2;
  md_zram[(SP+1)&0x1fffu]=0x04;md_zram[SP&0x1fffu]=0xc4;
  NOTE_TRACE(0x0008,142);timers_sync(cyc+142);A=ym_status;
  NOTE_TRACE(0x000b,155);OP_BIT(A,7);NOTE_TRACE(0x000d,163);NOTE_TRACE(0x000f,170);SP+=2;
  NOTE_TRACE(0x04c4,180);A=BC&255;NOTE_TRACE(0x04c5,184);ym_data(1,ym_addr[1],A,cyc+184,cyc+197);
  NOTE_TRACE(0x04c8,197);cyc+=207;R+=23;
 }
done:
 native_note.a=A;native_note.f=F;native_note.cyc=cyc;native_note.r=R;
}
#undef NOTE_TRACE
