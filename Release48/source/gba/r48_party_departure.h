/* A scripted departure must not turn a surviving party into a game over.
 * Battle defeat is unaffected: call only after removing/replacing a living
 * character. Chaz (otherwise the first remaining member) receives full HP;
 * the other members, TP, inventory and departed record remain unchanged.
 */
static unsigned r48_char_living(uint8_t id){
    if(id>=11u)return 0u;
    uint16_t d=(uint16_t)(PS4_CHAR_BASE+(unsigned)id*128u);
    return ram16r((uint16_t)(d+0x0Eu))!=0u && !(ram8r((uint16_t)(d+0x16u))&0x44u);
}
static void r48_scripted_departure_guard(unsigned removed_living){
    if(!removed_living)return;
    uint8_t first=0xFFu;
    for(unsigned slot=0u;slot<5u;slot++){
        uint8_t id=ram8r((uint16_t)(0xF40Au+slot));if(id==0xFFu)break;
        if(id>=11u)continue;
        if(r48_char_living(id))return;
        if(first==0xFFu || id==0u)first=id;
    }
    if(first==0xFFu)return;
    uint16_t d=(uint16_t)(PS4_CHAR_BASE+(unsigned)first*128u);
    uint16_t hp=ram16r((uint16_t)(d+0x10u));
    ram16w((uint16_t)(d+0x0Eu),hp?hp:1u);ram8w((uint16_t)(d+0x16u),0u);
}
