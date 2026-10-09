/* Independently compressed 4 KiB ROM pages; four direct-mapped cache slots. */
#include <stdint.h>
static const uint32_t *page_table;
static uint8_t page_cache[4][4096];
static uint32_t page_tags[4];
static void pager_init(const void *table){page_table=table;for(unsigned i=0;i<4;i++)page_tags[i]=0xffffffff;}
static uint8_t pager_read(uint32_t off){
 unsigned page=off>>12,slot=page&3;
 if(page_table[page]&1)return ((const uint8_t*)(page_table[page]&~1u))[off&4095];
 if(page_tags[slot]!=page){
  const uint8_t *s=(const void*)(page_table[page]&~1u);uint8_t*d=page_cache[slot];
  if(page_table[page]&1){for(unsigned i=0;i<4096;i++)d[i]=s[i];}
  else{unsigned n=0;while(n<4096){unsigned flags=*s++;for(unsigned bit=0;bit<8&&n<4096;bit++){
   if(flags&(1u<<bit)){unsigned v=s[0]|(s[1]<<8);s+=2;unsigned back=(v>>4)+1,len=(v&15)+3;while(len--){d[n]=d[n-back];n++;}}
   else d[n++]=*s++;
  }}}
  page_tags[slot]=page;
 }
 return page_cache[slot][off&4095];
}
