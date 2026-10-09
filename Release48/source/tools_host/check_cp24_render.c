#define main legacy_mdhost_main
#include "md_host/mdhost.c"
#undef main
u32 aot_rgen[128];
void mdsnd_ym_68k(u32 a,u32 v){}
void mdsnd_psg_68k(u32 v){}
void mdsnd_busreq(u32 v){}
void mdsnd_reset(u32 v){}
int main(int argc,char **argv){
 if(argc>1&&!strcmp(argv[1],"dirty")){
  for(u32 test=0;test<120;test++){
   memset(&M,0,sizeof M);memset(md_vsram,0,sizeof md_vsram);
   memset(md_vram,0,sizeof md_vram);memset(md_vdirty,0,sizeof md_vdirty);
   for(u32 i=16;i<8192;i++)md_vram[i]=(u16)rnd();
   M.reg[1]=64u|((test&1u)?8u:0u);M.reg[12]=(test>>1)&1u;
   M.reg[2]=0x30;M.reg[4]=7;M.reg[3]=0x34;M.reg[5]=0x58;M.reg[13]=0x3c;M.reg[7]=test&63u;
   M.reg[16]=(test/4)%3u;M.reg[11]=(test/12)%4u;
   for(u32 i=0xc000u/2;i<32768;i++)md_vram[i]=(u16)((rnd()&0xf800u)|(1u+rnd()%511u));
   M.reg[17]=(u8)rnd();M.reg[18]=(u8)rnd();
   md_vsram[0]=(u16)rnd();md_vsram[1]=(u16)rnd();
   M.rfull=1;mdr_init();mdr_render(0);mdr_render(1);
   for(u32 step=0;step<12;step++){
    u32 wi;
    if(step%3u==0)wi=(1u+rnd()%511u)*16u+(rnd()&15u);
    else if(step%3u==1)wi=0xc000u/2+(rnd()&8191u);
    else wi=0xf000u/2+(rnd()&1u);
    md_vram[wi]=(u16)rnd();u32 block=wi>>4;
    md_vdirty[block>>5]|=1u<<(block&31u);M.vgen++;
    u32 page=step&1u;mdr_render(page);old_render();
    if(memcmp(md_h_fb+page*0xa000,oref,240*160)){
     printf("FAIL: incremental test %u step %u width %u scroll mode %u plane width mode %u\n",test,step,M.reg[12],M.reg[11],M.reg[16]);return 1;
    }
   }
  }
  puts("PASS: 1440 incremental updates, scroll/wrap/window/flip/priority and both pages exact");return 0;
 }

 if(argc>1&&!strcmp(argv[1],"memo")){
  for(u32 test=0;test<400;test++){
   memset(&M,0,sizeof M);memset(md_vsram,0,sizeof md_vsram);
   for(u32 i=0;i<32768;i++)md_vram[i]=(u16)rnd();
   M.reg[1]=64u|((test&1u)?8u:0u);M.reg[12]=(test>>1)&1u;
   M.reg[2]=0x30;M.reg[4]=7;M.reg[3]=0x34;M.reg[5]=0x58;M.reg[13]=0x3c;M.reg[7]=test&63u;
   for(u32 i=0;i<320;i++)md_vram[(0xb000u>>1)+i]=0;
   md_vram[0xf000u>>1]=md_vram[(0xf000u>>1)+1]=0;
   M.reg[17]=(u8)rnd();M.reg[18]=(u8)rnd();
   if(compare_render("memo-cold"))return 1;
   if(compare_render("memo-reuse"))return 1;
   M.reg[7]=(M.reg[7]+7)&63;
   if(compare_render("memo-backdrop"))return 1;
   md_vram[(rnd()&0x7ff)*16]=(u16)rnd();
   if(compare_render("memo-changed-tile"))return 1;
  }
  puts("PASS: 1600 aligned mixed-plane states, exact pixels");return 0;
 }
 if(argc>1&&!strcmp(argv[1],"single")){
  for(u32 test=0;test<120;test++){
   memset(&M,0,sizeof M);memset(&C,0,sizeof C);memset(md_vram,0,sizeof md_vram);memset(md_vsram,0,sizeof md_vsram);
   M.reg[1]=64u|((test&1u)?8u:0u);M.reg[12]=(test>>1)&1u;
   M.reg[2]=0x30;M.reg[4]=7;M.reg[3]=0x34;M.reg[5]=0x58;M.reg[13]=0x3c;M.reg[7]=test&63u;
   for(u32 i=16;i<1024;i++)md_vram[i]=(u16)rnd();
   u32 plane=test%3u,base=plane==0?0xc000u:plane==1?0xe000u:0xd000u;
   for(u32 i=0;i<1024;i++)md_vram[(base>>1)+i]=(u16)((rnd()&0xf800u)|1u+(rnd()%63u));
   if(plane==2){M.reg[18]=0x80;M.reg[17]=(test&4u)?0x84u:0;}
   else {md_vsram[plane]=(u16)(test%16);md_vram[0xf000u/2+plane]=(u16)(test%8);}
   if(compare_render("single-plane"))return 1;
  }
  puts("PASS: 120 single-plane/window states, exact pixels");return 0;
 }
 return legacy_mdhost_main(argc,argv);
}
