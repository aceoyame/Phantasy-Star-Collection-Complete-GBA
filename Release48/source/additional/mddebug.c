/* Genesis runtime debug launcher / debug menu (CP08).
 *
 * Opens from the Additional Content menu (START on a Genesis entry = start paused
 * inside the debug menu) or at any time in-game with SELECT.  A+B+START+SELECT
 * returns to the collection from anywhere.
 *
 * Pages (L/R switch):  CPU  VDP  TRACE  MEM  TILES  SYS  MENU
 *   CPU    PC, SR, D0-D7, A0-A7, USP/SSP, cycles, frame/line, STOP, exception diagnostics, spin-skip counters
 *   VDP    registers, derived display state, command/address latch, status, interrupt flags, DMA, write counters,
 *          first-event frame numbers (when the guest first touched the VDP, CRAM, display enable, ...)
 *   TRACE  last 32 control-flow transfers (branch/jsr/rts/jmp/dbf/exception/irq)
 *   MEM    hex view of RAM/VRAM/CRAM/VSRAM/Z80 RAM/ROM with checksums
 *   TILES  VRAM tile viewer using the game's CRAM
 *   SYS    timing, ROM header, checksums, input
 *   MENU   resume, live/pause, step frame/instructions, run-to (VDP/CRAM/display/vblank/PC), run N frames,
 *          force display on, spin-fold toggle, reset game, exit
 *
 * The menu never changes guest state unless an action says so.
 */
#include <stdarg.h>
#include "md.h"

int dbg_banner_enable=1;

/* ---------------- 5x7 font (ASCII 32..126, column-major, bit0 = top row) ---------------- */
static const u8 font5x7[95][5]={
{0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x5F,0x00,0x00},{0x00,0x07,0x00,0x07,0x00},{0x14,0x7F,0x14,0x7F,0x14},
{0x24,0x2A,0x7F,0x2A,0x12},{0x23,0x13,0x08,0x64,0x62},{0x36,0x49,0x55,0x22,0x50},{0x00,0x05,0x03,0x00,0x00},
{0x00,0x1C,0x22,0x41,0x00},{0x00,0x41,0x22,0x1C,0x00},{0x14,0x08,0x3E,0x08,0x14},{0x08,0x08,0x3E,0x08,0x08},
{0x00,0x50,0x30,0x00,0x00},{0x08,0x08,0x08,0x08,0x08},{0x00,0x60,0x60,0x00,0x00},{0x20,0x10,0x08,0x04,0x02},
{0x3E,0x51,0x49,0x45,0x3E},{0x00,0x42,0x7F,0x40,0x00},{0x42,0x61,0x51,0x49,0x46},{0x21,0x41,0x45,0x4B,0x31},
{0x18,0x14,0x12,0x7F,0x10},{0x27,0x45,0x45,0x45,0x39},{0x3C,0x4A,0x49,0x49,0x30},{0x01,0x71,0x09,0x05,0x03},
{0x36,0x49,0x49,0x49,0x36},{0x06,0x49,0x49,0x29,0x1E},{0x00,0x36,0x36,0x00,0x00},{0x00,0x56,0x36,0x00,0x00},
{0x08,0x14,0x22,0x41,0x00},{0x14,0x14,0x14,0x14,0x14},{0x00,0x41,0x22,0x14,0x08},{0x02,0x01,0x51,0x09,0x06},
{0x32,0x49,0x79,0x41,0x3E},{0x7E,0x11,0x11,0x11,0x7E},{0x7F,0x49,0x49,0x49,0x36},{0x3E,0x41,0x41,0x41,0x22},
{0x7F,0x41,0x41,0x22,0x1C},{0x7F,0x49,0x49,0x49,0x41},{0x7F,0x09,0x09,0x09,0x01},{0x3E,0x41,0x49,0x49,0x7A},
{0x7F,0x08,0x08,0x08,0x7F},{0x00,0x41,0x7F,0x41,0x00},{0x20,0x40,0x41,0x3F,0x01},{0x7F,0x08,0x14,0x22,0x41},
{0x7F,0x40,0x40,0x40,0x40},{0x7F,0x02,0x0C,0x02,0x7F},{0x7F,0x04,0x08,0x10,0x7F},{0x3E,0x41,0x41,0x41,0x3E},
{0x7F,0x09,0x09,0x09,0x06},{0x3E,0x41,0x51,0x21,0x5E},{0x7F,0x09,0x19,0x29,0x46},{0x46,0x49,0x49,0x49,0x31},
{0x01,0x01,0x7F,0x01,0x01},{0x3F,0x40,0x40,0x40,0x3F},{0x1F,0x20,0x40,0x20,0x1F},{0x3F,0x40,0x38,0x40,0x3F},
{0x63,0x14,0x08,0x14,0x63},{0x07,0x08,0x70,0x08,0x07},{0x61,0x51,0x49,0x45,0x43},{0x00,0x7F,0x41,0x41,0x00},
{0x02,0x04,0x08,0x10,0x20},{0x00,0x41,0x41,0x7F,0x00},{0x04,0x02,0x01,0x02,0x04},{0x40,0x40,0x40,0x40,0x40},
{0x00,0x01,0x02,0x04,0x00},{0x20,0x54,0x54,0x54,0x78},{0x7F,0x48,0x44,0x44,0x38},{0x38,0x44,0x44,0x44,0x20},
{0x38,0x44,0x44,0x48,0x7F},{0x38,0x54,0x54,0x54,0x18},{0x08,0x7E,0x09,0x01,0x02},{0x0C,0x52,0x52,0x52,0x3E},
{0x7F,0x08,0x04,0x04,0x78},{0x00,0x44,0x7D,0x40,0x00},{0x20,0x40,0x44,0x3D,0x00},{0x7F,0x10,0x28,0x44,0x00},
{0x00,0x41,0x7F,0x40,0x00},{0x7C,0x04,0x18,0x04,0x78},{0x7C,0x08,0x04,0x04,0x78},{0x38,0x44,0x44,0x44,0x38},
{0x7C,0x14,0x14,0x14,0x08},{0x08,0x14,0x14,0x18,0x7C},{0x7C,0x08,0x04,0x04,0x08},{0x48,0x54,0x54,0x54,0x20},
{0x04,0x3F,0x44,0x40,0x20},{0x3C,0x40,0x40,0x20,0x7C},{0x1C,0x20,0x40,0x20,0x1C},{0x3C,0x40,0x30,0x40,0x3C},
{0x44,0x28,0x10,0x28,0x44},{0x0C,0x50,0x50,0x50,0x3C},{0x44,0x64,0x54,0x4C,0x44},{0x00,0x08,0x36,0x41,0x00},
{0x00,0x00,0x7F,0x00,0x00},{0x00,0x41,0x36,0x08,0x00},{0x10,0x08,0x08,0x10,0x08}
};

