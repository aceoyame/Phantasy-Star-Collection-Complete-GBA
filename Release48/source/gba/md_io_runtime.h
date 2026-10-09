#ifndef MD_IO_RUNTIME_H
#define MD_IO_RUNTIME_H
#include <stdint.h>
void md_io_reset(void);
void md_io_write8(uint32_t addr,uint8_t value);
uint8_t md_io_read8(uint32_t addr);
uint8_t md_io_read_psiv_pad(void);
#endif
