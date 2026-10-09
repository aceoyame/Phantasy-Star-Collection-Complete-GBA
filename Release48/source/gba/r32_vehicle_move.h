/* FieldObj_VehicleMove and UpdateVehicleCollision source lowering. */
static uint8_t r32_vehicle_collision(uint8_t d){
 int x0,y0,x1,y1;
 if(d==1u){x0=0;x1=16;y0=-48;y1=-32;}
 else if(d==2u){x0=0;x1=16;y0=16;y1=32;}
 else if(d>=4u&&d<=6u){x0=-32;x1=-16;y0=-16;y1=0;}
 else if(d>=8u&&d<=10u){x0=32;x1=48;y0=-16;y1=0;}
 else return 15u;
 uint8_t q=field_collision_at(chaz.x,chaz.y,x0,y0),v;
 v=field_collision_at(chaz.x,chaz.y,x1,y0);if(v>q)q=v;
 v=field_collision_at(chaz.x,chaz.y,x0,y1);if(v>q)q=v;
 v=field_collision_at(chaz.x,chaz.y,x1,y1);if(v>q)q=v;return q;
}
static void r32_world_bg_write(int x,int y,uint8_t value){
 unsigned cx=(unsigned)field_wrap_signed(x,4096)>>5,cy=(unsigned)field_wrap_signed(y,4096)>>5;
 (void)r32_world_bg_chunk(cx,cy);work_ram[0xB000u+(cy&31u)*128u+cx]=value;
 field_dirty_chunk_mark(1u,cx,cy);
}
static void r32_vehicle_break_ice(uint8_t d){
 ram8w(0x500Au,0xEEu);
 int x=chaz.x+(int8_t)rom8(0x46136u+d*2u),y=chaz.y+(int8_t)rom8(0x46137u+d*2u);
 static const uint32_t vectors[3]={0x4614Cu,0x46178u,0x461A4u};
 for(unsigned k=0u;k<3u;k++){
  int i=r31_actor_spawn(-1,0x150u,(uint16_t)x,(uint16_t)y,(uint8_t)(k?4u:0u),0x39Bu);if(i<0)continue;
  uint16_t b=(uint16_t)(0xC300u+(unsigned)i*64u);uint32_t p=vectors[k]+d*4u;
  ram32w((uint16_t)(b+0x20u),(uint32_t)((int32_t)(int16_t)rom16(p)*256));
  ram32w((uint16_t)(b+0x24u),(uint32_t)((int32_t)(int16_t)rom16(p+2u)*256));
  ram8w((uint16_t)(b+0x11u),(uint8_t)(k?6u:8u));ram16w((uint16_t)(b+0x1Cu),(uint16_t)(k?30u:40u));
 }
 /* Source loc_45CDC offsets are doubled before GetMapLayoutChunkBG. */
 int dx=(int8_t)rom8(0x45CDCu+d*2u),dy=(int8_t)rom8(0x45CDDu+d*2u);
 r32_world_bg_write(chaz.x+dx*2,chaz.y+dy*2,rom8(0x4612Au+d));
}
static void r32_vehicle_finish_ice(void){
 if(ram16r(0xF43Cu)!=2u||field_map.map_id!=1u)return;
 unsigned cx=(unsigned)field_wrap_signed(chaz.x,4096)>>5,cy=(unsigned)field_wrap_signed(chaz.y+16,4096)>>5;
 uint8_t q=r32_world_bg_chunk(cx,cy);if(q>=0x4Cu&&q<=0x51u&&q!=0x4Du)r32_world_bg_write(chaz.x,chaz.y+16,0x4Du);
}
__attribute__((noinline))
static void r32_vehicle_begin(uint8_t pad){
 int dx,dy;uint8_t face=chaz.facing,d=pad&15u;if(!field_direction_vector(d,&dx,&dy,&face))return;
 chaz.facing=face;uint8_t q=r32_vehicle_collision(d);unsigned v=ram16r(0xF43Cu);
 unsigned clear=q==0u||q==1u||q==10u||(v==3u&&q==9u);
 if(v==2u&&q==11u&&field_map.map_id==1u){r32_vehicle_break_ice(d);clear=1u;}
 if(!clear){chaz.anim=chaz.anim_clock=0u;field_sync_native_party_to_source();return;}
 chaz.dx=(int8_t)dx;chaz.dy=(int8_t)dy;chaz.step_left=32u;field_sync_native_party_to_source();
}
static unsigned r32_vehicle_speed(void){
 unsigned v=ram16r(0xF43Cu),d=chaz.facing==0u?2u:chaz.facing==1u?1u:chaz.facing==2u?8u:4u;
 uint32_t p=0x47C28u+(v==3u?128u:0u)+((unsigned)(ram8r(0xECE0u)&3u)<<7)+d*8u;
 int speed=(int16_t)rom16(p+(chaz.dx?2u:4u));if(speed<0)speed=-speed;return (unsigned)speed>>8;
}
