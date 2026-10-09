from pathlib import Path
W=Path(__file__).resolve().parents[1];A=W/'Release48/source/additional';D=W/'native58'
(A/'md.c').write_bytes((D/'accepted_md.c').read_bytes())
(A/'aot/generated/aot_code.c').write_bytes((D/'accepted_aot_code.c').read_bytes())
p=A/'md.c';p.write_text(p.read_text()+'''\n/* CP58: exact complete iterations of the packed-nibble repeat decoder.
 * Partial cycles and the zero-count flush branch keep their original path. */
__attribute__((section(".aotmore"),noinline))
u32 md_native_repeat_nibble(u32 room,u32 sr){
 u32 count=(C.d[0]&65535u)+1u,flush=C.d[3]&65535u;
 if(room<60u||flush<3u)return 0;
 u32 n=room/30u;if(n>count)n=count;if(n>=flush)n=flush-1u;
 if(n<2u)return 0;
 u32 v=C.d[1]&255u,x=C.d[4];
 if(n>=8u){u32 lo=v&15u,mid=lo|(v>>4);x=lo|((mid*0x11111111u)&0xfffffff0u);}
 else for(u32 k=0;k<n;k++)x=(x<<4)|v;
 C.d[4]=x;C.d[0]=(C.d[0]&0xffff0000u)|((C.d[0]-n)&65535u);
 flush-=n;C.d[3]=(C.d[3]&0xffff0000u)|flush;
 C.sr=(u16)((sr&~31u)|((flush&32768u)?8u:0u)|(flush==32767u?2u:0u));
 return n*30u;
}
''')
p=A/'aot/generated/aot_code.c';s=p.read_text();start=s.index('u32 aotf33(u32 entry){');end=s.index('u32 aotf34(u32 entry){',start);f=s[start:end]
k='B3708: if((s32)(cyc-lim)>=0){ i=7; goto ex; }';assert f.count(k)==1
f=f.replace(k,k+'\n{u32 elapsed=md_native_repeat_nibble(lim-cyc,sr);\n if(elapsed){cyc+=elapsed;sr=C.sr;if((AD[0]&65535u)==65535u)goto B371A;goto B3708;}}',1)
s=s[:start]+'extern u32 md_native_repeat_nibble(u32 room,u32 sr);\n'+f+s[end:];p.write_text(s)
