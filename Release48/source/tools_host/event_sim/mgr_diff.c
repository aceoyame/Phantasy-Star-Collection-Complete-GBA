#define _GNU_SOURCE
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "title_host.c"
static void map_gba(void){unsigned long R[][2]={{0x02000000,0x40000},{0x03000000,0x8000},{0x04000000,0x1000},{0x05000000,0x1000},{0x06000000,0x18000},{0x07000000,0x1000}};
  for(unsigned i=0;i<6;i++){void* p=mmap((void*)R[i][0],R[i][1],PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);if(p!=(void*)R[i][0])exit(1);}}
int main(void){ map_gba(); unsigned id,n;
  while(scanf("%u %u",&id,&n)==2){
    for(unsigned t=0;t<3;t++)for(unsigned f=0;f<256;f++)ps4_flag_clear_type(t,f);
    for(unsigned i=0;i<n;i++){unsigned t,f;scanf("%u %u",&t,&f);ps4_flag_set_type(t,f);}
    field_obj_count=32;for(unsigned s=0;s<32;s++){memset(&field_objs[s],0,sizeof field_objs[s]);field_objs[s].type=0x38;field_objs[s].active=1;}
    field_apply_map_manager_load(id);
    unsigned m=0;for(unsigned s=0;s<32;s++)if(!field_objs[s].active)m|=1u<<s;printf("%u %u",id,m);printf(" T");for(unsigned s=0;s<32;s++)if(field_objs[s].active&&field_objs[s].type!=0x38)printf("%u=%u,",s,field_objs[s].type);for(unsigned t=0;t<3;t++){printf(" ");for(unsigned f=0;f<256;f++)if(ps4_flag_test_type(t,f))printf("%u.%u,",t,f);}printf("\n");
  } return 0;}
