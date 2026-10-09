#define _GNU_SOURCE
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "title_host.c"
static void map_gba(void){unsigned long R[][2]={{0x02000000,0x40000},{0x03000000,0x8000},{0x04000000,0x1000},{0x05000000,0x1000},{0x06000000,0x18000},{0x07000000,0x1000}};
  for(unsigned i=0;i<6;i++){void* p=mmap((void*)R[i][0],R[i][1],PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);if(p!=(void*)R[i][0])exit(1);}}
int main(int argc,char** argv){ map_gba();
  for(unsigned t=0;t<3;t++)for(unsigned f=0;f<256;f++)ps4_flag_clear_type(t,f);
  const char* fl=getenv("FLAGS"); if(fl){char b[256];strncpy(b,fl,255);for(char* t=strtok(b," ");t;t=strtok(0," "))ps4_event_flag_set(strtoul(t,0,16));}
  unsigned map=strtoul(argv[1],0,16); ps4_load_field_map((uint16_t)map); hw_load_palettes_full(); field_obj_art_epoch_advance(); hw_prepare_static_obj_art();
  FILE* f=fopen(argv[2],"wb"); const uint16_t* pal=(const uint16_t*)0x05000200;
  for(unsigned i=0;i<field_obj_count;i++){struct FieldObjLite*o=&field_objs[i];if(!o->active||!o->render_w)continue;
    unsigned w=o->render_w,h=o->render_h,tw=w/8;const uint8_t* vr=(const uint8_t*)0x06010000;
    fprintf(f,"OBJ %u %03X %u %u\n",i,o->type,w,h);
    for(unsigned y=0;y<h;y++){for(unsigned x=0;x<w;x++){unsigned t=o->gba_tile+(y/8)*tw+(x/8);uint8_t b=vr[t*32+(y&7)*4+(x&7)/2];unsigned ci=(x&1)?(b>>4):(b&15);uint16_t c=ci?pal[3*16+ci]:0x7C1F;fprintf(f,"%u,",c);}fprintf(f,"\n");}}
  fclose(f);return 0;}
