#ifndef PS4_PSC_MAP_DESC_H
#define PS4_PSC_MAP_DESC_H
#include <stdint.h>
typedef struct Ps4PscMapDesc {
 uint32_t kos_tiles,nem_tiles,kos_ram,chunks,map_updates,trans1,trans2,objects,treasure,layout_fg,layout_bg,data_manager,dialogue_tree,interactions,events,palette;
 uint8_t valid,general,music,is_world,row_fg,col_fg,row_bg,col_bg,collision_plane;
} Ps4PscMapDesc;
const Ps4PscMapDesc* ps4_psc_map_desc_get(uint16_t map_id);
#endif
