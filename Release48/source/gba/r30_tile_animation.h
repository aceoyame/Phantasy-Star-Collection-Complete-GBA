/* All six retail TileAnimJmpTbl controllers. Zero is the source no-op. */
__attribute__((noinline))
static void r30_tile_animation_load(uint32_t treasure){
    uint32_t p=skip_word_records(treasure,6u);
    for(unsigned i=0u;i<16u;i++){uint16_t b=(uint16_t)(0xE800u+i*32u);for(unsigned j=0u;j<32u;j++)ram8w((uint16_t)(b+j),0u);}
    for(unsigned i=0u;i<16u&&rom16(p)!=0xFFFFu;i++,p+=2u){uint16_t b=(uint16_t)(0xE800u+i*32u);ram16w(b,rom8(p));ram16w((uint16_t)(b+4u),rom8(p+1u));}
}
__attribute__((noinline))
static void r30_tile_animation_update(void){
    static const uint16_t tiles[6]={0x28Du,0x203u,0x263u,0x246u,0x256u,0x266u};
    static const uint16_t bases[6]={0u,0u,0xC00u,0u,0x800u,0x1000u};
    static const uint8_t sizes[6]={16u,32u,16u,16u,16u,32u};
    static const uint8_t lists[6]={0u,1u,2u,3u,0u,4u};
    for(unsigned i=0u;i<16u;i++){
        uint16_t b=(uint16_t)(0xE800u+i*32u);unsigned id=ram16r(b)&0x7FFFu;if(id<4u||id>24u||(id&3u))continue;unsigned n=(id>>2)-1u;
        uint32_t seq=rom32(0x5175Eu+(unsigned)lists[n]*4u);uint8_t dur=ram8r((uint16_t)(b+15u)),frame=ram8r((uint16_t)(b+14u));unsigned changed=!(ram16r(b)&0x8000u);
        ram16w(b,(uint16_t)(id|0x8000u));dur=(uint8_t)(dur-1u);
        if(dur&0x80u){dur=rom8(seq+1u);frame=(uint8_t)(frame+1u);changed=1u;}
        if(frame>=rom8(seq)){frame=0u;changed=1u;}
        ram8w((uint16_t)(b+15u),dur);ram8w((uint16_t)(b+14u),frame);
        if(!changed)continue;
        unsigned src=(unsigned)bases[n]+(unsigned)rom8(seq+2u+frame)*32u,dst=(unsigned)tiles[n]*32u,bytes=(unsigned)sizes[n]*32u;
        if(src+bytes>0x10000u||dst+bytes>0x10000u)continue;
        for(unsigned k=0u;k<bytes;k++)md_vram[dst+k]=work_ram[src+k];
        for(unsigned k=0u;k<sizes[n];k++)field_gba_ready_tile[tiles[n]+k]=0xFFFFu;
        hw_publish_canonical_tiles(tiles[n],sizes[n]);
    }
}
