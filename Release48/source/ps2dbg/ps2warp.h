/* PSII warp / test states (included by ps2dbg.c).
 *
 * Design
 *   The warp menu only *arms* a request (4 words at 0x03007E40, magic + checksum), then restarts PSII cleanly.  PSII boots
 *   normally (Sega logo, title).  Two tiny hooks in PSII's own statically translated title-screen code do the rest:
 *     thunk A  at 0x0839F2FC  START pressed on the title: if a warp is armed, continue into the game's *opening-demo*
 *                             path (MoveToOpeningScreen: game_screen = Level) instead of the new-game intro.
 *     thunk B  at 0x0839F376  last instruction group of that path: ps2warp_commit() overwrites the party / level RAM,
 *                             then PSII's own Level screen loads the map from level_index / y / x exactly as it does
 *                             after the real game's building exits.
 *   Without an armed request both thunks execute the displaced instructions only (title screen behaves as before).
 *
 *   RAM written (68k addresses, byte-reversed EWRAM mirror), all values from gen_ps2_state.py:
 *     character_data_buffer $C000..$C1FF  8 x 64 B: level 32 stats, equipment and items from the game's own tables
 *     party $C600..$C611, joined/next, money $C620, level_index/y/x $C640.., saved positions $C646..$C650,
 *     names $C660, event_flags $C710 = 0, controls_locked = 0, last-music byte $F640 = 0 (so the level music starts).
 *   Story flags ($C700 block) and chest flags are left at their new-game values: this is a sandbox for music, maps and
 *   battles, not a save.  Do NOT save over a real game from a warped state.
 */
#include "ps2state.h"

struct WDest { const char*name; const char*note; u16 level, y, x; };
/* coordinates are the game's own: Telepipe / spaceship landing points and the opening-demo start (ps2.asm) */
static const struct WDest wdest[]={
 { "DEZOLIS SKURE FIELD", "SILENT ZONE MUSIC", 0x03, 0x4F0, 0x3C0 },
 { "DEZOLIS AUKBA TOWN",  "TOWN MUSIC",         0x0A, 0x060, 0x070 },
 { "MOTAVIA PASEO",       "TOWN MUSIC",         0x04, 0x100, 0x1F0 },
 { "MOTAVIA WORLD MAP",   "FIELD MUSIC",        0x00, 0x520, 0x5B0 },
 { "CLIMATROL TOWER 7F",  "TOWER MUSIC",        0x3D, 0x1A0, 0x300 },
};
#define WD_COUNT (sizeof(wdest)/sizeof(wdest[0]))

_Static_assert(WD_COUNT<=WD_MAX,"too many warp destinations");

/* 68k RAM lives byte-reversed in EWRAM: address a -> 0x0200FFFF-(a&0xFFFF) */
static inline void w8(u32 a,u32 v){ R8(0x0200FFFFu-(a&0xFFFFu))=(u8)v; }
static void w16(u32 a,u32 v){ w8(a,v>>8); w8(a+1u,v); }
static void w32(u32 a,u32 v){ w16(a,v>>16); w16(a+2u,v); }

/* thunk A: is a warp armed? (non-zero -> take the opening-demo path) */
u32 ps2warp_check(void){ return wreq_valid(); }

/* thunk B: write the test state; PSII's Level screen loads the map next */
void ps2warp_commit(void){
  if(!wreq_valid()) return;
  const struct WDest*d=&wdest[WREQ->dest];
  WREQ->magic=0;
  for(u32 i=0;i<512u;i++) w8(0xC000u+i,ps2st_chars[i]);
  w16(0xC600u,PS2ST_PARTY_N-1u); w16(0xC602u,0); w16(0xC604u,6u); w16(0xC606u,6u);   /* followers (Rolf not counted), (loop var), joined, join_next */
  for(u32 i=0;i<5u;i++) w16(0xC608u+2u*i,i<PS2ST_PARTY_N?ps2st_party[i]:0u);
  w32(0xC620u,50000u);
  w16(0xC640u,d->level); w16(0xC642u,d->y); w16(0xC644u,d->x);
  w16(0xC646u,d->level); w16(0xC648u,d->y); w16(0xC64Au,d->x);          /* saved positions (Telepipe / building returns) */
  w16(0xC64Cu,d->level); w16(0xC64Eu,d->y); w16(0xC650u,d->x);
  w16(0xC652u,0);                                                       /* jet scooter off */
  for(u32 i=0;i<32u;i++) w8(0xC660u+i,ps2st_names[i]);
  w16(0xC710u,0);                                                       /* event_flags: no scripted event */
  w16(0xFFF0u,0);                                                       /* controls_locked = 0 */
  w8(0xF640u,0);                                                        /* last music request: let the level start its own */
  w16(0xF780u,0);
}

static void warp_menu(void){
  u32 cur=0,edge,rep; D->page=3;
  clear_screen(C_BG); header("WARP / TEST STATES","PARTY LV32");
  text(1,2,C_DIM,"ARMS A START STATE, THEN RESTARTS PSII.");
  text(1,3,C_DIM,"AT THE TITLE SCREEN PRESS START.");
  footer("A ARM WARP  B BACK");
  u32 dirty=1;
  for(;;){
    vsync(); poll(&edge,&rep);
    u32 nc=nav(cur,WD_COUNT,edge,rep&(K_UP|K_DOWN)); if(nc!=cur){ cur=nc; dirty=1; }
    if(edge&K_B) return;
    if(edge&K_A){
      /* confirmation page */
      clear_screen(C_BG); header("WARP / TEST STATES",0);
      text(1,3,C_OK,wdest[cur].name); text(1,4,C_DIM,wdest[cur].note);
      text(1,6,C_TXT,"PARTY  ROLF RUDO AMY HUGH  LV32");
      text(1,8,C_TXT,"PSII RESTARTS NOW. AT THE TITLE");
      text(1,9,C_TXT,"SCREEN PRESS START TO ENTER THE MAP.");
      text(1,11,C_ERR,"TEST STATE ONLY: STORY FLAGS ARE NOT");
      text(1,12,C_ERR,"SET. DO NOT SAVE OVER A REAL GAME.");
      footer("A GO  B CANCEL");
      for(;;){
        vsync(); poll(&edge,&rep);
        if(edge&K_B){ dirty=1; break; }
        if(edge&K_A){
          WREQ->magic=WMAGIC; WREQ->dest=cur; WREQ->chk=WMAGIC^cur^0xA5A5A5A5u; WREQ->fresh=1;
          restart_game();
        }
      }
      clear_screen(C_BG); header("WARP / TEST STATES","PARTY LV32");
      text(1,2,C_DIM,"ARMS A START STATE, THEN RESTARTS PSII.");
      text(1,3,C_DIM,"AT THE TITLE SCREEN PRESS START.");
      footer("A ARM WARP  B BACK");
    }
    if(dirty){
      for(u32 i=0;i<WD_COUNT;i++) list_row(5u+i,i==cur,wdest[i].level,wdest[i].name,"",0);
      dirty=0;
    }
  }
}