/* ---------------- drawing ---------------- */
/* palette slots 240..255 are reserved for the debug UI (game CRAM uses 0..63) */
enum { C_BG=240, C_TXT, C_VAL, C_LAB, C_ERR, C_HDR, C_DIM, C_BOX, C_SEL, C_OK };
static const u16 dbg_pal[10]={ 0x2000, 0x7FFF, 0x2FE5, 0x03FF, 0x001F, 0x7FE0, 0x4210, 0x7C00, 0x5800, 0x03E0 };

static unsigned pg;                       /* page being drawn */
static inline void px(u32 x,u32 y,u32 c){
  volatile u16*p=(volatile u16*)(MD_PAGE(pg)+y*240u+(x&~1u)); u32 q=*p;
  *p=(u16)((x&1u)?((q&0xFFu)|(c<<8)):((q&0xFF00u)|c));
}
/* Mode 4 VRAM only takes 16/32-bit writes: spans are written as words/halfwords, only odd edges are read-modify-write. */
static inline void px_row(volatile u8*row,u32 x,u32 c){
  volatile u16*p=(volatile u16*)(row+(x&~1u)); u32 q=*p;
  *p=(u16)((x&1u)?((q&0xFFu)|(c<<8)):((q&0xFF00u)|c));
}
static void fill_rect(u32 x,u32 y,u32 w,u32 h,u32 c){
  if(x>=240u||y>=160u) return;
  if(x+w>240u) w=240u-x; if(y+h>160u) h=160u-y;
  u32 xe=x+w, c2=c*0x0101u, c4=c*0x01010101u;
  for(u32 j=0;j<h;j++){
    volatile u8*row=(volatile u8*)MD_PAGE(pg)+(y+j)*240u; u32 xx=x;
    if((xx&1u)&&xx<xe){ px_row(row,xx,c); xx++; }
    if((xx&2u)&&xx+2u<=xe){ *(volatile u16*)(row+xx)=(u16)c2; xx+=2u; }
    while(xx+4u<=xe){ *(volatile u32*)(row+xx)=c4; xx+=4u; }
    if(xx+2u<=xe){ *(volatile u16*)(row+xx)=(u16)c2; xx+=2u; }
    if(xx<xe) px_row(row,xx,c);
  }
}
static u32 text_bg=C_BG;                  /* flat colour under the text being drawn (set by clear_page / banners) */
static void clear_page(u32 c){
  text_bg=c;
  volatile u32*o=(volatile u32*)MD_PAGE(pg); u32 w=c*0x01010101u; for(u32 i=0;i<240u*160u/4u;i++) o[i]=w;
}
/* Text is drawn on a flat background: glyph cells (6x7 pixels, always at even x) are written as three halfwords per row. */
static void glyph(u32 x,u32 y,u32 ch,u32 c){
  if(ch<32u||ch>126u) ch='?';
  const u8*g=font5x7[ch-32u]; u32 bg=text_bg;
  volatile u8*row=(volatile u8*)MD_PAGE(pg)+y*240u+x;
  for(u32 r=0;r<7u;r++,row+=240u){
    u32 p0=((g[0]>>r)&1u)?c:bg, p1=((g[1]>>r)&1u)?c:bg, p2=((g[2]>>r)&1u)?c:bg, p3=((g[3]>>r)&1u)?c:bg, p4=((g[4]>>r)&1u)?c:bg;
    ((volatile u16*)row)[0]=(u16)(p0|(p1<<8)); ((volatile u16*)row)[1]=(u16)(p2|(p3<<8)); ((volatile u16*)row)[2]=(u16)(p4|(bg<<8));
  }
}
static void text(u32 col,u32 row,u32 c,const char*s){
  u32 x=col*6u,y=row*8u; for(;*s;s++){ if(x+5u>240u) break; glyph(x,y,(u32)(unsigned char)*s,c); x+=6u; }
}
static u32 put_num(char*o,u32 v,u32 base,u32 width,char pad,u32 neg,u32 upper){
  char t[12]; u32 n=0; if(!v) t[n++]='0';
  while(v){ u32 d=base==16u?(v&15u):(v-(v/10u)*10u); v=base==16u?(v>>4):(v/10u); t[n++]=(char)(d<10u?'0'+d:(upper?'A':'a')+d-10u); }
  if(neg) t[n++]='-';
  u32 k=0; while(n+k<width) o[k++]=pad;
  while(n) o[k++]=t[--n];
  return k;
}
static void vfmt(char*o,const char*f,va_list ap){
  while(*f){
    if(*f!='%'){ *o++=*f++; continue; }
    f++; char pad=' '; u32 w=0; if(*f=='0'){ pad='0'; f++; }
    while(*f>='0'&&*f<='9'){ w=w*10u+(u32)(*f-'0'); f++; }
    char k=*f++;
    if(k=='x'||k=='X') o+=put_num(o,va_arg(ap,u32),16u,w,pad,0,k=='X');
    else if(k=='u') o+=put_num(o,va_arg(ap,u32),10u,w,pad,0,0);
    else if(k=='d'){ s32 v=va_arg(ap,s32); o+=put_num(o,v<0?(u32)(-v):(u32)v,10u,w,pad,v<0,0); }
    else if(k=='s'){ const char*s=va_arg(ap,const char*); while(*s) *o++=*s++; }
    else if(k=='c') *o++=(char)va_arg(ap,int);
    else if(k=='%') *o++='%';
    else if(!k) break;
  }
  *o=0;
}
static void tf(u32 col,u32 row,u32 c,const char*f,...){
  char b[64]; va_list ap; va_start(ap,f); vfmt(b,f,ap); va_end(ap); text(col,row,c,b);
}

