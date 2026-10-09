#pragma once
#include <stdint.h>
typedef struct{uint32_t off;uint16_t frame_bytes;uint8_t geom,reserved;int8_t ox,oy;uint16_t pad;} Ps4SourceComboPart;
typedef struct{uint16_t first_part;uint8_t part_count,frame_count;uint32_t holds_off;uint32_t source_table;} Ps4SourceComboAnim;
typedef struct{uint16_t first_anim;uint8_t anim_count,kind,palette_id;} Ps4SourceComboDesc;
extern const uint8_t ps4_source_combo_bank[];
extern const Ps4SourceComboPart ps4_source_combo_parts[];
extern const Ps4SourceComboAnim ps4_source_combo_anims[];
extern const uint16_t ps4_source_combo_palettes[15][16] __attribute__((aligned(4)));
extern const Ps4SourceComboDesc ps4_source_combo[15];
#define PS4_SOURCE_COMBO_ANIM_COUNT 33u
