#!/usr/bin/env python3
"""Exact closed-form shift/rotate proof against the sealed CP26 bit loops."""
from pathlib import Path
import subprocess,tempfile,json
R=Path(__file__).resolve().parents[2]; A=R/'source/additional'; V=R.parent/'validation_cp27'
old=(R.parent/'cp26_base/aot/aotrt.h').read_text(); old=old[old.index('static inline u32 f_shift'):old.index('AINL u32 f_bcd_add')].replace('f_shift(', 'old_shift(')
with tempfile.TemporaryDirectory(prefix='cp27-shifts-') as td:
 t=Path(td); c=t/'check.c'
 c.write_text('#include <stdio.h>\n#include <stdint.h>\n#include "'+str(A/'aot/aotrt.h')+'"\n'+old+r'''
static unsigned long long cases;
static u32 rng=0x6d2b79f5u;
static u32 random32(void){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
static int test(u32 v,u32 mask,u32 sign,int count,int type,int left,u32 status){
 u32 s1=status,s2=status,c1=0xffffffe0u,c2=c1;
 u32 a=old_shift(&s1,&c1,type,left,v,count,mask,sign),b=f_shift(&s2,&c2,type,left,v,count,mask,sign);
 if(a!=b||s1!=s2||c1!=c2){fprintf(stderr,"value=%08x mask=%08x count=%d type=%d left=%d SR=%08x: old %08x/%08x/%08x new %08x/%08x/%08x\n",v,mask,count,type,left,status,a,s1,c1,b,s2,c2);return 1;}
 cases++;return 0;
}
int main(void){
 for(u32 v=0;v<256;v++)for(int n=0;n<64;n++)for(int type=0;type<4;type++)for(int d=0;d<2;d++)for(u32 sr=0;sr<32;sr++)
  if(test(v|0xa5c30000u,255,128,n,type,d,0x2700|sr))return 1;
 const int counts[]={0,1,2,3,4,5,6,7,8,14,15,16,17,30,31,32,33,63};
 for(u32 v=0;v<65536;v++)for(unsigned j=0;j<sizeof counts/sizeof *counts;j++)for(int type=0;type<4;type++)for(int d=0;d<2;d++)
  if(test(v|0xc3910000u,65535,32768,counts[j],type,d,0xabcd2700u|((v+j)&31)))return 1;
 const u32 edges[]={0,1,2,3,0x7fffffff,0x80000000,0x80000001,0xfffffffe,0xffffffff,0x55555555,0xaaaaaaaa,0x40000000,0xc0000000};
 for(unsigned j=0;j<sizeof edges/sizeof *edges;j++)for(int n=0;n<64;n++)for(int type=0;type<4;type++)for(int d=0;d<2;d++)for(u32 sr=0;sr<32;sr++)
  if(test(edges[j],0xffffffff,0x80000000,n,type,d,0x2700|sr))return 1;
 for(u32 j=0;j<250000;j++){u32 v=random32(),sr=random32(),spec=random32();if(test(v,0xffffffff,0x80000000,spec&63,(spec>>6)&3,(spec>>8)&1,sr))return 1;}
 /* Same reciprocal used in ARM UMULL; exhaustive guest budgets and sampled
  * uint32/event-wrap budgets are checked independently against division. */
 for(u32 n=0;n<2000000;n++)if((((uint64_t)n*0x4ec4ec4fu)>>35)!=n/26u)return 2;
 for(u32 j=0;j<2000000;j++){u32 n=random32();if((((uint64_t)n*0x4ec4ec4fu)>>35)!=n/26u)return 3;}
 for(u32 j=0;j<1024;j++){u32 n=0xffffffffu-j;if((((uint64_t)n*0x4ec4ec4fu)>>35)!=n/26u)return 4;}
 printf("%llu\n",cases);return 0;
}
''')
 subprocess.run(['gcc','-O2','-DMD_HOST','-fsanitize=undefined','-fno-sanitize-recover=undefined',str(c),'-o',str(t/'check')],check=True)
 p=subprocess.run([str(t/'check')],check=True,capture_output=True,text=True)
 result={'all_pass':True,'operand_flag_cycle_cases':int(p.stdout),'baseline':'sealed CP26 per-bit arithmetic shifts/rotates','coverage':'exhaustive byte values/counts/32 CCRs; exhaustive word values at 18 boundary/immediate counts; long edges/all counts/CCRs plus 250000 random full uint32 operand/status samples; all 4 instruction types, both directions; high operand bits, zero counts, counts through 63 and cycle wrap; undefined-behavior sanitizer','reciprocal26_cases':4001024}
 (V/'SHIFTS_EXACT.json').write_text(json.dumps(result,indent=2)+'\n');print(result)