/* ---------------- state ---------------- */
enum { P_CPU,P_VDP,P_TRACE,P_MEM,P_TILES,P_SYS,P_MENU,P_COUNT };
static const char*const page_name[P_COUNT]={"CPU","VDP","TRACE","MEM","TILES","SYS","MENU"};
static u32 page_idx=P_MENU, menu_sel, mem_region, mem_addr, tile_base, tile_pal, live, step_sel, frames_sel, pc_target, pc_edit, pc_digit;
static u32 held_prev;
static char status_msg[41];

static const char*const region_name[6]={"WORK RAM","VRAM","CRAM","VSRAM","Z80 RAM","ROM"};
static const u32 region_size[6]={0x10000u,0x10000u,0x80u,0x50u,0x2000u,0u};
static const u32 step_tab[5]={1u,16u,256u,4096u,65536u};
static const u32 frames_tab[5]={1u,10u,60u,300u,1200u};

static u32 rom_sum_cache; static u32 rom_sum_valid;
static u32 sum_region(u32 r){
  u32 a=0,b=0;
  if(r==5u&&rom_sum_valid) return rom_sum_cache;
  switch(r){
    case 0: for(u32 i=0;i<32768u;i++){ a=(a+md_ram[i])&0xFFFFu; b=(b+a)&0xFFFFu; } break;
    case 1: for(u32 i=0;i<32768u;i++){ a=(a+md_vram[i])&0xFFFFu; b=(b+a)&0xFFFFu; } break;
    case 2: for(u32 i=0;i<64u;i++){ a=(a+md_cram[i])&0xFFFFu; b=(b+a)&0xFFFFu; } break;
    case 3: for(u32 i=0;i<40u;i++){ a=(a+md_vsram[i])&0xFFFFu; b=(b+a)&0xFFFFu; } break;
    case 4: for(u32 i=0;i<8192u;i++){ a=(a+md_zram[i])&0xFFFFu; b=(b+a)&0xFFFFu; } break;
    default: for(u32 i=0;i<md_cfg.size;i+=2u){ a=(a+((u32)bus_read8(i)<<8|bus_read8(i+1u)))&0xFFFFu; b=(b+a)&0xFFFFu; } rom_sum_cache=(b<<16)|a; rom_sum_valid=1; break;
  }
  return (b<<16)|a;
}
static u32 mem_byte(u32 r,u32 off){
  switch(r){
    case 0: return ((const u8*)md_ram)[(off&0xFFFFu)^1u];
    case 1: return ((const u8*)md_vram)[(off&0xFFFFu)^1u];
    case 2: return ((const u8*)md_cram)[(off&0x7Fu)^1u];
    case 3: return ((const u8*)md_vsram)[(off&0x7Fu)^1u];
    case 4: return md_zram[off&0x1FFFu];
    default: return bus_read8(off);
  }
}

/* "Faults" are exceptions a healthy game never takes (TRAP/CHK/DIV0 are legitimate software interrupts). */
static u32 fault_count(void){ return C.illegal_count+C.linea_count+C.linef_count+C.priv_count; }

/* ---------------- pages ---------------- */
static const char*const ring_kind[8]={"???","BRA","JSR","RET","JMP","DBF","EXC","IRQ"};
static const char* exc_name(u32 v){
  switch(v){ case 2:return "BUS ERR"; case 3:return "ADDR ERR"; case 4:return "ILLEGAL"; case 5:return "DIV ZERO"; case 6:return "CHK"; case 7:return "TRAPV";
    case 8:return "PRIVILEGE"; case 9:return "TRACE"; case 10:return "LINE A"; case 11:return "LINE F"; default: if(v>=32u&&v<48u) return "TRAP"; return "OTHER"; }
}

