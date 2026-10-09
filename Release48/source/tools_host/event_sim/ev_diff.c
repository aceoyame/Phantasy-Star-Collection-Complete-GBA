#define _GNU_SOURCE
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "title_host_ev.c"
static void map_gba(void){unsigned long R[][2]={{0x02000000,0x40000},{0x03000000,0x8000},{0x04000000,0x1000},{0x05000000,0x1000},{0x06000000,0x18000},{0x07000000,0x1000}};
  for(unsigned i=0;i<6;i++){void* p=mmap((void*)R[i][0],R[i][1],PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);if(p!=(void*)R[i][0])exit(1);}}
int main(void){ map_gba(); unsigned id,n;
  while(scanf("%u %u",&id,&n)==2){
    for(unsigned t=0;t<4;t++)for(unsigned f=0;f<256;f++)ps4_flag_clear_type(t,f);
    for(unsigned i=0;i<n;i++){unsigned t,f;scanf("%u %u",&t,&f);ps4_flag_set_type(t,f);}
    unsigned cx=0,cy=0;host_rec_event=0;int r=field_run_map_event_condition((uint8_t)id,&cx,&cy);
    printf("%u %u %d\n",id,host_rec_event,r);fflush(stdout);
  } return 0;}
