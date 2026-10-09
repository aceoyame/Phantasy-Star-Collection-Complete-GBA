#ifndef PS4_PSC_FIELD_PREDECODE_H
#define PS4_PSC_FIELD_PREDECODE_H
#include <stdint.h>
#define PS4_PREDECODE_KOS 0u
#define PS4_PREDECODE_NEM 1u
#define PS4_PREDECODE_PACKED 2
extern const uint8_t ps4_psc_field_predecode[];
/* 0 = absent, 1 = raw, 2 = LZ4 block; length is always expanded bytes. */
int ps4_psc_predecode_lookup(unsigned codec,uint32_t source,const uint8_t** data,unsigned* length);
#endif