static void page_cpu(void){
  tf(0,2,C_LAB,"PC"); tf(3,2,C_VAL,"%08X",C.pc); tf(13,2,C_LAB,"SR"); tf(16,2,C_VAL,"%04X",C.sr); tf(22,2,C_LAB,"CYC"); tf(26,2,C_VAL,"%08X",C.cycles);
  tf(0,3,C_LAB,"FRAME"); tf(6,3,C_VAL,"%u",M.frame); tf(18,3,C_LAB,"LINE"); tf(23,3,C_VAL,"%u",(u32)M.line); tf(30,3,C_LAB,"STOP"); tf(35,3,C_VAL,"%u",(u32)C.stopped);
  for(u32 i=0;i<8;i++){ tf((i%3)*13,5+i/3,C_LAB,"D%u",i); tf((i%3)*13+3,5+i/3,C_VAL,"%08X",C.d[i]); }
  for(u32 i=0;i<8;i++){ tf((i%3)*13,8+i/3,C_LAB,"A%u",i); tf((i%3)*13+3,8+i/3,C_VAL,"%08X",C.a[i]); }
  tf(0,11,C_LAB,"USP"); tf(4,11,C_VAL,"%08X",C.usp); tf(14,11,C_LAB,"SSP"); tf(18,11,C_VAL,"%08X",C.ssp);
  tf(0,12,C_LAB,"IPL"); tf(4,12,C_VAL,"%u",(C.sr>>8)&7u); tf(8,12,C_LAB,"MODE"); tf(13,12,C_VAL,(C.sr&0x2000u)?"SUP":"USR");
  u32 ex=C.exc_count,fl=fault_count(); tf(0,13,fl?C_ERR:C_LAB,"EXC %u  ILL %u  LINEA %u  LINEF %u",ex,C.illegal_count,C.linea_count,C.linef_count);
  tf(0,14,fl?C_ERR:C_LAB,"TRAP %u  DIV0 %u  PRIV %u",C.trap_count,C.zdiv_count,C.priv_count);
  if(ex){ tf(0,15,fl?C_ERR:C_VAL,"LAST %s V:%02X PC:%06X OP:%04X",exc_name(C.last_exc_vec),C.last_exc_vec,C.last_exc_pc&0xFFFFFFu,C.last_exc_op);
          tf(0,16,fl?C_ERR:C_VAL,"      SR:%04X",C.last_exc_sr); }
  else tf(0,15,C_OK,"NO EXCEPTIONS");
  tf(0,17,C_LAB,"IRQ TAKEN %u  SPIN SKIPS %u",C.irq_taken,C.spin_skips);
  tf(0,18,C_LAB,"SPIN CYCLES %u  FOLD %s",C.spin_cycles,C.spin_enable?"ON":"OFF");
}

static u32 vdp_sub;
static void page_vdp(void){
  if(vdp_sub){
    tf(0,2,C_HDR,"VDP ACTIVITY COUNTERS");
    tf(0,3,C_LAB,"WRITES  VRAM %u",M.n_vram_wr); tf(0,4,C_LAB,"        CRAM %u  VSRAM %u",M.n_cram_wr,M.n_vsram_wr);
    tf(0,5,C_LAB,"CTRL %u  DATA %u  REG %u",M.n_ctrl_wr,M.n_data_wr,M.n_reg_wr);
    tf(0,6,C_LAB,"FILL %u  COPY %u  DATA READS %u",M.n_fill,M.n_copy,M.n_data_rd);
    tf(0,7,C_LAB,"STATUS READS %u  HV READS %u",M.n_status_rd,M.n_hv_rd);
    tf(0,8,C_LAB,"DMA %u  WORDS %u",M.n_dma,M.n_dma_words);
    tf(0,9,C_LAB,"LAST DMA SRC %06X LEN %04X DST %04X",M.dma_src,M.dma_len&0xFFFFu,M.dma_dst&0xFFFFu);
    tf(0,10,C_LAB,"VINT TAKEN %u  HINT TAKEN %u",M.n_vint,M.n_hint);
    tf(0,11,C_LAB,"IO READS %u WRITES %u  Z80 WR %u",M.n_io_rd,M.n_io_wr,M.n_z80_wr);
    tf(0,12,C_LAB,"YM2612 WR %u  PSG WR %u (NO SOUND)",M.n_ym_wr,M.n_psg_wr);
    tf(0,13,C_HDR,"FIRST OCCURRED AT FRAME (-=NEVER)");
    static const char*const en[EV_COUNT]={"VDP CTRL","VDP REG","VRAM","CRAM","VSRAM","DMA","DISPLAY ON","VINT ENABLE","VINT TAKEN","IDLE FOLD","IMAGE SHOWN","CPU FAULT"};
    for(u32 i=0;i<EV_COUNT;i++){ u32 c=(i&1u)*20u,r=14u+(i>>1); tf(c,r,C_LAB,"%s",en[i]); if(M.ev[i]==0xFFFFFFFFu) tf(c+13,r,C_DIM,"-"); else tf(c+13,r,C_VAL,"%u",M.ev[i]); }
    return;
  }
  for(u32 i=0;i<24;i++){ tf((i%6)*6,2+i/6,C_LAB,"%02X",i); tf((i%6)*6+2,2+i/6,C_VAL,":%02X",(u32)M.reg[i]); }
  u32 r1=M.reg[1],r12=M.reg[12];
  tf(0,6,(r1&0x40)?C_OK:C_ERR,"DISP %s",(r1&0x40)?"ON":"OFF"); tf(9,6,C_LAB,"VINT %s",(r1&0x20)?"ON":"OFF"); tf(18,6,C_LAB,"HINT %s",(M.reg[0]&0x10)?"ON":"OFF");
  tf(27,6,C_LAB,"%s %s",(r12&1)?"H40":"H32",(r1&8)?"V30":"V28");
  tf(0,7,C_LAB,"A:%04X B:%04X WIN:%04X SPR:%04X",(u32)(M.reg[2]&0x38)<<10,(u32)(M.reg[4]&7)<<13,(u32)(M.reg[3]&0x3E)<<10,(u32)(M.reg[5]&0x7F)<<9);
  static const u8 sz[4]={32,64,0,128};
  tf(0,8,C_LAB,"HSC:%04X MODE %u VSC MODE %u SIZE %ux%u",(u32)(M.reg[13]&0x3F)<<10,(u32)(M.reg[11]&3),(u32)((M.reg[11]>>2)&1),(u32)sz[M.reg[16]&3],(u32)sz[(M.reg[16]>>4)&3]);
  tf(0,9,C_LAB,"BG:%02X AUTOINC:%02X HINT CNT:%02X",(u32)M.reg[7],(u32)M.reg[15],(u32)M.reg[10]);
  tf(0,10,C_LAB,"CODE:%02X ADDR:%04X PEND:%u FILL:%u",(u32)M.code,M.addr&0xFFFFu,(u32)M.pending,(u32)M.fill_pending);
  tf(0,11,C_LAB,"STATUS:%04X LINE:%u VINT-P:%u HINT-P:%u",md_vdp_status(),(u32)M.line,(u32)M.vint_pending,(u32)M.hint_pending);
  tf(0,12,C_LAB,"DMA:%u LAST SRC:%06X LEN:%04X",M.n_dma,M.dma_src,M.dma_len&0xFFFFu);
  tf(0,13,C_LAB,"WR VRAM:%u CRAM:%u",M.n_vram_wr,M.n_cram_wr);
  tf(0,14,C_LAB,"VINT TAKEN:%u HINT:%u",M.n_vint,M.n_hint);
  tf(0,15,C_LAB,"CRAM (GAME PALETTE):");
  for(u32 i=0;i<64u;i++){ u32 c=md_cram[i]; fill_rect((i&15u)*14u+8u,(16u*8u)+(i>>4)*6u,12u,5u,(i&63u)); (void)c; }
  tf(22,15,C_DIM,"A=COUNTERS/EVENTS");
}

