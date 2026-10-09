/* MC68000 core interface (CP08).
 *
 * Two implementations share this state and bus interface:
 *   m68k.c      - fast interpreter used by the GBA runtime (hot code in IWRAM)
 *   m68k_ref.c  - original compact interpreter.  It is kept as (a) the slow
 *                 path for rare instructions and (b) the oracle that
 *                 tools_host/md_host/fuzz_cpu.c compares the fast core against.
 *
 * Architectural cycle accounting is deliberately identical between the two.
 */
#pragma once
#include <stdint.h>
typedef int8_t s8; typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32; typedef int32_t s32; typedef int16_t s16; typedef uint64_t u64;

#define M68K_RING 32u
/* Core state and tables live in IWRAM on the GBA (section .hotdata, see build script). */
#define M68K_ROMFN __attribute__((section(".text.rom")))
#define M68K_HOT __attribute__((section(".hotdata")))

typedef struct {
  union { struct { u32 d[8], a[8]; }; u32 regs[16]; };
  u32 usp, ssp;
  u32 pc;
  u32 cycles;            /* wraps; compare with (s32)(a-b) */
  u16 sr;
  u8  stopped;
  u8  spin_enable;       /* 1: allow idle-spin fast forward (fast core only) */
  /* Diagnostics: exceptions are never silent. */
  u32 exc_count, illegal_count, linea_count, linef_count, trap_count, zdiv_count, priv_count;
  u32 last_exc_vec, last_exc_pc, last_exc_op, last_exc_sr;
  u32 irq_taken;
  /* Side-effect counter: every memory write and every VDP/IO port access that
   * can change machine state bumps this.  Used only for spin detection. */
  u32 fx;
  /* spin detector */
  u32 spin_tgt, spin_fx, spin_cyc, spin_valid;
  u32 spin_lrd;          /* line-dependent read counter at the last loop boundary */
  u32 lrd;               /* VDP status reads (their value depends on the scanline) */
  u32 idle_ok;           /* set when this m68k_exec call fast-forwarded a loop whose iterations touch nothing line/time dependent */
  u32 rcp_per, rcp;      /* cached reciprocal for the fast-forward division */
  u32 spin_wait, spin_back;  /* detector back-off: loops whose registers keep changing are re-examined only every few iterations */
  u32 spin_regs[17];
  u32 spin_skips;        /* number of fast-forwards taken */
  u32 spin_cycles;       /* cycles skipped */
  /* Recent control-flow transfers: low 24 bits target PC, top byte kind. */
  u32 ring[M68K_RING]; u32 ring_pos;
  /* instruction counter is only maintained by m68k_step_traced() */
  u32 steps;
  u32 insns_hi;          /* not maintained by the fast loop */
} M68K;
extern M68K C;
/* Native longword MOVEM in aligned data RAM; 0 requests the bus fallback.
 * mode: 0 store forward, 1 load forward, 2 store predecrement. */
u32 m68k_movem_ram(u32 address,u32 mask,u32 mode);

/* Ring kinds (top byte) */
#define RK_BRA 1u
#define RK_JSR 2u
#define RK_RET 3u
#define RK_JMP 4u
#define RK_DBF 5u
#define RK_EXC 6u
#define RK_IRQ 7u

/* Bus implemented by md.c (slow path).  The fast core inlines ROM and RAM. */
u8 bus_read8(u32 a); u16 bus_read16(u32 a); u32 bus_read32(u32 a);
void bus_write8(u32 a,u8 v); void bus_write16(u32 a,u16 v); void bus_write32(u32 a,u32 v);

/* Memory view for the inline fast paths (set up by md.c). */
extern const u8* m68k_romp[128];   /* 4 KiB page pointers */
extern u32 m68k_romsize;
extern u16* m68k_ram;              /* 64 KiB as host-order 16-bit words */

void m68k_init_tables(void) M68K_ROMFN;       /* call once before m68k_exec */
void m68k_reset(void);
void m68k_set_sr(u16 v);
void m68k_exception(int vec);
void m68k_interrupt(int level);

/* Reference single step (slow, complete). */
void ref_step(void);
/* Execute until cycles >= limit (at least one instruction), or until the CPU
 * executes STOP / changes the interrupt mask (the scheduler must re-evaluate
 * pending interrupts when this returns early; it always may). */
void m68k_exec(u32 limit);
/* Execute exactly one instruction with the fast core. */
static inline void m68k_step(void){ m68k_exec(C.cycles+1u); }
