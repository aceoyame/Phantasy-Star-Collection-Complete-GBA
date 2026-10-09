#define main legacy_mdhost_main
#include "md_host/mdhost.c"
#undef main
void mdsnd_ym_68k(u32 a,u32 v){}
void mdsnd_psg_68k(u32 v){}
void mdsnd_busreq(u32 v){}
void mdsnd_reset(u32 v){}
int main(int argc,char **argv){
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
