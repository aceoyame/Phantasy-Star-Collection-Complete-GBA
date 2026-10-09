/* The original world Plane B uses four 128x8 pages in $B000..$BFFF.
 * Retain mutations until their page is replaced, including Ice Digger paths. */
static uint8_t r32_world_bg_pages[4];
static void r32_world_bg_reset(void){for(unsigned i=0u;i<4u;i++)r32_world_bg_pages[i]=255u;}
__attribute__((noinline))
static uint8_t r32_world_bg_chunk(unsigned cx,unsigned cy){
 cx&=127u;cy&=127u;unsigned page=cy>>3,slot=page&3u;
 if(r32_world_bg_pages[slot]!=page){
  uint32_t table=field_map.map_id?0x1175C4u:0x10BE02u,src=rom32(table+page*4u);
  for(unsigned i=0u;i<1024u;i++)work_ram[0xB000u+slot*1024u+i]=rom8(src+i);
  r32_world_bg_pages[slot]=(uint8_t)page;
 }
 return work_ram[0xB000u+(cy&31u)*128u+cx];
}
