#pragma once
#include <stdint.h>
typedef struct{uint32_t off;uint16_t frame_bytes;uint8_t geom,reserved;int8_t ox,oy;uint16_t pad;} Ps4SourceAbilityPart;
typedef struct{uint16_t first_part;uint8_t part_count,frame_count;uint32_t holds_off;uint32_t source_table;} Ps4SourceAbilityAnim;
typedef struct{uint16_t first_anim;uint8_t anim_count,kind,sfx,palette_id;} Ps4SourceAbilityDesc;
extern const uint8_t ps4_source_ability_bank[];
extern const Ps4SourceAbilityPart ps4_source_ability_parts[];
extern const Ps4SourceAbilityAnim ps4_source_ability_anims[];
#define PS4_SOURCE_ABILITY_ANIM_COUNT 132u
#define PS4_SOURCE_ABILITY_PALETTE_COUNT 50u
extern const uint16_t ps4_source_ability_palettes[PS4_SOURCE_ABILITY_PALETTE_COUNT][16] __attribute__((aligned(4)));
extern const Ps4SourceAbilityDesc ps4_source_tech[39];
extern const Ps4SourceAbilityDesc ps4_source_skill[55];
