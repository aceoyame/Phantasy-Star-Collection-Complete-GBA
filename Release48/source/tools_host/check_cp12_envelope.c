/* Compare CP12 bulk advancement with the unchanged CP11 tick oracle. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define MD_HOST
#include "../additional/gsnd/gfm_tabs.h"
#define eop ref_eop
#define EG_ATK REF_ATK
#define EG_DEC REF_DEC
#define EG_SUS REF_SUS
#define EG_REL REF_REL
#define eg_eff_rate ref_eff_rate
#define eg_clock ref_clock
#define eg_next ref_next
#define eg_advance ref_advance
#define eg_key_on ref_key_on
#define eg_key_off ref_key_off
#include "../references/cp11_speed/gsnd/gsnd_eg.h"
#undef eop
#undef EG_ATK
#undef EG_DEC
#undef EG_SUS
#undef EG_REL
#undef eg_eff_rate
#undef eg_clock
#undef eg_next
#undef eg_advance
#undef eg_key_on
#undef eg_key_off
#include "../additional/gsnd/gsnd_eg.h"
static uint32_t seed=0x12345678;
static uint32_t rnd(void){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
int main(void){
  _Static_assert(sizeof(eop)==sizeof(ref_eop),"envelope layout");
  for(uint32_t i=0;i<300000;i++){
    ref_eop a={0};eop b;
    uint32_t now=rnd()&0x3fffffffu;
    a.env=rnd()&1023u;a.tl8=rnd()&1016u;a.sl=(rnd()&31u)<<5;
    a.state=rnd()&3u;a.key=rnd()&1u;
    for(int j=0;j<4;j++) a.rate[j]=rnd()&63u;
    a.next_ev=ref_next(&a,now);memcpy(&b,&a,sizeof b);
    for(int j=0;j<8;j++){
      now+=rnd()&2047u;
      ref_advance(&a,now);eg_advance(&b,now);
      if(memcmp(&a,&b,sizeof b)){
        printf("FAIL case %u span %d tick %u state %u/%u env %u/%u next %u/%u\n",i,j,now,a.state,b.state,a.env,b.env,a.next_ev,b.next_ev);return 1;
      }
      if(j==2){ref_key_on(&a,now);eg_key_on(&b,now);}
      if(j==5){ref_key_off(&a,now);eg_key_off(&b,now);}
    }
  }
  puts("2,400,000 envelope spans exactly match CP11, including all rates and key transitions");
}
