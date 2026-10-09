static int r32_select_town(const uint8_t choices[16],unsigned n,uint32_t table){
 unsigned sel=0u,visible=n<5u?n:5u,first=0u;uint8_t prev=0u;ps4_ui_wait_buttons_up();
 for(;;){first=sel>=visible?sel-visible+1u:0u;if(first+visible>n)first=n-visible;
  ps4_ui_clear();ps4_ui_box(8u,6u,232u,154u);battle_draw_text(20u,14u,"TELEPORT",80u);
  for(unsigned row=0u;row<visible;row++){unsigned k=first+row,y=38u+row*20u;if(k==sel)pause_draw_cursor(18u,y,83u);ps4_ui_draw_rom_name(30u,y,0x2AB8A2u,rom8(table+choices[k]*8u),0xFEu,k==sel?83u:80u);}ps4_ui_present();
  uint8_t e=ps4_ui_wait_edge(&prev);if(e&1u)sel=sel?sel-1u:n-1u;else if(e&2u)sel=(sel+1u==n)?0u:sel+1u;else if(e&0x60u){ps4_ui_wait_buttons_up();return choices[sel];}else if(e&0x90u){ps4_ui_wait_buttons_up();return -1;}
 }
}
static int r32_use_teleport(unsigned town,int inventory_slot,uint16_t stats,unsigned cost){
 uint8_t choices[16];uint32_t table=0u;int selected=0;
 if(town){unsigned n=r32_town_destinations(choices,&table);if(!n){ps4_ui_message("CAN'T TELEPORT HERE",0);return 0;}selected=r32_select_town(choices,n,table);if(selected<0)return 0;}
 else if(!ram8r(0xED51u)){ps4_ui_message("CAN'T ESCAPE HERE",0);return 0;}
 if(stats&&ram16r((uint16_t)(stats+0x12u))<cost)return 0;
 if(!r32_teleport_destination(town,table,(unsigned)selected))return 0;
 if(inventory_slot>=0)ps4_inventory_remove((unsigned)inventory_slot);else if(stats)ram16w((uint16_t)(stats+0x12u),(uint16_t)(ram16r((uint16_t)(stats+0x12u))-cost));
 ram8w(0x500Au,0xBDu);field_event_reload_pending=1u;return 1;
}
