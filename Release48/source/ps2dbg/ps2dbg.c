/* Phantasy Star II (PSC native) boot-time debug menu -- CP08.
 *
 * How it is entered
 *   Hold L+R while PSII starts (press A on the PSII card while holding L and R).  Normal launches never see it.
 *   The hook runs once, after PSII's own init and before its first frame (see thunk.s / build_cp08.py).
 *
 * What it does
 *   MUSIC TEST       every song of the PSII sound table, played through the game's own sound engine
 *                    (writes the engine's pending-music word exactly like the game's sound-queue code does)
 *   SOUND EFFECTS    every SFX id
 *   WARP             arms a start state (party, level, position); PSII restarts and boots normally, and at the title
 *                    screen START enters the chosen map through the game's own level loader (see ps2warp.h)
 *   START GAME       clean restart of PSII (RegisterRamReset + jump to the PSII entry) -- identical to a normal launch
 *
 * Nothing here runs unless the chord is held (or a warp request left by this menu is pending), so normal slot-II
 * play is the retail code path.  The module keeps no state in the game's RAM: its variables live at 0x0203C000.. which
 * PSII never references (the warp request is 4 words at 0x03007E40, in the IWRAM tail PSII never touches).
 */
#include <stdint.h>
#include <stdarg.h>
typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32; typedef int32_t s32;
#define R8(a)  (*(volatile u8*)(a))
#define R16(a) (*(volatile u16*)(a))
#define R32(a) (*(volatile u32*)(a))

#define K_A 0x001u
#define K_B 0x002u
#define K_SEL 0x004u
#define K_START 0x008u
#define K_RIGHT 0x010u
#define K_LEFT 0x020u
#define K_UP 0x040u
#define K_DOWN 0x080u
#define K_R 0x100u
#define K_L 0x200u

/* PSII native sound engine variables (found from the VBlank sound tick at 0x08394E78):
 *   0x02019534  pending music request (ID 0x81..0x97; 0x80 = silence; -1 = none)  -> SongStart(id-0x80)
 *   0x02019538  pending sound-effect request (ID 0xA0..0xD8; -1 = none)
 */
#define SND_MUSIC_REQ (*(volatile s32*)0x02019534)
#define SND_SFX_REQ   (*(volatile s32*)0x02019538)
/* The game's own entry point is the 68k sound queue byte $FFFFD204 (the translated DoSoundQueue at 0x08390200 reads it every
 * VBlank, converts 0x81..0x97 / 0xA0..0xD8 into the two request words above and writes 0x80 back; 0 or 0xE0 = stop all).
 * 68k RAM is mirrored byte-reversed at 0x0200FFFF-(addr&0xFFFF). We post requests through the queue exactly as the game does. */
#define SQ (*(volatile u8*)0x02002DFB)
#define GAME_VIDEO_LOCK (*(volatile u32*)0x02038000)   /* non-zero: PSII's VBlank handler leaves palette/OAM/scroll alone */

extern u32 __bss_start[], __bss_end[];
struct Dbg { u32 magic, page, cur, top, music, sfx, frames, keys, stop_cnt; };
#define D ((volatile struct Dbg*)0x0203C000)

#include "font5x7.h"
#include "sfxnames.h"

/* ------------------------------------------------------------------ drawing (Mode 4, page 0, 240x160, 8-bit) */
enum { C_BG, C_TXT, C_VAL, C_LAB, C_ERR, C_HDR, C_DIM, C_BOX, C_SEL, C_OK };
static const u16 pal[10]={ 0x2000, 0x7FFF, 0x2FE5, 0x03FF, 0x001F, 0x7FE0, 0x4210, 0x7C00, 0x5800, 0x03E0 };
#define PAGE ((volatile u8*)0x06000000)
static u32 text_bg;

