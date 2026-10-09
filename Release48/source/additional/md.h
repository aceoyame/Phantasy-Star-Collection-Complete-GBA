/* Genesis runtime shared definitions (CP08).
 *
 * Modules:
 *   m68k.c / m68k_ref.c  CPU (fast core + reference/slow path)
 *   md.c                 bus, VDP, scheduler               (IWRAM)
 *   mdrender.c           VDP -> Mode 4 renderer             (IWRAM)
 *   mdmain.c             start-up, frame loop, pacing, status banner
 */
#pragma once
#include "m68k.h"
#include "mdplat.h"

/* ---- machine state ---- */
extern u16 md_ram[32768];     /* 64 KiB work RAM, logical big-endian words: byte a is ((u8*)md_ram)[a^1] */
extern u16 md_vram[32768];    /* 64 KiB VRAM, same layout */
extern u8  md_zram[8192];
extern u16 md_cram[64];
extern u16 md_vsram[64];      /* 40 used */

enum { EV_CTRL, EV_REG, EV_VRAM, EV_CRAM, EV_VSRAM, EV_DMA, EV_DISPON, EV_VINT_EN, EV_VINT, EV_SPIN, EV_VISIBLE, EV_EXC, EV_COUNT };

typedef struct {
  u8  reg[32];
  u8  pending, fill_pending, code, pad_th, ctrl1, sram_on, vint_pending, hint_pending;
  u32 rfull;                            /* renderer: a change that needs a full redraw (registers, VSRAM) */
  u16 pad, first;
  u32 addr;
  s32 line, hc;
  u32 frame, line_start;
  u32 vgen, cgen;                       /* renderer dirty generations (VRAM/regs/VSRAM; CRAM) */
  /* scheduler */
  u32 line_end; s32 next_line; u32 line_active;
  /* diagnostics */
  u32 n_vram_wr,n_cram_wr,n_vsram_wr,n_reg_wr,n_ctrl_wr,n_data_wr,n_data_rd,n_status_rd,n_hv_rd,
      n_dma,n_dma_words,n_fill,n_copy,n_vint,n_hint,n_io_rd,n_io_wr,n_z80_wr,n_ym_wr,n_psg_wr,n_unm_rd,n_unm_wr;
  u32 last_unm_rd,last_unm_wr;
  u32 dma_src,dma_len,dma_dst;
  u32 ev[EV_COUNT];                     /* frame number of first occurrence, 0xFFFFFFFF = never */
  u32 evctx[EV_COUNT];                  /* most recent taken branch (RK_<<24|pc) at that moment */
} MD;
extern MD M HOTDATA;

#define MD_EVENT(i) do{ if(M.ev[i]==0xFFFFFFFFu){ M.ev[i]=M.frame; M.evctx[i]=C.ring[(C.ring_pos-1u)&(M68K_RING-1u)]; } }while(0)

/* Registers that influence the rendered image. */
#define MD_REG_RENDER_MASK ((1u<<1)|(1u<<2)|(1u<<3)|(1u<<4)|(1u<<5)|(1u<<7)|(1u<<11)|(1u<<12)|(1u<<13)|(1u<<16)|(1u<<17)|(1u<<18))

#define MD_LINES 262
#define MD_LINE_CYCLES 488u

/* serial EEPROM (md.c) */
typedef struct { u8 mem[128]; u8 state, bit, shift, addr, rw, nack, rbyte, sda_in, sda_out, scl, rise, armed; } EEP;
extern EEP md_ee;
extern u32 md_vdirty[64];
void md_eeprom_store(u32 addr,u32 v);                  /* mdmain.c: mirror one EEPROM byte to GBA SRAM */
void md_eeprom_load(void);                             /* mdmain.c: fill md_ee.mem from GBA SRAM (0xFF if never saved) */

/* ---- md.c ---- */
void md_init_machine(const u32*page_table,u32 size);   /* ROM page table (ptr|1 entries), clears RAM/VDP, resets CPU */
void md_reset_game(void);                              /* CPU reset + clear RAM/VDP, keep ROM and counters */
/* Run emulation.  single=0: until the end of the frame (returns 1).  single=1:
 * execute one instruction.  single=2: run to the end of the current scanline.
 * Single modes return 1 only if a frame boundary was passed, else 0. */
int  md_run(u32 single);
void md_begin_frame_input(u32 keys);                   /* latch GBA keys into the pad */
u32  md_vdp_status(void);
void md_vdp_write16(u32 v);                         /* direct data-port access, same bus side effects */
void md_vdp_write32(u32 v);                         /* two ordered data-port words */
int md_native_vram_copy(u32 src,u32 n);              /* guarded native longword transfer */

/* ---- mdrender.c ---- */
void mdr_init(void);
void mdr_render(unsigned page);                        /* guest image -> Mode 4 page, updates palette 0..63 */
extern u32 mdr_vgen, mdr_cgen;                         /* generations last rendered */
int  mdr_image_blank(void);                            /* last render produced a single colour */
extern u32 mdr_nonblank;                               /* #pixels != the first pixel in last render */

/* ---- mdmain.c ---- */
typedef struct {
  u32 table, size, flags, index;
  char title[49];
} MDCfg;
extern MDCfg md_cfg;
extern u32 md_page;                                    /* page currently displayed */
void md_present_game(void);                            /* render + flip */
void md_wait_pace(void);                               /* 59.7 Hz pacing against the CPU-clock timer */
void md_heartbeat(void);
void md_soft_reset(void) __attribute__((noreturn));    /* back to the collection */
extern u32 md_frame_ticks, md_render_ticks, md_last_frame_ticks, md_frames_off;

/* Code-window writes must use this generation update, including native spans.
 * A single epoch lets the dispatcher reuse byte validation between writes. */
extern u32 aot_rgen[128];
#ifdef AOT_ON
extern u32 aot_code_epoch;
void aot_epoch_wrap(void);
#define AOT_RGEN_ADD(index,count) do{ aot_rgen[(index)]+=(count); if(++aot_code_epoch==0u)aot_epoch_wrap(); }while(0)
#else
#define AOT_RGEN_ADD(index,count) (aot_rgen[(index)]+=(count))
#endif

int md_native_vsram_fill(u32 value,u32 n);
u32 md_native_scroll_batch(u32 room);

u32 md_native_rte(void);
