/* Generated from ps4.asm; R30 palette data remains in ROM. */
static const uint16_t r30_PlateSysF1CyclingPal[]={0x0860u,0x0A80u,0x0CA0u,0x0EC0u,0x0CA0u,0x0A80u,0x00CEu,0x00ACu,0x008Au,0x0068u,0x008Au,0x00ACu};
static const uint16_t r30_PlateSysF1CyclingPal2[]={0x0420u,0x0642u,0x0864u,0x0A86u,0x0864u,0x0642u,0x068Au,0x0468u,0x0246u,0x0024u,0x0246u,0x0468u};
static const uint16_t r30_NurvusB1CyclingPal[]={0x0024u,0x0046u,0x0068u,0x028Au,0x04ACu,0x028Au,0x0068u,0x0046u,0x0AC0u,0x08A0u,0x0680u,0x0460u,0x0240u,0x0460u,0x0680u,0x08A0u};
static const uint16_t r30_NurvusB1CyclingPal2[]={0x0068u,0x028Au,0x04ACu,0x06CEu,0x04ACu,0x028Au,0x0CE0u,0x0AC0u,0x08A0u,0x0680u,0x08A0u,0x0AC0u};
static const uint16_t r30_WreckageEngineCyclingPal[]={0x0CE6u,0x0AC4u,0x08A2u,0x0680u,0x08A2u,0x0AC4u,0x0860u,0x0A80u,0x0CA0u,0x0EC0u,0x0CA0u,0x0A80u};
static const uint16_t r30_PlateSysCyclingPal[]={0x0068u,0x028Au,0x04ACu,0x06CEu,0x04ACu,0x028Au,0x0CE0u,0x0AC0u,0x08A0u,0x0680u,0x08A0u,0x0AC0u};
static const uint16_t r30_NurvusCyclingPal[]={0x0024u,0x0046u,0x0068u,0x028Au,0x04ACu,0x028Au,0x0068u,0x0046u,0x0AC0u,0x08A0u,0x0680u,0x0460u,0x0240u,0x0460u,0x0680u,0x08A0u};
static const uint16_t r30_NurvusCyclingPal2[]={0x0068u,0x028Au,0x04ACu,0x06CEu,0x04ACu,0x028Au,0x0CE0u,0x0AC0u,0x08A0u,0x0680u,0x08A0u,0x0AC0u};
static const uint16_t r30_VahFortCyclingPal[]={0x0068u,0x028Au,0x04ACu,0x06CEu,0x04ACu,0x028Au,0x0CE0u,0x0AC0u,0x08A0u,0x0680u,0x08A0u,0x0AC0u};
static const uint16_t r30_VahFortCyclingPal2[]={0x0024u,0x0046u,0x0068u,0x028Au,0x04ACu,0x028Au,0x0068u,0x0046u,0x0AC0u,0x08A0u,0x0680u,0x0460u,0x0240u,0x0460u,0x0680u,0x08A0u};
static const uint16_t r30_GrbrkTwCyclingPal[]={0x08CAu,0x0486u,0x06A8u,0x08CAu,0x06A8u,0x08CAu,0x0486u,0x06A8u,0x0486u,0x06A8u,0x08CAu,0x0486u};
static const uint16_t r30_GrbrkTwCyclingPal2[]={0x0E80u,0x0420u,0x0640u,0x0A60u,0x0A60u,0x0E80u,0x0420u,0x0640u,0x0640u,0x0A60u,0x0E80u,0x0420u,0x0420u,0x0640u,0x0A60u,0x0E80u};
static const uint16_t r30_RykrosCrystalsCyclingPal[]={0x0CECu,0x0EECu,0x0EEEu,0x0EECu};
static const uint16_t r30_LeRoofRoomCyclingPal[]={0x000Eu,0x000Cu,0x000Au,0x0008u,0x0006u,0x0004u,0x0002u,0x0000u,0x0002u,0x0004u,0x0006u,0x0008u,0x000Au,0x000Cu,0x000Eu};
__attribute__((noinline)) static void r30_palette_pair(const uint16_t* p,unsigned dst,uint16_t state,unsigned n){if(native_main_frame_count&7u)return;unsigned q=ram8r(state);if(q>=n)q=0u;field_palette_live_word(dst,p[q]);field_palette_live_word(dst+1u,p[q+6u]);ram8w(state,(uint8_t)(q+1u));}
__attribute__((noinline)) static void r30_palette_update(unsigned id){unsigned powered=ps4_event_flag_test(0xDAu)||(!ps4_event_flag_test(0x65u)&&!ps4_event_flag_test(0x61u));switch(id){
case 0x17u:if(powered)r30_palette_pair(r30_PlateSysF1CyclingPal,28u,0xECF2u,6u);else{field_palette_live_word(28u,0x860u);field_palette_live_word(29u,0x68u);}break;
case 0x18u:if(powered)r30_palette_pair(r30_PlateSysF1CyclingPal2,12u,0xECF3u,6u);break;
case 0x1Cu:if(!ps4_event_flag_test(0x66u))r30_palette_pair(r30_NurvusB1CyclingPal,12u,0xECF3u,8u);break;
case 0x1Du:if(!ps4_event_flag_test(0x66u))r30_palette_pair(r30_NurvusB1CyclingPal2,28u,0xECF2u,6u);break;
case 0x20u:if(ps4_event_flag_test(0x40u))r30_palette_pair(r30_WreckageEngineCyclingPal,28u,0xECF2u,6u);break;
case 0x27u:if(powered)r30_palette_pair(r30_PlateSysCyclingPal,12u,0xECF2u,6u);break;
case 0x28u:if(!ps4_event_flag_test(0x65u))r30_palette_pair(r30_NurvusCyclingPal,28u,0xECF3u,8u);break;
case 0x29u:if(!ps4_event_flag_test(0x65u))r30_palette_pair(r30_NurvusCyclingPal2,12u,0xECF2u,6u);break;
case 0x35u:if(!ps4_event_flag_test(0xBBu)&&ps4_event_flag_test(0xAFu))r30_palette_pair(r30_VahFortCyclingPal,12u,0xECF2u,6u);break;
case 0x36u:if(!ps4_event_flag_test(0xBBu)&&ps4_event_flag_test(0xAFu))r30_palette_pair(r30_VahFortCyclingPal2,28u,0xECF3u,8u);break;
case 0x23u:if(!(native_main_frame_count&3u)){unsigned q=(native_main_frame_count>>2)&3u;for(unsigned n=0u;n<3u;n++)field_palette_live_word(10u+n,r30_GrbrkTwCyclingPal[q+n*4u]);}break;
case 0x24u:if(!(native_main_frame_count&7u)){unsigned q=(native_main_frame_count>>3)&3u;for(unsigned n=0u;n<3u;n++)field_palette_live_word(17u+n,r30_GrbrkTwCyclingPal2[q+n*4u]);field_palette_live_word(20u,r30_GrbrkTwCyclingPal2[q+10u]);}break;
case 0x2Au:{uint8_t d=(uint8_t)(ram8r(0xECF2u)-1u);ram8w(0xECF2u,d);if(d){field_palette_live_word(2u,0xCECu);field_palette_live_word(18u,0xCECu);}else if(!(native_main_frame_count&3u)&&ram8r(0xECF3u)==4u){ram8w(0xECF2u,(uint8_t)ps4_rng2_next());ram8w(0xECF3u,0u);}else{if(!(native_main_frame_count&3u)){unsigned q=ram8r(0xECF3u);if(q>3u)q=0u;field_palette_live_word(2u,r30_RykrosCrystalsCyclingPal[q]);field_palette_live_word(18u,r30_RykrosCrystalsCyclingPal[q]);ram8w(0xECF3u,(uint8_t)(q+1u));}ram8w(0xECF2u,1u);}break;}
case 0x2Cu:if(!(native_main_frame_count&7u)){unsigned q=ram8r(0xECF2u);if(q>=14u)q=0u;field_palette_live_word(13u,r30_LeRoofRoomCyclingPal[q]);ram8w(0xECF2u,(uint8_t)(q+1u));}break;
}}
