/* Source TownFlags and eight-byte teleport records. */
static void r32_visit_place(uint16_t map,uint16_t previous,uint16_t x){
 for(uint32_t p=0x664BCu;rom16(p)!=0xFFFFu;p+=6u){
  if(rom16(p)!=map||rom16(p+2u)!=previous)continue;
  unsigned f=rom8(p+5u);if(f<128u){uint16_t a=(uint16_t)(0xF160u+(f>>3));ram8w(a,(uint8_t)(ram8r(a)|(128u>>(f&7u))));}
  ram8w(0xED51u,0u);if(f!=254u&&f>=128u){unsigned d=f&127u;if(d==7u&&x==0x520u)d=8u;ram8w(0xED51u,(uint8_t)d);}return;
 }
}
static unsigned r32_town_destinations(uint8_t out[16],uint32_t* table){
 unsigned world=ram8r(0xF400u);if(ram8r(0xED51u)||ram8r(0xED50u)||world>=3u)return 0u;
 unsigned bits,count;if(world==0u){bits=ram16r(0xF160u);count=16u;*table=0x6115Au;}
 else if(world==1u){bits=ram16r(0xF162u)&0xFF80u;count=9u;*table=0x611C2u;}
 else{bits=(unsigned)ram8r(0xF163u)<<9;count=1u;*table=0x6120Au;}
 unsigned n=0u;for(unsigned i=0u;i<count;i++){if(bits&0x8000u)out[n++]=(uint8_t)i;bits=(bits<<1)&65535u;}return n;
}
static int r32_teleport_destination(unsigned town,uint32_t table,unsigned selected){
 uint32_t p;if(town){uint8_t available[16];uint32_t expected=0u;unsigned n=r32_town_destinations(available,&expected),found=0u;
  for(unsigned i=0u;i<n;i++)if(available[i]==selected)found=1u;if(!found||table!=expected)return 0;
  p=table+selected*8u;ram16w(0xEC2Au,rom16(p+2u));ram16w(0xEC28u,ram8r(0xF400u));ram16w(0xEC48u,rom16(p+4u));ram16w(0xEC4Au,rom16(p+6u));ram16w(0xEC44u,0u);ram16w(0xEC46u,0u);
 }else{unsigned index=ram8r(0xED51u);if(!index)return 0;p=0x61212u+index*8u;
  ram16w(0xEC2Au,ram16r(0xEC28u));ram16w(0xEC28u,rom16(p));ram16w(0xEC48u,rom16(p+2u));ram16w(0xEC4Au,rom16(p+4u));ram16w(0xEC44u,rom8(p+6u));ram16w(0xEC46u,rom8(p+7u));
 }
 ram8w(0xEC4Eu,(uint8_t)(ram8r(0xEC4Eu)&~8u));return 1;
}
