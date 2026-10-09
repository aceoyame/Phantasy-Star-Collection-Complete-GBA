/* host test: the closed-form audio_catchup in psc8.c must equal the per-sample loop it replaced.  gcc -O2 -o /tmp/ct tools/catchup_test.c && /tmp/ct */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#define N 304
static uint32_t rs=12345; static uint32_t rnd(void){rs=rs*1664525u+1013904223u;return rs>>8;}
int main(void){
  long bad=0,tot=0;
  for(long it=0;it<20000000;it++){
    uint32_t due_cyc=rnd()*7919u, due_ph=rnd()%N; unsigned aud_due=rnd()%(N+1);
    int32_t delta=(int32_t)(rnd()%(rnd()%3==0?70000:rnd()%2?600:250))-(int32_t)(rnd()%3==0?50:0);
    uint32_t now=due_cyc+delta;
    /* reference */
    uint32_t c1=due_cyc,p1=due_ph; unsigned a1=aud_due;
    while(a1<N&&(int32_t)(now-c1)>=0){a1++;p1+=59736u%N;uint32_t d=59736u/N;if(p1>=N){p1-=N;d++;}c1+=d;}
    /* closed form */
    uint32_t c2=due_cyc,p2=due_ph; unsigned a2=aud_due;
    int32_t dl=(int32_t)(now-c2);
    if(dl>=0&&a2<N){
      unsigned a=p2>=152u, b=p2-152u*a, m=1;
      if(dl>=196){ m=((uint32_t)dl*334u)>>16;
        while(196u*m+((a+m)>>1)>(uint32_t)dl) m--;
        while(196u*(m+1)+((a+m+1)>>1)<=(uint32_t)dl) m++;
        m++; }
      unsigned room=N-a2; if(m>room)m=room;
      c2+=196u*m+((a+m)>>1); p2=b+(((a+m)&1u)?152u:0u); a2+=m;
    }
    tot++; if(c1!=c2||p1!=p2||a1!=a2){ if(bad<5)printf("MISMATCH delta %d due_ph %u a %u: ref (%u,%u,%u) cf (%u,%u,%u)\n",delta,due_ph,aud_due,c1-due_cyc,p1,a1,c2-due_cyc,p2,a2); bad++; }
  }
  printf("%ld cases, %ld mismatches\n",tot,bad); return bad!=0;
}
