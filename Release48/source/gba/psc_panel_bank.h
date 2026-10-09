#ifndef PS4_PSC_PANEL_BANK_H
#define PS4_PSC_PANEL_BANK_H
#include <stdint.h>
typedef struct Ps4PscPanelDesc {
 uint32_t stream_offset; uint16_t stream_length,palette_word_offset; uint8_t palette_mask,valid;
} Ps4PscPanelDesc;
extern const uint8_t ps4_psc_panel_bank[];
extern const uint32_t ps4_psc_panel_palette_byte_offset;
const Ps4PscPanelDesc* ps4_psc_panel_get(uint16_t panel_id);
#endif
