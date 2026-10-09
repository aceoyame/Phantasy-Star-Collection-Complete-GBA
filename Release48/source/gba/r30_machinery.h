__attribute__((noinline))
static void field_update_layout_byte(uint16_t addr,uint8_t value);
__attribute__((noinline))
static void r30_conveyor_rows(uint32_t addresses,unsigned count,uint32_t frames){
    uint8_t value=rom8(frames+(native_main_frame_count&7u));
    for(unsigned i=0u;i<count;i++)field_update_layout_byte(rom16(addresses+i*2u),value);
}
__attribute__((noinline))
static void r30_conveyor_update(unsigned id){
    ram8w(0xECF6u,(uint8_t)(ram8r(0xEC24u)+1u));
    if(id==0x11u){unsigned a=ps4_flag_test_type(2u,0x0Bu),b=ps4_flag_test_type(2u,0x0Cu);
        r30_conveyor_rows(0x5503Cu,5u,0x55074u);r30_conveyor_rows(0x55046u,5u,0x5507Cu);
        r30_conveyor_rows(0x55050u,5u,a?0x5507Cu:0x55074u);r30_conveyor_rows(0x5505Au,4u,a?0x55084u:0x5508Cu);
        r30_conveyor_rows(0x55062u,5u,b?0x5507Cu:0x55074u);r30_conveyor_rows(0x5506Cu,4u,b?0x5508Cu:0x55084u);
    }else{unsigned a=ps4_flag_test_type(2u,0x11u),b=ps4_flag_test_type(2u,0x12u);
        r30_conveyor_rows(0x551C4u,6u,a?0x55204u:0x551FCu);r30_conveyor_rows(0x551D0u,4u,a?0x55214u:0x5520Cu);r30_conveyor_rows(0x551D8u,4u,a?0x55214u:0x5520Cu);
        r30_conveyor_rows(0x551E0u,6u,b?0x55204u:0x551FCu);r30_conveyor_rows(0x551ECu,4u,b?0x5520Cu:0x55214u);r30_conveyor_rows(0x551F4u,4u,b?0x5520Cu:0x55214u);
    }
}