static inline void px_row(volatile u8*row,u32 x,u32 c){
  volatile u16*p=(volatile u16*)(row+(x&~1u)); u32 q=*p;
  *p=(u16)((x&1u)?((q&0xFFu)|(c<<8)):((q&0xFF00u)|c));
}
static void fill_rect(u32 x,u32 y,u32 w,u32 h,u32 c){
  if(x>=240u||y>=160u) return;
  if(x+w>240u) w=240u-x;
  if(y+h>160u) h=160u-y;
  u32 xe=x+w, c2=c*0x0101u, c4=c*0x01010101u;
  for(u32 j=0;j<h;j++){
    volatile u8*row=PAGE+(y+j)*240u; u32 xx=x;
    if((xx&1u)&&xx<xe){ px_row(row,xx,c); xx++; }
    if((xx&2u)&&xx+2u<=xe){ *(volatile u16*)(row+xx)=(u16)c2; xx+=2u; }
    while(xx+4u<=xe){ *(volatile u32*)(row+xx)=c4; xx+=4u; }
    if(xx+2u<=xe){ *(volatile u16*)(row+xx)=(u16)c2; xx+=2u; }
    if(xx<xe) px_row(row,xx,c);
  }
}
static void clear_screen(u32 c){ text_bg=c; fill_rect(0,0,240,160,c); }
static void glyph(u32 x,u32 y,u32 ch,u32 c){
  if(ch<32u||ch>126u) ch='?';
  const u8*g=font5x7[ch-32u]; u32 bg=text_bg;
  volatile u8*row=PAGE+y*240u+x;
  for(u32 r=0;r<7u;r++,row+=240u){
    u32 p0=((g[0]>>r)&1u)?c:bg, p1=((g[1]>>r)&1u)?c:bg, p2=((g[2]>>r)&1u)?c:bg, p3=((g[3]>>r)&1u)?c:bg, p4=((g[4]>>r)&1u)?c:bg;
    ((volatile u16*)row)[0]=(u16)(p0|(p1<<8)); ((volatile u16*)row)[1]=(u16)(p2|(p3<<8)); ((volatile u16*)row)[2]=(u16)(p4|(bg<<8));
  }
}
static void text(u32 col,u32 row,u32 c,const char*s){
  u32 x=col*6u,y=row*8u; for(;*s;s++){ if(x+5u>240u) break; glyph(x,y,(u32)(unsigned char)*s,c); x+=6u; }
}
static u32 put_hex(char*o,u32 v,u32 w){ u32 n=0; char t[8]; if(!v) t[n++]='0'; while(v){ u32 d=v&15u; t[n++]=(char)(d<10u?'0'+d:'A'+d-10u); v>>=4; } u32 k=0; while(n+k<w) o[k++]='0'; while(n) o[k++]=t[--n]; return k; }
static void row_clear(u32 row,u32 bg){ fill_rect(0,row*8u,240,8,bg); }
/* one list row: "NN NAME........ USE" */
static void list_row(u32 row,u32 sel,u32 id,const char*name,const char*use,u32 mark){
  u32 bg=sel?C_SEL:C_BG; text_bg=bg; row_clear(row,bg);
  char b[8]; u32 n=put_hex(b,id,2); b[n]=0; text(1,row,sel?C_HDR:C_LAB,b);
  text(4,row,sel?C_TXT:C_VAL,name);
  if(use&&use[0]) text(19,row,sel?C_TXT:C_DIM,use);
  if(mark) text(0,row,C_OK,">");
  text_bg=C_BG;
}

/* ------------------------------------------------------------------ hardware helpers */
static inline u32 held(void){ return (~R16(0x04000130))&0x3FFu; }
static void vsync(void){ while(R16(0x04000006)>=160u){} while(R16(0x04000006)<160u){} }

static void video_init(void){
  GAME_VIDEO_LOCK=1;                       /* PSII's VBlank handler: no palette / OAM / scroll writes while this is set */
  R16(0x04000000)=0x0080;                  /* forced blank while reprogramming */
  R16(0x0400004C)=0; R16(0x04000048)=0; R16(0x0400004A)=0; R16(0x04000050)=0; R16(0x04000052)=0; R16(0x04000054)=0;
  R16(0x0400000C)=0;                       /* BG2: Mode 4 ignores char/screen base */
  R16(0x04000020)=0x100; R16(0x04000022)=0; R16(0x04000024)=0; R16(0x04000026)=0x100; R32(0x04000028)=0; R32(0x0400002C)=0;
  for(u32 i=0;i<10u;i++) R16(0x05000000+2u*i)=pal[i];
  R16(0x05000000+2u*255u)=pal[0];
  clear_screen(C_BG);
  R16(0x04000000)=0x0404;                  /* Mode 4, BG2, page 0, no OBJ, no windows */
}