static void page_trace(void){
  tf(0,2,C_HDR,"RECENT CONTROL FLOW (NEWEST FIRST)");
  for(u32 i=0;i<32u;i++){
    u32 e=C.ring[(C.ring_pos-1u-i)&(M68K_RING-1u)]; u32 k=e>>24; if(k>7u) k=0;
    u32 col=(i>>4)*20u,row=3u+(i&15u);
    tf(col,row,(k==6u)?C_ERR:(k==7u)?C_HDR:C_LAB,"%s",ring_kind[k]); tf(col+4,row,C_VAL,"%06X",e&0xFFFFFFu);
  }
}

static void page_mem(void){
  u32 r=mem_region,sz=r==5?md_cfg.size:region_size[r],base=r==0?0xFF0000u:r==4?0xA00000u:0u;
  tf(0,2,C_HDR,"%s",region_name[r]); tf(11,2,C_LAB,"SUM"); tf(15,2,C_VAL,"%08X",sum_region(r)); tf(25,2,C_LAB,"SIZE"); tf(30,2,C_VAL,"%X",sz);
  for(u32 row=0;row<16u;row++){
    u32 off=mem_addr+row*8u; if(sz&&off>=sz) break;
    tf(0,3+row,C_LAB,"%06X",base+off);
    for(u32 i=0;i<8u;i++){ u32 o=off+i; if(sz&&o>=sz) break; tf(7+i*3,3+row,C_VAL,"%02X",mem_byte(r,o)); }
    char t[9]; for(u32 i=0;i<8u;i++){ u32 c=mem_byte(r,off+i); t[i]=(c>=32u&&c<127u)?(char)c:'.'; } t[8]=0; text(32,3+row,C_DIM,t);
  }
}

static void page_tiles(void){
  /* 30 columns x 19 rows of 8x8 tiles below a one-row header, game palette line tile_pal */
  for(u32 ty=0;ty<19u;ty++) for(u32 tx=0;tx<30u;tx++){
    u32 t=tile_base+ty*30u+tx; if(t>=2048u) continue;
    for(u32 y=0;y<8u;y++){
      u32 row=((u32)md_vram[t*16u+y*2u]<<16)|md_vram[t*16u+y*2u+1u];
      u32 lo=0,hi=0; for(u32 x=0;x<8u;x++){ u32 v=(row>>(28u-4u*x))&15u; u32 q=v?(tile_pal*16u+v):C_BG; if(x<4u) lo|=q<<(8u*x); else hi|=q<<(8u*(x-4u)); }
      volatile u32*o=(volatile u32*)(MD_PAGE(pg)+(8u+ty*8u+y)*240u+tx*8u); o[0]=lo; o[1]=hi;
    }
  }
  tf(0,0,C_HDR,"TILES %u-%u  PAL %u",tile_base,tile_base+569u,tile_pal);
  tf(26,0,C_DIM,"A=PAL U/D=ROW");
}

static void page_sys(void){
  u32 ms10=(md_last_frame_ticks/(16777u/10u)); /* ms*10 */
  tf(0,2,C_HDR,"%s",md_cfg.title);
  tf(0,3,C_LAB,"ENTRY %u  SIZE %X  FLAGS %X",md_cfg.index,md_cfg.size,md_cfg.flags);
  tf(0,4,C_LAB,"PAGE TABLE %08X",md_cfg.table);
  tf(0,5,C_LAB,"HDR:"); { char t[17]; for(u32 i=0;i<16u;i++){ u32 c=bus_read8(0x100u+i); t[i]=(c>=32u&&c<127u)?(char)c:'.'; } t[16]=0; text(5,5,C_VAL,t); }
  { char t[15]; for(u32 i=0;i<14u;i++){ u32 c=bus_read8(0x180u+i); t[i]=(c>=32u&&c<127u)?(char)c:'.'; } t[14]=0; tf(22,5,C_LAB,"SER"); text(26,5,C_VAL,t); }
  tf(0,6,C_LAB,"ROM CHK %04X  START %X END %X",((u32)bus_read8(0x18E)<<8)|bus_read8(0x18F),bus_read32(0x1A0),bus_read32(0x1A4));
  tf(0,7,C_LAB,"RESET SP %X PC %X",bus_read32(0),bus_read32(4));
  tf(0,9,C_LAB,"GUEST FRAME %u.%u MS  (16.7=FULL SPEED)",ms10/10u,ms10%10u);
  u32 rms10=(md_render_ticks/(16777u/10u)); tf(0,10,C_LAB,"LAST RENDER %u.%u MS  GBA CYC %u",rms10/10u,rms10%10u,md_render_ticks);
  tf(0,11,C_LAB,"RENDERED V:%u C:%u NOW V:%u C:%u",mdr_vgen,mdr_cgen,M.vgen,M.cgen);
  tf(0,12,C_LAB,"IMAGE %s  FRAMES SINCE DISP ON: %s",mdr_image_blank()?"BLANK":"HAS PIXELS",M.ev[EV_DISPON]==0xFFFFFFFFu?"NEVER":"YES");
  tf(0,14,C_LAB,"SUM RAM %08X VRAM %08X",sum_region(0),sum_region(1));
  tf(0,15,C_LAB,"SUM CRAM %08X VSRAM %08X",sum_region(2),sum_region(3));
  tf(0,16,C_LAB,"SUM Z80 %08X ROM %08X",sum_region(4),sum_region(5));
  tf(0,17,C_LAB,"KEYS %03X PAD %04X TH %02X",md_keys_raw(),(u32)M.pad,(u32)M.pad_th);
  tf(0,18,C_LAB,"UNMAPPED RD %u (%06X) WR %u (%06X)",M.n_unm_rd,M.last_unm_rd,M.n_unm_wr,M.last_unm_wr);
}

