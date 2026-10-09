#pragma once
#include <stdint.h>
typedef struct{uint32_t off;uint16_t frame_bytes;uint8_t geom,reserved;int8_t ox,oy;uint16_t pad;} Ps4SourceEnemyPart;
typedef struct{uint16_t first_part;uint8_t part_count,frame_count;uint32_t holds_off;uint32_t source_table;} Ps4SourceEnemyAnim;
typedef struct{uint16_t first_anim;uint8_t anim_count,kind,target_mode,primary_sfx,palette_id,enemy_id,ability_id;} Ps4SourceEnemyDesc;
typedef struct{uint8_t ability_id,desc_index;} Ps4SourceEnemyLookupEntry;
extern const uint8_t ps4_source_enemy_bank[];
extern const Ps4SourceEnemyPart ps4_source_enemy_parts[];
extern const Ps4SourceEnemyAnim ps4_source_enemy_anims[];
extern const Ps4SourceEnemyDesc ps4_source_enemy_descs[234];
#define PS4_SOURCE_ENEMY_PALETTE_COUNT 108u
extern const uint16_t ps4_source_enemy_palettes[PS4_SOURCE_ENEMY_PALETTE_COUNT][16] __attribute__((aligned(4)));
extern const uint16_t ps4_source_enemy_row_off[154];
extern const Ps4SourceEnemyLookupEntry ps4_source_enemy_lookup_entries[234];
extern const uint16_t ps4_source_enemy_sfx_off[235];
extern const uint8_t ps4_source_enemy_sfx_ids[];
const Ps4SourceEnemyDesc* ps4_source_enemy_find(uint8_t enemy_id,uint8_t ability_id);
int ps4_source_enemy_has_sfx(const Ps4SourceEnemyDesc* d,uint8_t sfx);
#define PS4_SOURCE_ENEMY_KIND_GENERATED 1u
#define PS4_SOURCE_ENEMY_KIND_NOOP 2u
#define PS4_SOURCE_ENEMY_KIND_BODY_DBLSLASH 3u
#define PS4_SOURCE_ENEMY_KIND_RIMIT_SCREEN 4u
#define PS4_SOURCE_ENEMY_KIND_EARTHQUAKE_SCREEN 5u
#define PS4_SOURCE_ENEMY_KIND_EXISTING 6u
#define PS4_SOURCE_ENEMY_TARGET_SELF 0u
#define PS4_SOURCE_ENEMY_TARGET_VICTIM 1u
#define PS4_SOURCE_ENEMY_TARGET_ALL_PARTY 2u