/* Armed warp request (see ps2warp.h): 4 words in the BIOS-protected tail of IWRAM (0x03007E40).  PSII's own init clears
 * every byte of EWRAM and most of IWRAM, but never touches 0x03007E00..0x03007EFF (below its IRQ stack), and RegisterRamReset
 * never clears it either, so the request survives the restart that follows arming it.  magic + checksum guard against the
 * random contents of a cold boot; `fresh` marks the restart that follows arming so a stale request can be told apart. */
struct WReq { u32 magic, dest, chk, fresh; };
#define WREQ ((volatile struct WReq*)0x03007E40)
#define WMAGIC 0x50524157u                       /* 'WARP' */
#define WD_MAX 16u
static u32 wreq_valid(void){
  return WREQ->magic==WMAGIC && WREQ->dest<WD_MAX && WREQ->chk==(WMAGIC^WREQ->dest^0xA5A5A5A5u);
}

/* Clean restart of PSII (identical to a fresh launch). */
static void __attribute__((noreturn)) restart_game(void){
  R16(0x04000208)=0; R16(0x04000200)=0; R16(0x04000202)=0xFFFFu;
  R16(0x040000BA)=0; R16(0x040000C6)=0; R16(0x040000D2)=0; R16(0x040000DE)=0;
  R16(0x04000102)=0; R16(0x04000106)=0; R16(0x0400010A)=0; R16(0x0400010E)=0;
  R16(0x04000084)=0;
  R16(0x04000000)=0x0080;
  __asm__ volatile("movs r0,#0x9d\n swi 1" ::: "r0","r1","r2","r3","memory");   /* EWRAM, palette, VRAM, OAM, I/O (not IWRAM/SIO/sound) */
  ((void(*)(void))0x08390000u)();
  for(;;){}
}

/* ------------------------------------------------------------------ menus */
#define MUS_COUNT 23u
static const char*const mus_name[MUS_COUNT]={
 "PHANTASY","RESTORATION","SILENT ZONE","PLEASURE","EXCITING TOWN","SECRET WAYS","ADVANCED","MYSTERY","VIOLATION","UNDER",
 "STEP UP","BRACKY NEWS","MY HOME","PRESSURE","DREAM","POWER","EXCLAIM","RISE OR FALL","PLACE OF DEATH","MOVEMENT",
 "OVER","MOVEMENT VER.2","NEVER DREAM" };
/* where the original game plays each song (from the level / building music tables in the disassembly) */
static const char*const mus_use[MUS_COUNT]={
 "","MOTAVIA FIELD, TUBES","DEZOLIS FIELD, SKURE","MOTAVIA TOWNS","DEZOLIS TOWNS","UZO/CREVICE/WATER","TOWERS/LABS/CLIMATRL",
 "DAMS/CTRL TWR/GAIRA","NAVAL/IKUTO/GUARON","NOAH","SHOPS/HOSPITAL/SAVE","CLONE LAB/WEAPON/ARM","ROLF'S HOUSE",
 "C.TOWER/LIBRARY","TYLER'S SPACESHIP","ESPER MANSION","","","","","GAME OVER","","" };

#define ROWS 16u
static void header(const char*t,const char*sub){
  fill_rect(0,0,240,8,C_BOX); text_bg=C_BOX; text(1,0,C_TXT,t); text_bg=C_BG;
  if(sub){ u32 n=0; while(sub[n]) n++; text_bg=C_BOX; text(39u-n,0,C_HDR,sub); text_bg=C_BG; }
}
static void footer(const char*t){ fill_rect(0,152,240,8,C_BOX); text_bg=C_BOX; text(1,19,C_TXT,t); text_bg=C_BG; }