enum { M_RESUME,M_LIVE,M_STEPF,M_STEPI,M_RUNCTRL,M_RUNREG,M_RUNCRAM,M_RUNDISP,M_RUNVINT,M_RUNPC,M_RUNFR,M_FORCE,M_SPIN,M_RESET,M_EXIT,M_COUNT };
static void page_menu(void){
  static const char*const nm[M_COUNT]={"RESUME GAME","","STEP ONE FRAME","","RUN TO VDP CONTROL WRITE","RUN TO VDP REGISTER WRITE","RUN TO CRAM WRITE","RUN TO DISPLAY ENABLE","RUN TO VBLANK INTERRUPT","","","FORCE DISPLAY ENABLE","","RESET GAME","EXIT TO COLLECTION"};
  for(u32 i=0;i<M_COUNT;i++){
    u32 row=2+i,sel=(i==menu_sel);
    if(sel) fill_rect(0,row*8u-1u,240u,9u,C_SEL);
    text_bg=sel?C_SEL:C_BG;
    u32 c=sel?C_TXT:C_VAL;
    switch(i){
      case M_LIVE: tf(2,row,c,"EMULATION: %s",live?"LIVE (RUNNING)":"PAUSED"); break;
      case M_STEPI: tf(2,row,c,"STEP INSTRUCTIONS: %u",step_tab[step_sel]); break;
      case M_RUNPC: tf(2,row,c,"RUN TO PC = %06X",pc_target); if(pc_edit) fill_rect((2u+12u)*6u+(5u-pc_digit)*6u,row*8u+8u,5u,1u,C_ERR); break;
      case M_RUNFR: tf(2,row,c,"RUN FRAMES (NO DISPLAY): %u",frames_tab[frames_sel]); break;
      case M_SPIN: tf(2,row,c,"IDLE-LOOP FOLD: %s",C.spin_enable?"ON":"OFF"); break;
      default: tf(2,row,c,"%s",nm[i]); break;
    }
  }
  text_bg=C_BG;
  tf(0,18,C_HDR,"A=DO L/R=PAGE U/D=SEL LEFT/RIGHT=VALUE");
}

static void draw_screen(void){
  clear_page(C_BG);
  if(page_idx!=P_TILES){
    { u32 x=0; for(u32 i=0;i<P_COUNT;i++){ u32 n=0; while(page_name[i][n]) n++; tf(x,0,i==page_idx?C_TXT:C_DIM,"%s",page_name[i]); if(i==page_idx) fill_rect(x*6u,8,n*6u-1u,1,C_HDR); x+=n+1u; } }
    fill_rect(0,9,240,1,C_BOX);
    { char t[41]; u32 n=0; const char*s=md_cfg.title; while(*s&&n<24u) t[n++]=*s++; t[n]=0; text(0,1,C_HDR,t); tf(25,1,C_DIM,"F%u",M.frame); tf(34,1,live?C_OK:C_ERR,live?"LIVE":"PAUSED"); }
  }
  switch(page_idx){
    case P_CPU: page_cpu(); break; case P_VDP: page_vdp(); break; case P_TRACE: page_trace(); break;
    case P_MEM: page_mem(); break; case P_TILES: page_tiles(); break; case P_SYS: page_sys(); break; default: page_menu(); break;
  }
  if(page_idx!=P_MENU&&page_idx!=P_TILES) tf(0,19,C_DIM,"L/R PAGE  B MENU  START/SELECT RESUME");
  if(status_msg[0]) tf(0,19,C_ERR,"%s",status_msg);
}

/* ---------------- presenting the debug page ---------------- */
static void dbg_flip(void){
  md_set_dispcnt((u16)(0x0404u|(pg<<4))); md_page=pg;
}
static void redraw(void){
  pg=md_page^1u; draw_screen(); dbg_flip();
}

/* ---------------- run control ---------------- */
static u32 pad_keys(void){ return md_keys_raw(); }
static void do_frame_headless(void){ md_begin_frame_input(pad_keys()); md_run(0); }

static int abort_requested(void){ return (md_keys_raw()&K_B)!=0; }

static void msg(const char*s){ u32 i=0; for(;s[i]&&i<40u;i++) status_msg[i]=s[i]; status_msg[i]=0; }

