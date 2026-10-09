#define _GNU_SOURCE
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "title_host.c"
static void map_gba(void){unsigned long R[][2]={{0x02000000,0x40000},{0x03000000,0x8000},{0x04000000,0x1000},{0x05000000,0x1000},{0x06000000,0x18000},{0x07000000,0x1000}};
  for(unsigned i=0;i<6;i++){void* p=mmap((void*)R[i][0],R[i][1],PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);if(p!=(void*)R[i][0])exit(1);}}
int main(int argc,char** argv){ map_gba();
  unsigned map=strtoul(argv[1],0,16);
  for(unsigned t=0;t<3;t++)for(unsigned f=0;f<256;f++)ps4_flag_clear_type(t,f);
  for(int i=2;i<argc;i++){unsigned f=strtoul(argv[i],0,16);ps4_event_flag_set(f);}
  int r=ps4_load_field_map((uint16_t)map);
  printf("map %02X load rc=%d objs=%u\n",map,r,field_obj_count);
  for(unsigned i=0;i<field_obj_count;i++){struct FieldObjLite*o=&field_objs[i];
    printf("  slot %2u type %03X art %03X dlg %02X pos (%3u,%3u) active %u\n",i,o->type,o->base_tile,o->dialog,o->x/8,o->y/8,o->active);}
  return 0;}
