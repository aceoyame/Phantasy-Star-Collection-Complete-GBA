#ifndef PS4_PSC_FIELD_FASTASSETS_H
#define PS4_PSC_FIELD_FASTASSETS_H
#include <stdint.h>
#define PS4_FASTGFX_KOS 0u
#define PS4_FASTGFX_NEM 1u
extern const uint8_t ps4_psc_field_fastassets[];
extern const uint32_t ps4_psc_pose_bank_offset;
int ps4_psc_fastgfx_lookup(unsigned codec,uint32_t source,const uint8_t** data,unsigned* length);
const uint8_t* ps4_psc_party_pose(uint32_t art,uint8_t facing,unsigned anim);
#endif
