#include <stdio.h>
#include <stdint.h>
#include "ps4_audio_runtime.h"
volatile uint16_t hw_reg16[0x200]; volatile uint32_t hw_reg32[0x100];
int main(void){ ps4_audio_init(); ps4_audio_play(0x87);
  int bad=0; for(unsigned v=0; v<0x81u; v++){ ps4_audio_sound_index((uint8_t)v); if(ps4_audio_current_music()!=0x87){printf("value %02X stopped music\n",v);bad=1;break;} }
  ps4_audio_sound_index(0xFE); int stopped=ps4_audio_current_music()==0;
  printf("%s: $00-$80 ignored=%d, $FE StopAll still stops=%d\n",(!bad&&stopped)?"PASS":"FAIL",!bad,stopped); return bad||!stopped; }