static u32 nav(u32 cur,u32 count,u32 keys_edge,u32 rep_edge){
  (void)keys_edge;
  if(rep_edge&K_UP)   cur=cur?cur-1u:count-1u;
  if(rep_edge&K_DOWN) cur=(cur+1u<count)?cur+1u:0u;
  if(rep_edge&K_LEFT){ cur=(cur>=8u)?cur-8u:0u; }
  if(rep_edge&K_RIGHT){ cur=(cur+8u<count)?cur+8u:count-1u; }
  return cur;
}
static u32 prev_keys, hold_cnt;
static u32 poll(u32*edge,u32*rep){
  if(D->stop_cnt&&--D->stop_cnt==0) SQ=0x80;           /* a 'stop all' (queue byte 0) lasts two frames, then the queue idles again */
  u32 k=held(); u32 e=k&~prev_keys; u32 r=e;
  if(k&(K_UP|K_DOWN|K_LEFT|K_RIGHT)){ if(k==prev_keys){ hold_cnt++; if(hold_cnt>=24u&&((hold_cnt&3u)==0u)) r|=k&(K_UP|K_DOWN|K_LEFT|K_RIGHT); } else hold_cnt=0; } else hold_cnt=0;
  prev_keys=k; *edge=e; *rep=r; D->frames++; D->keys=k; return k;
}
static u32 scroll_fix(u32 cur,u32 top,u32 count){
  if(cur<top) top=cur; if(cur>=top+ROWS) top=cur+1u-ROWS; if(top+ROWS>count) top=count>ROWS?count-ROWS:0u; return top;
}

static void now_playing(u32 row,const char*what,u32 id,const char*name){
  fill_rect(0,row*8u,240,8,C_BG); text_bg=C_BG; char b[40]; u32 n=0;
  const char*p=what; while(*p) b[n++]=*p++; b[n++]=' ';
  if(id){ n+=put_hex(b+n,id,2); b[n++]=' '; p=name; while(*p&&n<38u) b[n++]=*p++; } else { p="-"; while(*p) b[n++]=*p++; }
  b[n]=0; text(1,row,C_OK,b);
}

static void music_menu(void){
  u32 cur=D->music?D->music-0x81u:0u, top=0; u32 edge,rep; D->page=1;
  clear_screen(C_BG); header("MUSIC TEST","PSII SOUND TABLE"); footer("A PLAY  START STOP  L/R PAGE  B BACK");
  u32 dirty=1;
  for(;;){
    vsync(); poll(&edge,&rep);
    u32 nc=nav(cur,MUS_COUNT,edge,rep); if(nc!=cur){ cur=nc; dirty=1; }
    if(edge&K_A){ D->music=0x81u+cur; SQ=(u8)(0x81u+cur); dirty=1; }
    if(edge&K_START){ D->music=0; SQ=0; D->stop_cnt=3; dirty=1; }
    if(edge&K_L){ cur=cur>=ROWS?cur-ROWS:0u; dirty=1; }
    if(edge&K_R){ cur=cur+ROWS<MUS_COUNT?cur+ROWS:MUS_COUNT-1u; dirty=1; }
    if(edge&K_B){ return; }
    if(dirty){
      top=scroll_fix(cur,top,MUS_COUNT);
      for(u32 i=0;i<ROWS&&top+i<MUS_COUNT;i++){ u32 k=top+i; list_row(1u+i,k==cur,0x81u+k,mus_name[k],mus_use[k],D->music==0x81u+k); }
      now_playing(18,"NOW PLAYING",D->music,D->music?mus_name[D->music-0x81u]:"");
      dirty=0;
    }
  }
}

