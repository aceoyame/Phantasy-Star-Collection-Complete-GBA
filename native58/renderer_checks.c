/* Original CP07 per-pixel renderer is the independent oracle. */
#define main original_host_main
#include "../Release48/source/tools_host/md_host/mdhost.c"
#undef main
#include "../Release48/source/additional/mdrender.c"
void mdsnd_ym_68k(u32 a,u32 v){} void mdsnd_psg_68k(u32 v){}
void mdsnd_busreq(u32 v){} void mdsnd_reset(u32 v){}
static void vw(u32 a,u32 v){md_vram[(a&65535u)>>1]=(u16)v;}
static void fixture(u32 trial){
 random_state();
 M.reg[1]=0x54u|((trial&1u)?8u:0u);M.reg[12]=(trial&2u)?0x81u:0;
 M.reg[2]=0x30;M.reg[3]=0x28;M.reg[4]=7;M.reg[5]=0x78;M.reg[13]=0x3e;
 M.reg[16]=0x11;M.reg[11]=(trial>>2)&7u;
 M.reg[17]=((trial>>5)&3u)==0?0x80:((trial>>5)&3u)==1?0x94:((trial>>5)&3u)==2?10:0;
 M.reg[18]=((trial>>7)&3u)==0?0x80:((trial>>7)&3u)==1?0x8a:((trial>>7)&3u)==2?18:0;
 for(u32 k=0;k<40;k++)md_vsram[k]=(trial&512u)?(k*11u)&511u:0;
 for(u32 t=0;t<512;t++)for(u32 j=0;j<16;j++){
  u32 v=0;for(u32 n=0;n<4;n++)v=(v<<4)|(1u+rnd()%15u);
  vw(t*32u+j*2u,v);
 }
 u32 ww=(trial&2u)?64u:32u;
 for(u32 y=0;y<32;y++)for(u32 k=0;k<ww;k++){
  u32 e=1u+rnd()%510u;e|=rnd()&0x7800u;
  /* Four cases: opaque/high, mixed priority, holes/high, mixed + holes. */
  if((trial&3u)!=1u&&(trial&3u)!=3u)e|=0x8000u;else e|=rnd()&0x8000u;
  vw(0xa000u+(y*ww+k)*2u,e);
 }
 if(trial&1u)for(u32 k=0;k<256;k++)vw(32u+(rnd()%16000u)*2u,rnd()&0xfff0u);
 /* Overlay visible low/high sprites; sometimes no visible sprite. */
 for(u32 k=0;k<6;k++){
  u32 a=0xf000u+k*8u;vw(a,(trial&1024u)?128u+k*30u:0);
  vw(a+2u,(k<5u?k+1u:0u)|(rnd()&0xf00u));
  vw(a+4u,450u+(k&31u)|(rnd()&0xf800u));vw(a+6u,128u+k*44u);
 }
 M.rfull=1;M.vgen++;M.cgen++;
}
static int check_page(u32 page){old_render();for(u32 k=0;k<38400;k++)if(md_h_fb[page*0xa000u+k]!=((u8*)oref)[k])return 1;return 0;}
int main(void){
 m68k_ram=md_ram;mdr_init();u32 n=0,covered=0;
 for(u32 t=0;t<2048;t++){
  fixture(t);if(compare_render("directed"))return 1;
  covered+=window_cover!=0;n++;
  /* Both pending pages, tile opacity changes, table priority changes. */
  mdr_render(1);if(check_page(1))return 2;
  for(u32 j=0;j<4;j++){
   u32 a=j&1u?0xa000u+(rnd()%1024u)*2u:(rnd()%512u)*32u+(rnd()%16u)*2u;
   vw(a,rnd());u32 tile=a>>5;md_vdirty[tile>>5]|=1u<<(tile&31u);M.vgen++;
   for(u32 p=0;p<2;p++){mdr_render(p);if(check_page(p)){printf("incremental fail %u/%u/%u\n",t,j,p);return 3;}}n+=2;
  }
 }
 if(!covered)return 4;
 for(u32 t=0;t<4096;t++){random_state();if(compare_render("random"))return 5;n++;}
 printf("PASS %u full/incremental comparisons, %u directed final-line covers\n",n,covered);
 return 0;
}
