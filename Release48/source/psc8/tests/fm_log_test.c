/* host: replay a captured PSI FM write log (audits/fmcmp/fmlog.txt: frame, sample-within-frame, port, value) through the
 * block renderer exactly as the runtime does (catch-up render before each write) and compare every sample and the final state
 * with n x fastopll_calc().  Prints which render paths the log exercised.
 *   gcc -O2 -DFM_STEADY_C -DFM_STATS -I. -o /tmp/flt tests/fm_log_test.c fast_opll.c fast_opll_block.c && /tmp/flt audits/fmcmp/fmlog.txt */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fast_opll.h"
#define N 304
unsigned long fm_stat[8];
int main(int argc,char**argv){
  FILE*f=fopen(argv[1],"r"); static int ev[9000][4]; int cnt=0;
  while(fscanf(f,"%d %d %d %d",&ev[cnt][0],&ev[cnt][1],&ev[cnt][2],&ev[cnt][3])==4) cnt++;
  fclose(f);
  static FastOPLL a,b; fastopll_reset(&a); fastopll_reset(&b);
  long tot=0,bad=0; int first=ev[0][0], last=ev[cnt-1][0]; int k=0;
  for(int fr=first; fr<=last; fr++){
    int done=0;
    while(1){
      int due=(k<cnt&&ev[k][0]==fr)?ev[k][1]:N; if(due>N) due=N;
      if(due>done){ static int16_t acc[N+8]; unsigned n=(unsigned)(due-done); fastopll_render(&b,acc,n);
        for(unsigned i=0;i<n;i++){ int y=acc[i]>>3; y=y>127?127:y<-128?-128:y; int x=fastopll_calc(&a); tot++; if(x!=y){ if(bad<5) printf("mismatch frame %d sample %u: %d vs %d\n",fr,i+done,x,y); bad++; } }
        if(memcmp(&a,&b,FASTOPLL_STATE_BYTES)){ if(bad<5) printf("state differs frame %d\n",fr); bad++; }
        done=due; }
      if(k<cnt&&ev[k][0]==fr){ int port=ev[k][2],v=ev[k][3];
        if(port==0){ fastopll_addr(&a,v); fastopll_addr(&b,v);} else if(port==1){ fastopll_data(&a,v); fastopll_data(&b,v);} k++; }
      else break;
    }
  }
  printf("samples %ld mismatches %ld | steady %lu calls %lu samples, ramp %lu calls %lu samples, single %lu samples, %lu channel-blocks\n",tot,bad,fm_stat[0],fm_stat[1],fm_stat[2],fm_stat[3],fm_stat[4],fm_stat[5]);
  return bad!=0;
}
