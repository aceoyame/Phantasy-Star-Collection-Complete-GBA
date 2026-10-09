#pragma once
#include <stdint.h>
typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32; typedef int32_t s32; typedef int16_t s16; typedef uint64_t u64;
typedef struct { u32 d[8], a[8]; u32 usp, ssp; u32 pc; u16 sr; int stopped; u64 cycles; } M68K;
extern M68K C;
u8 m68k_read8(u32 a); u16 m68k_read16(u32 a); u32 m68k_read32(u32 a);
void m68k_write8(u32 a,u8 v); void m68k_write16(u32 a,u16 v); void m68k_write32(u32 a,u32 v);
void m68k_reset(void); void m68k_step(void); void m68k_interrupt(int level); void m68k_exception(int vec); void m68k_set_sr(u16 v);
