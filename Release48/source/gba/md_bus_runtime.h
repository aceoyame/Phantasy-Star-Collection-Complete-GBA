#ifndef MD_BUS_RUNTIME_H
#define MD_BUS_RUNTIME_H
#include <stdint.h>
void md_bus_reset(void);
uint8_t md_read8(uint32_t addr);
uint16_t md_read16(uint32_t addr);
uint32_t md_read32(uint32_t addr);
void md_write8(uint32_t addr,uint8_t value);
void md_write16(uint32_t addr,uint16_t value);
void md_write32(uint32_t addr,uint32_t value);
#endif
