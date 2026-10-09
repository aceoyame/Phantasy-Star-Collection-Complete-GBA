"""Share five identical checksum translations and batch complete pure-read
ADD.L / DBRA iterations within the original guest instruction budget."""
from pathlib import Path
import re,json
W=Path(__file__).resolve().parents[1];A=W/'Release48/source/additional';D=W/'native58'
p=A/'md.c';s=(W/'cp57_base/md.c').read_text();s+='''
/* CP58: one native checksum transaction; only complete 16-cycle iterations
 * over aligned ordinary ROM/RAM are batched. Partial instructions and MMIO
 * use the original AOT body. No guest writes, events or IRQ edges occur here. */
typedef u32 __attribute__((may_alias)) ChecksumWord;
__attribute__((section(".aotmore"),noinline))
u32 md_native_checksum(u32 room,u32 sr){
 u32 a=C.a[0]&0xffffffu,count=(C.d[1]&65535u)+1u;
 if(room<32u||(a&3u))return 0;
 u32 n=room>>4;if(n>count)n=count;
 u32 ram=a>=0xe00000u,avail;
 const ChecksumWord*p;
 if(ram){avail=(65536u-(a&65535u))>>2;p=(const ChecksumWord*)(md_ram+((a&65535u)>>1));}
 else {if(a>=m68k_romsize)return 0;avail=(4096u-(a&4095u))>>2;
  u32 tail=(m68k_romsize-a)>>2;if(avail>tail)avail=tail;
  p=(const ChecksumWord*)(m68k_romp[a>>12]+(a&4095u));}
 if((uintptr_t)p&3u)return 0;
 if(n>avail)n=avail;if(n<2u)return 0;
 u32 sum=C.d[0],before=0,value=0;
 for(u32 k=0;k<n;k++){
  u32 v=p[k];
  if(ram)value=(v<<16)|(v>>16);
  else value=((v&255u)<<24)|((v&0xff00u)<<8)|((v>>8)&0xff00u)|(v>>24);
  before=sum;sum+=value;
 }
 C.d[0]=sum;C.d[1]=(C.d[1]&0xffff0000u)|((C.d[1]-n)&65535u);C.a[0]+=n*4u;
 C.sr=(u16)((sr&~31u)|(sum?0u:4u)|((sum>>28)&8u)|((~(before^value)&(before^sum))>>30&2u)|(sum<value?17u:0u));
 return n*16u;
}
''';p.write_text(s)
code=(D/'text_code.c').read_text()
# Verify the complete five original functions differ only by their PCs.
bs=[]
for fn in [86,87,88,89,90,92]:
 m=re.search(r'^u32 aotf'+str(fn)+r'\(u32 entry\)\{(.*?)(?=^u32 aotf\d+|^__attribute__|\Z)',code,re.M|re.S);assert m
 body=m[0].rstrip();base=int(re.search(r'xpc\[\] AOT_TAB=\{0x([0-9a-fA-F]+)',body)[1],16)
 norm=body.replace('aotf'+str(fn),'@FN@')
 for pc in (base,base+2,base+6):norm=norm.replace(f'{pc:X}','@P'+str(pc-base)+'@')
 bs.append(norm)
assert all(b==bs[0] for b in bs), 'checksum bodies differ beyond relocation'
new='''u32 md_native_checksum(u32,u32);
u32 aotf86(u32 entry){
 u32 sr=C.sr,cyc=C.cycles,npc,rc=AOT_CONT;const u32 lim=aot_lim;
 if(entry>1u)return rc;
 const u32 root=C.pc-(entry?2u:0u);
 if(entry)goto db;
add:
 if((s32)(cyc-lim)>=0){npc=root;goto out;}
 {u32 elapsed=md_native_checksum(lim-cyc,sr);
  if(elapsed){cyc+=elapsed;sr=C.sr;if((AD[1]&65535u)==65535u){npc=root+6u;goto out;}goto add;}}
 {u32 a=AA[0];AA[0]+=4u;cyc+=12u;u32 v=R32(a);
  AD[0]=ADDL(31u,v,AD[0],0xffffffffu,0x80000000u,32,0,1,0);}
db:
 if((s32)(cyc-lim)>=0){npc=root+2u;goto out;}
 cyc+=4u;{u32 w=(AD[1]-1u)&65535u;AD[1]=(AD[1]&0xffff0000u)|w;if(w!=65535u)goto add;}
 npc=root+6u;
out:C.sr=(u16)sr;C.cycles=cyc;C.pc=npc;return rc;
}
'''
for fn in [86,87,88,89,90,92]:
 code=re.sub(r'^u32 aotf'+str(fn)+r'\(u32 entry\)\{.*?(?=^u32 aotf\d+|^__attribute__|\Z)',new if fn==86 else 'u32 aotf'+str(fn)+'(u32) __attribute__((alias("aotf86")));\n',code,flags=re.M|re.S)
(A/'aot/generated/aot_code.c').write_text(code)
(D/'CHECKSUM_TRANSFORMATION.json').write_text(json.dumps({'six_original_translations_identical_except_PC':True,'functions':[86,87,88,89,90,92],'shared_body':86,'complete_iteration_guest_cycles':16,'batch_boundary':'aligned pure ROM/RAM; within current 4KiB ROM page / 64KiB RAM boundary; count and instruction budget bounded','CCR':'exact last addition N/Z/V/X/C; fallback retains every partial instruction'},indent=2)+'\n')