static u32 sfx_id(u32 i){ return i==SFX_COUNT-1u?0xD8u:SFX_FIRST+i; }
static void sfx_menu(void){
  u32 cur=D->sfx?0u:0u, top=0; u32 edge,rep; D->page=2;
  clear_screen(C_BG); header("SOUND EFFECT TEST","PSII SOUND TABLE"); footer("A PLAY  L/R PAGE  B BACK");
  u32 dirty=1;
  for(;;){
    vsync(); poll(&edge,&rep);
    u32 nc=nav(cur,SFX_COUNT,edge,rep); if(nc!=cur){ cur=nc; dirty=1; }
    if(edge&K_A){ D->sfx=sfx_id(cur); SQ=(u8)sfx_id(cur); dirty=1; }
    if(edge&K_L){ cur=cur>=ROWS?cur-ROWS:0u; dirty=1; }
    if(edge&K_R){ cur=cur+ROWS<SFX_COUNT?cur+ROWS:SFX_COUNT-1u; dirty=1; }
    if(edge&K_B) return;
    if(dirty){
      top=scroll_fix(cur,top,SFX_COUNT);
      for(u32 i=0;i<ROWS&&top+i<SFX_COUNT;i++){ u32 k=top+i; list_row(1u+i,k==cur,sfx_id(k),sfx_name[k],0,D->sfx==sfx_id(k)); }
      now_playing(18,"LAST SFX",D->sfx,D->sfx?sfx_name[D->sfx==0xD8u?SFX_COUNT-1u:D->sfx-SFX_FIRST]:"");
      dirty=0;
    }
  }
}


#include "ps2warp.h"

static void main_menu(void){
  static const char*const items[]={ "MUSIC TEST","SOUND EFFECT TEST","WARP / TEST STATES","START GAME NORMALLY" };
  const u32 n=4; u32 cur=0,edge,rep; D->page=0;
  clear_screen(C_BG); header("PHANTASY STAR II DEBUG","CP08");
  text(1,3,C_DIM,"HOLD L+R AT LAUNCH TO OPEN THIS MENU"); text(1,4,C_DIM,"NORMAL LAUNCH IS UNCHANGED");
  footer("UP/DOWN  A SELECT  B START GAME");
  u32 dirty=1;
  /* wait for the launch buttons to be released so they cannot act as menu input */
  while(held()&(K_A|K_B|K_START|K_SEL|K_L|K_R)){ vsync(); }
  prev_keys=held(); hold_cnt=0;
  for(;;){
    vsync(); poll(&edge,&rep);
    u32 nc=nav(cur,n,edge,rep&(K_UP|K_DOWN)); if(nc!=cur){ cur=nc; dirty=1; }
    if(edge&K_B){ WREQ->magic=0; restart_game(); }
    if(edge&(K_A|K_START)){
      if(cur==0){ music_menu(); }
      else if(cur==1){ sfx_menu(); }
      else if(cur==2){ warp_menu(); }
      else { WREQ->magic=0; restart_game(); }
      clear_screen(C_BG); header("PHANTASY STAR II DEBUG","CP08");
      text(1,3,C_DIM,"HOLD L+R AT LAUNCH TO OPEN THIS MENU"); text(1,4,C_DIM,"NORMAL LAUNCH IS UNCHANGED");
      footer("UP/DOWN  A SELECT  B START GAME"); dirty=1; D->page=0;
    }
    if(dirty){
      for(u32 i=0;i<n;i++){ u32 bg=(i==cur)?C_SEL:C_BG; text_bg=bg; row_clear(7u+2u*i,bg); text(3,7u+2u*i,i==cur?C_TXT:C_VAL,items[i]); if(i==cur) text(1,7u+2u*i,C_HDR,">"); text_bg=C_BG; }
      dirty=0;
    }
  }
}

/* called from thunk.s right after PSII's init, before its first frame */
void ps2dbg_hook(void){
  if(wreq_valid()){
    if(WREQ->fresh){ WREQ->fresh=0; return; }   /* restart that follows arming a warp: stay armed, boot normally */
    WREQ->magic=0;                              /* stale request from an earlier launch: ignore it */
  }
  if((held()&(K_L|K_R))!=(K_L|K_R)) return;
  for(volatile u32*z=(volatile u32*)__bss_start;z<(volatile u32*)__bss_end;z++) *z=0;
  D->magic=0x44325350u;                    /* 'PS2D' */
  D->music=0; D->sfx=0; D->frames=0;
  SQ=0x80;                                 /* idle sound queue (the game does this itself once its first frame runs) */
  video_init();
  main_menu();
}
