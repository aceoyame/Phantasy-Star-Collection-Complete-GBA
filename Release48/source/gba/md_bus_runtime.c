#include "md_bus_runtime.h"
#include "md_io_runtime.h"
extern const uint8_t ps4_reversed_rom[];
static uint8_t* const work_ram=(uint8_t*)0x02000000;
static uint8_t* const vram=(uint8_t*)0x02010000;
static uint16_t* const cram=(uint16_t*)0x02023000;
static uint16_t* const vsram=(uint16_t*)0x02023100;
#define REG_WAITCNT (*(volatile uint16_t*)0x04000204)
#ifndef PS4_SRAM_PHYS_BASE
#define PS4_SRAM_PHYS_BASE 0u
#endif
#if PS4_SRAM_PHYS_BASE > 0xE000u
#error "PS4_SRAM_PHYS_BASE outside 64 KiB GamePak SRAM window"
#endif
static volatile uint8_t* const gba_sram=(volatile uint8_t*)(0x0E000000u + PS4_SRAM_PHYS_BASE);
static uint8_t sram_mode;
static uint16_t vdp_first;
static uint8_t vdp_pending;
static uint16_t vdp_addr;
static uint8_t vdp_code;
static uint8_t vdp_autoinc;
static uint8_t vdp_regs[0x20];
static inline uint8_t rom8(uint32_t a){return ps4_reversed_rom[0x2FFFFFu-(a&0xFFFFFFu)];}
static inline int sram_lane(uint32_t a){return a>=0x200001u&&a<=0x203FFFu&&(a&1u);}
/* GBA GamePak SRAM reads must execute from internal WRAM because ROM and SRAM share
   the cartridge bus.  Keep both accessors in IWRAM for one portable hardware path. */
__attribute__((section(".iwram.text.gba_sram_read_phys"),noinline))
static uint8_t gba_sram_read_phys(uint16_t i){ return gba_sram[i]; }
__attribute__((section(".iwram.text.gba_sram_write_phys"),noinline))
static void gba_sram_write_phys(uint16_t i,uint8_t v){ gba_sram[i]=v; }
static inline uint8_t sram_read(uint32_t a){return gba_sram_read_phys((uint16_t)((a-0x200001u)>>1));}
static inline void sram_write(uint32_t a,uint8_t v){gba_sram_write_phys((uint16_t)((a-0x200001u)>>1),v);}
static void vdp_ctrl16(uint16_t v){
 if((v&0xC000u)==0x8000u){unsigned r=(v>>8)&0x1Fu;vdp_regs[r]=(uint8_t)v;if(r==15)vdp_autoinc=(uint8_t)v;vdp_pending=0;return;}
 if(!vdp_pending){vdp_first=v;vdp_pending=1;return;}
 vdp_addr=(uint16_t)((vdp_first&0x3FFFu)|((v&3u)<<14));
 vdp_code=(uint8_t)(((vdp_first>>14)&3u)|((v>>2)&0x3Cu));vdp_pending=0;
}
static void vdp_data16(uint16_t v){
 if(vdp_code==1u){vram[vdp_addr]=(uint8_t)(v>>8);vram[(uint16_t)(vdp_addr+1u)]=(uint8_t)v;}
 else if(vdp_code==3u){cram[(vdp_addr&0x7Eu)>>1]=v&0x0EEEu;}
 else if(vdp_code==5u){vsram[(vdp_addr&0x7Eu)>>1]=v;}
 vdp_addr=(uint16_t)(vdp_addr+vdp_autoinc);
}
static uint16_t vdp_data_read16(void){uint16_t v=0;if(vdp_code==0u)v=((uint16_t)vram[vdp_addr]<<8)|vram[(uint16_t)(vdp_addr+1u)];else if(vdp_code==8u)v=cram[(vdp_addr&0x7Eu)>>1];else if(vdp_code==4u)v=vsram[(vdp_addr&0x7Eu)>>1];vdp_addr=(uint16_t)(vdp_addr+vdp_autoinc);return v;}
void md_bus_reset(void){
 /* PSC uses the same optimized GamePak timing in both native PSII and PSIII
    runtimes: its common startup literal pools pair WAITCNT ($04000204) with
    $4014 (WS0 3/1 cycles + GamePak prefetch).  Keep that ROM timing/prefetch
    policy, but use SRAM wait bits = 3 (8 cycles), giving $4017.  The previous
    prototype only changed the SRAM bits and therefore left ROM execution at
    the GBA power-on 4/2-cycle, no-prefetch setting -- a large penalty for a
    ROM-resident static recompiler. */
 REG_WAITCNT=0x4017u;
 sram_mode=2;vdp_pending=0;vdp_addr=0;vdp_code=0;vdp_autoinc=2;for(unsigned i=0;i<32;i++)vdp_regs[i]=0;md_io_reset();
}
uint8_t md_read8(uint32_t a){a&=0xFFFFFFu;if((sram_mode&1u)&&sram_lane(a))return sram_read(a);if(a<0x300000u)return rom8(a);if(a>=0xFF0000u)return work_ram[a&0xFFFFu];if(a==0xA10001u)return 0x80u;if((a>=0xA10003u&&a<=0xA1000Du)&&(a&1u))return md_io_read8(a);if(a==0xA11100u||a==0xA11101u)return 0;return 0xFFu;}
uint16_t md_read16(uint32_t a){a&=0xFFFFFFu;if(a==0xC00000u)return vdp_data_read16();if(a==0xC00004u)return 0;return ((uint16_t)md_read8(a)<<8)|md_read8(a+1u);}
uint32_t md_read32(uint32_t a){return ((uint32_t)md_read16(a)<<16)|md_read16(a+2u);}
void md_write8(uint32_t a,uint8_t v){a&=0xFFFFFFu;if(a==0xA130F1u){sram_mode=v;return;}if((sram_mode&1u)&&!(sram_mode&2u)&&sram_lane(a)){sram_write(a,v);return;}if(a>=0xFF0000u){work_ram[a&0xFFFFu]=v;return;}if((a>=0xA10003u&&a<=0xA1000Du)&&(a&1u)){md_io_write8(a,v);return;}}
void md_write16(uint32_t a,uint16_t v){a&=0xFFFFFFu;if(a==0xC00000u){vdp_data16(v);return;}if(a==0xC00004u){vdp_ctrl16(v);return;}md_write8(a,(uint8_t)(v>>8));md_write8(a+1u,(uint8_t)v);}
void md_write32(uint32_t a,uint32_t v){md_write16(a,(uint16_t)(v>>16));md_write16(a+2u,(uint16_t)v);}