/* kind: 0 ctrl wr, 1 reg wr, 2 cram wr, 3 display on, 4 vint, 5 pc */
static void run_to(u32 kind){
  u32 c0=M.n_ctrl_wr,r0=M.n_reg_wr,m0=M.n_cram_wr,v0=M.n_vint,f0=M.frame,lines=0; int found=0;
  u32 on0=(M.reg[1]&0x40u)!=0;
  if(kind==3u&&on0){ msg("DISPLAY IS ALREADY ON"); return; }
  md_begin_frame_input(pad_keys());
  for(;;){
    md_run(kind==5u?1u:2u);
    switch(kind){
      case 0: found=M.n_ctrl_wr!=c0; break; case 1: found=M.n_reg_wr!=r0; break; case 2: found=M.n_cram_wr!=m0; break;
      case 3: found=(M.reg[1]&0x40u)!=0; break; case 4: found=M.n_vint!=v0; break;
      default: found=(C.pc&0xFFFFFFu)==pc_target; break;
    }
    if(found) break;
    if(((++lines)&255u)==0u){
      if(abort_requested()){ msg("ABORTED (B)"); return; }
      if(kind==5u&&((lines&0xFFFFu)==0u)) { pg=md_page^1u; clear_page(C_BG); tf(1,8,C_HDR,"RUNNING TO PC... FRAME %u",M.frame); tf(1,10,C_DIM,"HOLD B TO ABORT"); dbg_flip(); }
      else if(kind!=5u&&((lines&0xFFFu)==0u)){ pg=md_page^1u; clear_page(C_BG); tf(1,8,C_HDR,"RUNNING... FRAME %u (%u LINES)",M.frame,lines); tf(1,10,C_DIM,"HOLD B TO ABORT"); dbg_flip(); }
      if(M.frame-f0>20000u){ msg("GAVE UP AFTER 20000 FRAMES"); return; }
    }
  }
  msg("STOPPED: CONDITION MET");
}

static void soft_exit(void){ md_soft_reset(); }

static void act_menu(void){
  switch(menu_sel){
    case M_RESUME: live=0; page_idx=0xFFu; break;
    case M_LIVE: live^=1u; break;
    case M_STEPF: md_begin_frame_input(pad_keys()); md_run(0); msg("STEPPED 1 FRAME"); break;
    case M_STEPI: { u32 n=step_tab[step_sel]; md_begin_frame_input(pad_keys()); for(u32 i=0;i<n;i++){ md_run(1); if((i&0xFFFu)==0xFFFu&&abort_requested()) break; } msg("STEPPED"); } break;
    case M_RUNCTRL: run_to(0); break; case M_RUNREG: run_to(1); break; case M_RUNCRAM: run_to(2); break;
    case M_RUNDISP: run_to(3); break; case M_RUNVINT: run_to(4); break;
    case M_RUNPC: if(pc_edit){ pc_edit=0; run_to(5); } else pc_edit=1; break;
    case M_RUNFR: { u32 n=frames_tab[frames_sel]; for(u32 i=0;i<n;i++){ do_frame_headless(); if((i&7u)==7u){ if(abort_requested()) break; pg=md_page^1u; clear_page(C_BG); tf(1,8,C_HDR,"RUNNING FRAME %u / %u",i+1u,n); dbg_flip(); } } msg("RUN COMPLETE"); } break;
    case M_FORCE: M.reg[1]|=0x40u; M.vgen++; msg("DISPLAY FORCED ON (REG 01 BIT 6)"); break;
    case M_SPIN: C.spin_enable^=1u; break;
    case M_RESET: md_reset_game(); msg("GAME RESET"); break;
    case M_EXIT: soft_exit(); break;
  }
}

/* ---------------- input ---------------- */
int dbg_open_request(u32 keys,u32 edge){ (void)keys; return (edge&K_SELECT)!=0; }

static void tile_clamp(void){ if(tile_base>2048u-570u+30u) tile_base=2048u-570u+30u; }

void dbg_run(int start_paused){
  u32 prev=md_keys_raw()|K_SELECT; (void)start_paused;
  live=0; page_idx=P_MENU; status_msg[0]=0;
  if(start_paused){ msg("PAUSED AT RESET VECTOR"); }
  redraw();
  u32 refresh=0;
  for(;;){
    u32 keys=md_keys_raw(), edge=keys&~prev; prev=keys;
    if((keys&(K_A|K_B|K_SELECT|K_START))==(K_A|K_B|K_SELECT|K_START)) soft_exit();
    int dirty=0;
    if(edge&K_START){ break; }
    if(edge&K_SELECT){ break; }
    if(edge&K_R){ page_idx=(page_idx+1u)%P_COUNT; dirty=1; msg(""); }
    if(edge&K_L){ page_idx=(page_idx+P_COUNT-1u)%P_COUNT; dirty=1; msg(""); }
    switch(page_idx){
      case P_MENU:
        if(pc_edit){
          if(edge&K_LEFT){ if(pc_digit<5u) pc_digit++; dirty=1; } if(edge&K_RIGHT){ if(pc_digit>0u) pc_digit--; dirty=1; }
          if(edge&K_UP){ pc_target=(pc_target&~(15u<<(pc_digit*4u)))|((((pc_target>>(pc_digit*4u))+1u)&15u)<<(pc_digit*4u)); dirty=1; }
          if(edge&K_DOWN){ pc_target=(pc_target&~(15u<<(pc_digit*4u)))|((((pc_target>>(pc_digit*4u))-1u)&15u)<<(pc_digit*4u)); dirty=1; }
          if(edge&K_B){ pc_edit=0; dirty=1; }
          if(edge&K_A){ act_menu(); dirty=1; }
          break;
        }
        if(edge&K_UP){ menu_sel=menu_sel?menu_sel-1u:M_COUNT-1u; dirty=1; }
        if(edge&K_DOWN){ menu_sel=(menu_sel+1u)%M_COUNT; dirty=1; }
        if(edge&(K_LEFT|K_RIGHT)){ u32 d=(edge&K_RIGHT)?1u:4u;
          if(menu_sel==M_STEPI) step_sel=(step_sel+d)%5u;
          else if(menu_sel==M_RUNFR) frames_sel=(frames_sel+d)%5u;
          else if(menu_sel==M_LIVE) live^=1u;
          else if(menu_sel==M_SPIN) C.spin_enable^=1u;
          dirty=1; }
        if(edge&K_A){ msg(""); act_menu(); dirty=1; if(page_idx==0xFFu) goto leave; }
        if(edge&K_B){ page_idx=P_CPU; dirty=1; }
        break;
      case P_MEM: {
        u32 sz=mem_region==5?md_cfg.size:region_size[mem_region]; u32 mul=(keys&K_B)?16u:1u;
        if(edge&K_DOWN){ mem_addr+=8u*mul; dirty=1; } if(edge&K_UP){ mem_addr-=8u*mul; dirty=1; }
        if(edge&K_RIGHT){ mem_addr+=0x80u*mul; dirty=1; } if(edge&K_LEFT){ mem_addr-=0x80u*mul; dirty=1; }
        if(edge&K_A){ mem_region=(mem_region+1u)%6u; mem_addr=0; dirty=1; }
        if(sz&&mem_addr>=sz) mem_addr=(mem_addr&0x80000000u)?0u:(sz>128u?(sz-128u)&~7u:0u);
        } break;
      case P_VDP:
        if(edge&K_A){ vdp_sub^=1u; dirty=1; }
        if(edge&K_B){ page_idx=P_MENU; dirty=1; }
        break;
      case P_TILES:
        if(edge&K_DOWN){ tile_base+=30u; tile_clamp(); dirty=1; } if(edge&K_UP){ tile_base=tile_base>=30u?tile_base-30u:0u; dirty=1; }
        if(edge&K_RIGHT){ tile_base+=570u; tile_clamp(); dirty=1; } if(edge&K_LEFT){ tile_base=tile_base>=570u?tile_base-570u:0u; dirty=1; }
        if(edge&K_A){ tile_pal=(tile_pal+1u)&3u; dirty=1; }
        break;
      default:
        if(edge&K_B){ page_idx=P_MENU; dirty=1; }
        break;
    }
    if(live){
      md_begin_frame_input(keys&~(K_SELECT|K_START|K_L|K_R)); md_run(0);
      if(++refresh>=12u){ refresh=0; dirty=1; }
    }
    if(dirty) redraw();
    md_wait_pace();
  }
leave:
  /* resume: force a fresh render of the game image */
  mdr_vgen=0xFFFFFFFFu; mdr_cgen=0xFFFFFFFFu;
  md_present_game();
  held_prev=0;
}

