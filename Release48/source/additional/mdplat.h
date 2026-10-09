/* Platform layer for the Genesis runtime.  The same sources build for the GBA
 * (default) and for the PC test harness (-DMD_HOST, see tools_host/md_host). */
#pragma once
#include <stdint.h>
#include <stddef.h>

#ifdef MD_HOST
extern uint16_t md_h_pal[256];
extern uint8_t  md_h_fb[2*0xA000];
extern uint16_t md_h_dispcnt;
extern uint32_t md_h_keys;        /* active-high GBA key bits */
extern uint32_t md_h_ticks;
extern uint32_t md_h_hb[32];
extern int      md_h_reset_requested;
#define MD_PAL        ((volatile uint16_t*)md_h_pal)
#define MD_PAGE(n)    ((volatile uint8_t*)(md_h_fb+(n)*0xA000))
#define MD_HB         ((volatile uint32_t*)md_h_hb)
static inline uint32_t md_keys_raw(void){ return md_h_keys; }
static inline void     md_set_dispcnt(uint16_t v){ md_h_dispcnt=v; }
static inline uint32_t md_ticks(void){ return md_h_ticks; }
#else
#define MD_PAL        ((volatile uint16_t*)0x05000000)
#define MD_PAGE(n)    ((volatile uint8_t*)(0x06000000+(n)*0xA000))
#define MD_HB         ((volatile uint32_t*)0x0203e000)
#define MD_REG16(a)   (*(volatile uint16_t*)(a))
static inline uint32_t md_keys_raw(void){ return (~MD_REG16(0x04000130))&1023u; }
static inline void     md_set_dispcnt(uint16_t v){ MD_REG16(0x04000000)=v; }
/* Free-running 32-bit counter at the CPU clock: Timer2 (/1) cascaded into Timer3. */
static inline uint32_t md_ticks(void){
  uint32_t hi,lo,hi2;
  do{ hi=MD_REG16(0x0400010c); lo=MD_REG16(0x04000108); hi2=MD_REG16(0x0400010c);}while(hi!=hi2);
  return (hi<<16)|lo;
}
#endif

/* GBA key bits (active high, after inversion) */
#define K_A 1u
#define K_B 2u
#define K_SELECT 4u
#define K_START 8u
#define K_RIGHT 16u
#define K_LEFT 32u
#define K_UP 64u
#define K_DOWN 128u
#define K_R 256u
#define K_L 512u

#define ROMFN __attribute__((section(".text.rom")))   /* cold code inside an object whose .text goes to IWRAM */
#define HOT __attribute__((section(".hot")))
#define HOTDATA __attribute__((section(".hotdata")))
