/* replay a captured FM write log through the PSC8 runtime synth (fastopll) and the emu2413 reference */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "fast_opll.h"
#include "emu2413.h"
#define N 304
int main(int argc,char**argv){
  FILE*f=fopen(argv[1],"r"); int fr0,cnt=0; static int ev[9000][4];
  while(fscanf(f,"%d %d %d %d",&ev[cnt][0],&ev[cnt][1],&ev[cnt][2],&ev[cnt][3])==4) cnt++;
  fclose(f);
  FastOPLL fo; fastopll_reset(&fo);
  OPLL *ref=OPLL_new(3579545, 49716); OPLL_reset(ref);
  FILE*o1=fopen("fast.raw","wb"),*o2=fopen("ref.raw","wb");
  int first=ev[0][0], last=ev[cnt-1][0]; int k=0;
  double refpos=0; /* emu2413 samples are generated proportionally: 49716/60 per frame (vs N per frame) */
  for(int fr=first; fr<=last; fr++){
    int done=0; int16_t acc[N];
    while(1){
      int due = (k<cnt && ev[k][0]==fr) ? ev[k][1] : N;
      if(due>done){ fastopll_render(&fo,acc,due-done); for(int i=0;i<due-done;i++){int y=acc[i]>>3; y=y>127?127:y<-128?-128:y; int16_t s=(int16_t)(y*256); fwrite(&s,2,1,o1);}
        /* reference up to the same time */
        double target = (fr-first)*(49716.0/59.7275) + due*(49716.0/59.7275)/N;
        while(refpos<target){ int16_t s=OPLL_calc(ref); fwrite(&s,2,1,o2); refpos+=1; }
        done=due; }
      if(k<cnt && ev[k][0]==fr){ int port=ev[k][2], v=ev[k][3];
        if(port==0){ fastopll_addr(&fo,v); OPLL_writeIO(ref,0,v);} else if(port==1){ fastopll_data(&fo,v); OPLL_writeIO(ref,1,v);} k++; }
      else break;
    }
  }
  fclose(o1); fclose(o2); return 0;
}