/* ---------------- status banner / boot panel ---------------- */
void dbg_init(void){
  volatile u16*pal=MD_PAL; for(u32 i=0;i<10u;i++) pal[240u+i]=dbg_pal[i];
  menu_sel=0; mem_region=0; mem_addr=0; tile_base=0; tile_pal=0; step_sel=0; frames_sel=2; pc_target=0; pc_digit=0; pc_edit=0;
  pc_target=bus_read32(4)&0xFFFFFFu;
}

void dbg_status_banner(unsigned page){
  if(!dbg_banner_enable) return;
  pg=page;
  u32 on=(M.reg[1]&0x40u)!=0;
  if(!on){
    /* the guest has its display off: say what it is doing instead of showing black */
    u32 never=(M.ev[EV_DISPON]==0xFFFFFFFFu);
    clear_page(C_BG);
    tf(0,0,C_HDR,"GENESIS RUNTIME");  tf(0,1,C_TXT,"%s",md_cfg.title);
    if(never) tf(0,3,C_VAL,"BOOTING - GAME DISPLAY IS OFF"); else tf(0,3,C_VAL,"GAME DISPLAY OFF FOR %u FRAMES",md_frames_off);
    tf(0,4,C_LAB,"FRAME %u  (%u SECONDS OF GAME TIME)",M.frame,M.frame/60u);
    tf(0,5,C_LAB,"PC %06X  SR %04X  IPL %u",C.pc&0xFFFFFFu,C.sr,(C.sr>>8)&7u);
    tf(0,6,C_LAB,"VDP WRITES CTRL %u VRAM %u CRAM %u",M.n_ctrl_wr,M.n_vram_wr,M.n_cram_wr);
    tf(0,7,C_LAB,"DMA %u  VINT ENABLED %s  TAKEN %u",M.n_dma,(M.reg[1]&0x20)?"YES":"NO",M.n_vint);
    tf(0,8,C_LAB,"IDLE FOLDS %u  STOPPED %u",C.spin_skips,(u32)C.stopped);
    if(fault_count()){ tf(0,10,C_ERR,"CPU FAULTS: %u",fault_count()); tf(0,11,C_ERR,"LAST %s AT %06X OP %04X",exc_name(C.last_exc_vec),C.last_exc_pc&0xFFFFFFu,C.last_exc_op); }
    else if(M.frame>60u&&M.n_ctrl_wr==0u) tf(0,10,C_ERR,"NO VDP ACTIVITY YET");
    else tf(0,10,C_DIM,"THE GAME TURNS ITS DISPLAY ON WHEN READY");
    tf(0,17,C_DIM,"SELECT = DEBUG MENU");
    tf(0,18,C_DIM,"A+B+START+SELECT = EXIT TO COLLECTION");
    return;
  }
  /* display on: only report faults */
  if(fault_count()&&M.ev[EV_EXC]!=0xFFFFFFFFu&&(M.frame-M.ev[EV_EXC])<900u){
    fill_rect(0,141,240,19,C_ERR); text_bg=C_ERR;
    tf(0,18,C_TXT,"CPU %s PC %06X OP %04X  (X%u)",exc_name(C.last_exc_vec),C.last_exc_pc&0xFFFFFFu,C.last_exc_op,fault_count());
    tf(0,19,C_TXT,"SELECT = DEBUG MENU"); text_bg=C_BG;
  } else if(C.stopped&&((C.sr>>8)&7u)==7u){
    fill_rect(0,141,240,19,C_ERR); text_bg=C_ERR; tf(0,18,C_TXT,"CPU STOPPED WITH INTERRUPTS MASKED"); tf(0,19,C_TXT,"SELECT = DEBUG MENU"); text_bg=C_BG;
  }
}
