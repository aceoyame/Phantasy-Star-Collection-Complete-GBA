#include "md_io_runtime.h"
#include "md_bus_runtime.h"
#define REG_KEYINPUT (*(volatile uint16_t*)0x04000130)
static uint8_t port1_data,port2_data,exp_data;
static uint8_t port1_ctrl,port2_ctrl,exp_ctrl;
static inline int down(uint16_t keys,unsigned bit){return (keys&(1u<<bit))==0;}
void md_io_reset(void){port1_data=port2_data=exp_data=0x40;port1_ctrl=port2_ctrl=exp_ctrl=0x40;}
void md_io_write8(uint32_t a,uint8_t v){
 a&=0xFFFFFFu;
 if(a==0xA10003u)port1_data=v;
 else if(a==0xA10005u)port2_data=v;
 else if(a==0xA10007u)exp_data=v;
 else if(a==0xA10009u)port1_ctrl=v;
 else if(a==0xA1000Bu)port2_ctrl=v;
 else if(a==0xA1000Du)exp_ctrl=v;
}
__attribute__((section(".iwram.text.port1_read"),noinline,aligned(16)))
static uint8_t port1_read(void){
 uint16_t k=REG_KEYINPUT; uint8_t r=0x7Fu; /* active-low 3-button MD data lines */
 if(port1_data&0x40u){
  if(down(k,6))r&=~0x01u; /* Up */
  if(down(k,7))r&=~0x02u; /* Down */
  if(down(k,5))r&=~0x04u; /* Left */
  if(down(k,4))r&=~0x08u; /* Right */
  if(down(k,1))r&=~0x10u; /* GBA B -> MD B */
  if(down(k,8)||down(k,9))r&=~0x20u; /* shoulder -> MD C */
  r|=0x40u;
 }else{
  if(down(k,6))r&=~0x01u;
  if(down(k,7))r&=~0x02u;
  r&=~0x0Cu; /* MD 3-button ID bits while TH low */
  if(down(k,0))r&=~0x10u; /* GBA A -> MD A */
  if(down(k,3))r&=~0x20u; /* GBA Start -> MD Start */
  r&=~0x40u;
 }
 return r;
}
uint8_t md_io_read8(uint32_t a){a&=0xFFFFFFu;if(a==0xA10003u)return port1_read();if(a==0xA10005u)return 0x7Fu;if(a==0xA10007u)return 0x7Fu;if(a==0xA10009u)return port1_ctrl;if(a==0xA1000Bu)return port2_ctrl;if(a==0xA1000Du)return exp_ctrl;return 0xFFu;}
/* Execute the hardware-facing portion of PSIV ReadJoypad $000898-$0008B8. */
static uint8_t swap_pad_bits(uint8_t v,uint8_t a,uint8_t b){
 uint8_t qa=(uint8_t)(v&a),qb=(uint8_t)(v&b);v=(uint8_t)(v&~(a|b));
 if(qa)v|=b;if(qb)v|=a;return v;
}
/* Cold canonical oracle retained for source-regression comparison; the live
 * IWRAM fast path below reads this fixed WRAM word directly.  This unreferenced
 * section is discarded by --gc-sections. */
static uint8_t ps4_pad_remap_canonical(uint8_t v){
 switch(md_read16(0xFFF43Eu)){
  case 2u:v=swap_pad_bits(v,0x40u,0x20u);break;
  case 4u:v=swap_pad_bits(v,0x10u,0x20u);break;
  case 6u:v=swap_pad_bits(v,0x10u,0x40u);v=swap_pad_bits(v,0x40u,0x20u);break;
  case 8u:v=swap_pad_bits(v,0x10u,0x40u);break;
  case 10u:v=swap_pad_bits(v,0x10u,0x40u);v=swap_pad_bits(v,0x10u,0x20u);break;
  default:break;
 }
 return v;
}
__attribute__((section(".iwram.text.md_io_read_psiv_pad"),noinline,aligned(16)))
uint8_t md_io_read_psiv_pad(void){
 /* This entry point is PSIV-specific, so do not re-enter the generic Genesis
  * address classifiers for the two fixed port-1 phases and fixed WRAM mapping
  * word on every GBA frame.  These assignments are exactly what md_io_write8
  * does for $A10003 and port1_read is exactly the matching read path. */
 static uint8_t* const ps4_work_ram=(uint8_t*)0x02000000;
 port1_data=0u; uint8_t d0=(uint8_t)((port1_read()<<2)&0xC0u);
 port1_data=0x40u; uint8_t d1=(uint8_t)(port1_read()&0x3Fu);
 uint8_t v=(uint8_t)~(d0|d1);
 /* PSIV ReadJoypad immediately remaps physical B/C/A through the selected
  * ButtonMappings entry.  Keep the semantic result here so every caller
  * (field, CAMP/SYSTEM and battle UI) observes the same configured buttons.
  * Index is the source word offset: 0,2,4,6,8,10. */
 switch(((uint16_t)ps4_work_ram[0xF43Eu]<<8)|ps4_work_ram[0xF43Fu]){
  case 2u:v=swap_pad_bits(v,0x40u,0x20u);break;                 /* A <-> C */
  case 4u:v=swap_pad_bits(v,0x10u,0x20u);break;                 /* B <-> C */
  case 6u:v=swap_pad_bits(v,0x10u,0x40u);v=swap_pad_bits(v,0x40u,0x20u);break;
  case 8u:v=swap_pad_bits(v,0x10u,0x40u);break;                 /* B <-> A */
  case 10u:v=swap_pad_bits(v,0x10u,0x40u);v=swap_pad_bits(v,0x10u,0x20u);break;
  default:break;
 }
 return v;
}
