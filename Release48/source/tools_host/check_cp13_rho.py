#!/usr/bin/env python3
"""Exercise the production quotient/remainder helper across all 255 ratios."""
from pathlib import Path
import re,subprocess,tempfile
R=Path(__file__).resolve().parents[2];A=R/'source/additional'
helper=re.search(r'static inline u32 rho_divrem\(.*?\n}',(A/'mdsnd.c').read_text(),re.S)[0]
with tempfile.TemporaryDirectory(prefix='cp13-rho-') as tmp:
 t=Path(tmp);p=t/'check.c'
 p.write_text('''#include <stdint.h>
#include <stdio.h>
typedef uint32_t u32;typedef uint64_t u64;
#include "'''+str(A/'gsnd/gsnd_rho_recip.h')+'"\n'+helper+'''
int main(void){u32 state=0x13579bdf,count=0;
 for(u32 d=1;d<=255;d++)for(u32 i=0;i<16384;i++){
  state^=state<<13;state^=state>>17;state^=state<<5;
  u32 n=i<1024?i:i<2048?UINT32_MAX-(i-1024):state;
  u32 r,q=rho_divrem(n,d,rho_recip[d],&r);
  if(q!=n/d||r!=n%d)return 1;
  if(q<=131700){u32 unused,step=q*16308u+rho_divrem(r*16308u,d,rho_recip[d],&unused);
   if(step!=(u32)(((u64)n*16308u)/d))return 2;}
  count++;
 }
 printf("PASS %u exact quotient/remainder/pitch cases, all 255 ratios\\n",count);
}
''')
 subprocess.run(['gcc','-O2',str(p),'-o',str(t/'check')],check=True)
 out=subprocess.run([str(t/'check')],capture_output=True,text=True,check=True).stdout
 (R.parent/'validation_cp13/RHO_EXACT.txt').write_text(out);print(out.strip())
