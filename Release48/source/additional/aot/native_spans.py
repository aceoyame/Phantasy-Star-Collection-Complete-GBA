"""Fuse exact MOVE.B/postincrement + DBRA pairs into event-bounded RAM spans.
The generated ordinary instructions remain the fallback. Register-A7 byte
stepping and source/counter alias cases are deliberately excluded.
"""
import re
LABEL=r'(?P<label>B[0-9A-F]+): if\(\(s32\)\(cyc-lim\)>=0\)\{ i=\d+; goto ex; \}\n'
COPY=r'\{ cyc\+=4;\nu32 ea0=AA\[(?P<src>[0-6])\]; AA\[(?P=src)\]\+=1u;\ncyc\+=4; u32 r2=R8\(ea0\);\nu32 mv=r2;\n'
FILL=r'\{ cyc\+=4;\nu32 mv=\(AD\[(?P<val>[0-7])\]&0xFFu\);\n'
TAIL=r'u32 ea1=AA\[(?P<dst>[0-6])\]; AA\[(?P=dst)\]\+=1u;\ncyc\+=4; W8\(ea1,mv\);\nNZL\((?P<live>\d+)u,mv,0xFFu,0x80u\); \}\nB[0-9A-F]+: if\(\(s32\)\(cyc-lim\)>=0\)\{ i=\d+; goto ex; \}\n\{ cyc\+=4;\nif\(!0\)\{ u32 w=\(AD\[(?P<count>[0-7])\]-1u\)&0xFFFFu; AD\[(?P=count)\]=\(AD\[(?P=count)\]&0xFFFF0000u\)\|w; if\(w!=0xFFFFu\)\{ goto (?P=label); \} \} \}\n'
def fuse(text):
 sites=[]
 for copy,body in [(True,COPY),(False,FILL)]:
  def repl(m):
   q=m.groupdict();c=q['count'];d=q['dst'];src=q.get('src');v=q.get('val');L=int(q['live']);label=q['label']
   if (copy and src==d) or (not copy and v==c):return m.group(0)
   step=16 if copy else 12;div='((lim-cyc)>>4)' if copy else 'aot_div12_arm(lim-cyc)';fill='256u' if copy else f'(AD[{v}]&255u)';source=f'AA[{src}]' if copy else '0u'
   code=f'''/* CP24_NATIVE_RAM_SPAN: exact {step}-cycle MOVE/DBRA pairs. */
{{
 u32 n={div},count=(AD[{c}]&65535u)+1u;if(n>count)n=count;
 if(n>=4u&&md_native_ram_bytes({source},AA[{d}],n,{fill})){{
'''
   if copy:code+=f'  AA[{src}]+=n;\n'
   code+=f'  AA[{d}]+=n;cyc+=n*{step}u;AD[{c}]=(AD[{c}]&0xFFFF0000u)|((AD[{c}]-n)&65535u);\n'
   if L&15:
    value=f'RAM8((AA[{d}]-1u)&65535u)' if copy else f'AD[{v}]'
    code+=f'  NZL({L}u,{value},255u,128u);\n'
   code+=f'  if((AD[{c}]&65535u)==65535u)goto NATIVE_DONE_{label};\n  goto {label};\n }}\n}}\n'
   original=m.group(0);head=original[:original.index('{ cyc+=4;')]
   sites.append(label)
   return head+code+original[len(head):]+f'NATIVE_DONE_{label}:;\n'
  text=re.sub(LABEL+body+TAIL,repl,text)
 text,nibble_sites=fuse_nibbles(text);sites+=nibble_sites
 text,scan_sites=fuse_text_scans(text);sites+=scan_sites
 text,glyph_sites=fuse_glyphs(text);sites+=glyph_sites
 text,row_sites=fuse_rows(text);sites+=row_sites
 text,shifted_sites=fuse_shifted(text);sites+=shifted_sites
 return text,sites
NIBBLE_BODY = 'B2BC12: if((s32)(cyc-lim)>=0){ i=2; goto ex; }\n{ cyc+=4;\nu32 ea0=AA[1];\ncyc+=4; u32 r2=R8(ea0);\nu32 mv=r2;\nAD[2]=(AD[2]&~0xFFu)|((mv)&0xFFu);\nNZL(0u,mv,0xFFu,0x80u); }\nB2BC14: if((s32)(cyc-lim)>=0){ i=3; goto ex; }\n{ cyc+=4;\nu32 ea0=AA[2];\ncyc+=4; u32 r2=R8(ea0);\nu32 mv=r2;\nAD[3]=(AD[3]&~0xFFu)|((mv)&0xFFu);\nNZL(0u,mv,0xFFu,0x80u); }\nB2BC16: if((s32)(cyc-lim)>=0){ i=4; goto ex; }\n{ cyc+=4;\n{ u32 r=f_shift(&sr,&cyc,1,0,AD[2],4,0xFFu,0x80u); AD[2]=(AD[2]&~0xFFu)|(r&0xFFu); } }\nB2BC18: if((s32)(cyc-lim)>=0){ i=5; goto ex; }\n{ cyc+=4;\nCMPL(4u,(AD[2]&0xFFu),AD[5],0xFFu,0x80u); }\nB2BC1A: if((s32)(cyc-lim)>=0){ i=6; goto ex; }\n{ cyc+=4;\nif(CC(sr,6)){ cyc+=2;  goto B2BC20; } }\nB2BC1C: if((s32)(cyc-lim)>=0){ i=7; goto ex; }\n{ cyc+=4;\nu32 ea0=AA[2];\ncyc+=4; u32 r2=R8(ea0);\nu32 mv=r2;\nAD[2]=(AD[2]&~0xFFu)|((mv)&0xFFu);\nNZL(0u,mv,0xFFu,0x80u); }\nB2BC1E: if((s32)(cyc-lim)>=0){ i=8; goto ex; }\n{ cyc+=4;\n{ u32 r=f_shift(&sr,&cyc,1,0,AD[2],4,0xFFu,0x80u); AD[2]=(AD[2]&~0xFFu)|(r&0xFFu); } }\nB2BC20: if((s32)(cyc-lim)>=0){ i=9; goto ex; }\n{ cyc+=4;\n{ u32 r=f_shift(&sr,&cyc,1,1,AD[2],4,0xFFu,0x80u); AD[2]=(AD[2]&~0xFFu)|(r&0xFFu); } }\nB2BC22: if((s32)(cyc-lim)>=0){ i=10; goto ex; }\n{ cyc+=4;\nu32 mv=(AD[2]&0xFFu);\nAD[3]=(AD[3]&~0xFFu)|((mv)&0xFFu);\nNZL(16u,mv,0xFFu,0x80u); }\nB2BC24: if((s32)(cyc-lim)>=0){ i=11; goto ex; }\n{ cyc+=4;\nu32 ea0=AA[1]; AA[1]+=1u;\ncyc+=4; u32 r2=R8(ea0);\nu32 mv=r2;\nAD[2]=(AD[2]&~0xFFu)|((mv)&0xFFu);\nNZL(16u,mv,0xFFu,0x80u); }\nB2BC26: if((s32)(cyc-lim)>=0){ i=12; goto ex; }\n{ cyc+=4;\ncyc+=4;\nu32 r=(AD[2]&0xFu)&0xFFu;\nNZL(16u,r,0xFFu,0x80u);\nAD[2]=(AD[2]&~0xFFu)|r; }\nB2BC2A: if((s32)(cyc-lim)>=0){ i=13; goto ex; }\n{ cyc+=4;\nCMPL(20u,(AD[2]&0xFFu),AD[5],0xFFu,0x80u); }\nB2BC2C: if((s32)(cyc-lim)>=0){ i=14; goto ex; }\n{ cyc+=4;\nif(CC(sr,6)){ cyc+=2;  goto B2BC34; } }\nB2BC2E: if((s32)(cyc-lim)>=0){ i=15; goto ex; }\n{ cyc+=4;\nu32 ea0=AA[2];\ncyc+=4; u32 r2=R8(ea0);\nu32 mv=r2;\nAD[2]=(AD[2]&~0xFFu)|((mv)&0xFFu);\nNZL(16u,mv,0xFFu,0x80u); }\nB2BC30: if((s32)(cyc-lim)>=0){ i=16; goto ex; }\n{ cyc+=4;\ncyc+=4;\nu32 r=(AD[2]&0xFu)&0xFFu;\nNZL(16u,r,0xFFu,0x80u);\nAD[2]=(AD[2]&~0xFFu)|r; }\nB2BC34: if((s32)(cyc-lim)>=0){ i=17; goto ex; }\n{ cyc+=4;\nu32 r=(AD[3]|(AD[2]&0xFFu))&0xFFu;\nNZL(16u,r,0xFFu,0x80u);\nAD[3]=(AD[3]&~0xFFu)|r; }\nB2BC36: if((s32)(cyc-lim)>=0){ i=18; goto ex; }\n{ cyc+=4;\nu32 mv=(AD[3]&0xFFu);\nu32 ea1=AA[2]; AA[2]+=1u;\ncyc+=4; W8(ea1,mv);\nNZL(31u,mv,0xFFu,0x80u); }\nB2BC38: if((s32)(cyc-lim)>=0){ i=19; goto ex; }\n{ cyc+=4;\nif(!0){ u32 w=(AD[0]-1u)&0xFFFFu; AD[0]=(AD[0]&0xFFFF0000u)|w; if(w!=0xFFFFu){ goto B2BC12; } } }\n'
def fuse_nibbles(text):
 # Restrict this fusion to the exact shared ROM loop with every known fallback
 # instruction retained; raw regeneration still has its earlier fusion guard.
 head='B2BC12: if((s32)(cyc-lim)>=0){ i=2; goto ex; }\n'
 code="""/* CP26_NATIVE_NIBBLE_BLEND: complete event-bounded 96..128-cycle iterations. */
{
 u32 elapsed=md_native_nibble_blend(lim-cyc,sr);
 if(elapsed){cyc+=elapsed;sr=C.sr;if((AD[0]&65535u)==65535u)goto B2BC3C;goto B2BC12;}
}
"""
 if head not in text or NIBBLE_BODY not in text:return text,[]
 if 'CP26_NATIVE_NIBBLE_BLEND' in text:return text,[]
 return text.replace(head,head+code),['B2BC12']
SCAN_BODIES = {'B2BA0E': 'B2BA0E: if((s32)(cyc-lim)>=0){ i=20; goto ex; }\n{ cyc+=4;\nu32 ea0=AA[1];\ncyc+=4; u32 r2=R8(ea0);\nNZL(20u,r2,0xFFu,0x80u); }\nB2BA10: if((s32)(cyc-lim)>=0){ i=21; goto ex; }\n{ cyc+=4;\nif(CC(sr,7)){ cyc+=2;  goto B2BA06; } }\nB2BA12: if((s32)(cyc-lim)>=0){ i=22; goto ex; }\n{ cyc+=4;\nu32 ea0=AA[1]; AA[1]+=1u;\ncyc+=4; u32 r2=R8(ea0);\nCMPL(20u,r2,AD[1],0xFFu,0x80u); }\nB2BA14: if((s32)(cyc-lim)>=0){ i=23; goto ex; }\n{ cyc+=4;\nif(CC(sr,6)){ cyc+=2;  goto B2BA0E; } }\n', 'B2BA62': 'B2BA62: if((s32)(cyc-lim)>=0){ i=42; goto ex; }\n{ cyc+=4;\nu32 ea0=AA[0]; AA[0]+=2u;\ncyc+=4; u32 r2=R16(ea0);\nCMPL(31u,r2,AD[1],0xFFFFu,0x8000u); }\nB2BA64: if((s32)(cyc-lim)>=0){ i=43; goto ex; }\n{ cyc+=4;\nif(CC(sr,7)){ cyc+=2;  i=44; goto ex; } }\nB2BA66: if((s32)(cyc-lim)>=0){ i=45; goto ex; }\n{ cyc+=4;\nu32 ea0=AA[0]; AA[0]+=2u;\ncyc+=4; u32 r2=R16(ea0);\nNZL(20u,r2,0xFFFFu,0x8000u); }\nB2BA68: if((s32)(cyc-lim)>=0){ i=46; goto ex; }\n{ cyc+=4;\nif(CC(sr,6)){ cyc+=2;  goto B2BA62; } }\n'}
def fuse_text_scans(text):
 sites=[]
 for label,name in [('B2BA0E','class'),('B2BA62','kern')]:
  body=SCAN_BODIES[label]
  if body not in text:continue
  head=body[:body.index('\n')+1]
  match='goto B2BA16;' if name=='class' else 'i=44;goto ex;'
  sentinel='goto B2BA06;' if name=='class' else 'goto B2BA6A;'
  code=f"""/* CP27_NATIVE_{name.upper()}_SCAN: exact nonmatching iterations and complete exits. */
{{
 u32 elapsed=md_native_{name}_scan(lim-cyc,sr);
 if(elapsed){{cyc+=elapsed&0x3FFFFFFFu;sr=C.sr;if(elapsed&0x40000000u){{{match}}}if(elapsed&0x80000000u){{{sentinel}}}goto {label};}}
}}
"""
  text=text.replace(head,head+code);sites.append(label)
 return text,sites
GLYPH_BODY = 'B2BACA: if((s32)(cyc-lim)>=0){ i=15; goto ex; }\n{ cyc+=4;\nu32 ea0=AA[1]; AA[1]+=1u;\ncyc+=4; u32 r2=R8(ea0);\nu32 mv=r2;\nAD[1]=(AD[1]&~0xFFu)|((mv)&0xFFu);\nNZL(0u,mv,0xFFu,0x80u); }\nB2BACC: if((s32)(cyc-lim)>=0){ i=16; goto ex; }\n{ cyc+=4;\nu32 mv=(AD[1]&0xFFu);\nAD[2]=(AD[2]&~0xFFu)|((mv)&0xFFu);\nNZL(0u,mv,0xFFu,0x80u); }\nB2BACE: if((s32)(cyc-lim)>=0){ i=17; goto ex; }\n{ cyc+=4;\n{ u32 r=f_shift(&sr,&cyc,1,0,AD[2],4,0xFFu,0x80u); AD[2]=(AD[2]&~0xFFu)|(r&0xFFu); } }\nB2BAD0: if((s32)(cyc-lim)>=0){ i=18; goto ex; }\n{ cyc+=4;\nif(CC(sr,6)){ cyc+=2;  goto B2BAD6; } }\nB2BAD2: if((s32)(cyc-lim)>=0){ i=19; goto ex; }\n{ cyc+=4;\nu32 mv=(AD[4]&0xFFu);\nAD[3]=(AD[3]&~0xFFu)|((mv)&0xFFu);\nNZL(0u,mv,0xFFu,0x80u); }\nB2BAD4: if((s32)(cyc-lim)>=0){ i=20; goto ex; }\n{ cyc+=4;\ncyc+=2;\n\ngoto B2BAD8; }\nB2BAD6: if((s32)(cyc-lim)>=0){ i=21; goto ex; }\n{ cyc+=4;\nu32 mv=(AD[5]&0xFFu);\nAD[3]=(AD[3]&~0xFFu)|((mv)&0xFFu);\nNZL(0u,mv,0xFFu,0x80u); }\nB2BAD8: if((s32)(cyc-lim)>=0){ i=22; goto ex; }\n{ cyc+=4;\n{ u32 r=f_shift(&sr,&cyc,1,1,AD[3],4,0xFFu,0x80u); AD[3]=(AD[3]&~0xFFu)|(r&0xFFu); } }\nB2BADA: if((s32)(cyc-lim)>=0){ i=23; goto ex; }\n{ cyc+=4;\nu32 mv=(AD[1]&0xFFu);\nAD[2]=(AD[2]&~0xFFu)|((mv)&0xFFu);\nNZL(16u,mv,0xFFu,0x80u); }\nB2BADC: if((s32)(cyc-lim)>=0){ i=24; goto ex; }\n{ cyc+=4;\ncyc+=4;\nu32 r=(AD[2]&0xFu)&0xFFu;\nNZL(20u,r,0xFFu,0x80u);\nAD[2]=(AD[2]&~0xFFu)|r; }\nB2BAE0: if((s32)(cyc-lim)>=0){ i=25; goto ex; }\n{ cyc+=4;\nif(CC(sr,6)){ cyc+=2;  goto B2BAE6; } }\nB2BAE2: if((s32)(cyc-lim)>=0){ i=26; goto ex; }\n{ cyc+=4;\nu32 r=(AD[3]|(AD[4]&0xFFu))&0xFFu;\nNZL(16u,r,0xFFu,0x80u);\nAD[3]=(AD[3]&~0xFFu)|r; }\nB2BAE4: if((s32)(cyc-lim)>=0){ i=27; goto ex; }\n{ cyc+=4;\ncyc+=2;\n\ngoto B2BAE8; }\nB2BAE6: if((s32)(cyc-lim)>=0){ i=28; goto ex; }\n{ cyc+=4;\nu32 r=(AD[3]|(AD[5]&0xFFu))&0xFFu;\nNZL(16u,r,0xFFu,0x80u);\nAD[3]=(AD[3]&~0xFFu)|r; }\nB2BAE8: if((s32)(cyc-lim)>=0){ i=29; goto ex; }\n{ cyc+=4;\nu32 mv=(AD[3]&0xFFu);\nu32 ea1=AA[0]; AA[0]+=1u;\ncyc+=4; W8(ea1,mv);\nNZL(31u,mv,0xFFu,0x80u); }\nB2BAEA: if((s32)(cyc-lim)>=0){ i=30; goto ex; }\n{ cyc+=4;\nif(!0){ u32 w=(AD[0]-1u)&0xFFFFu; AD[0]=(AD[0]&0xFFFF0000u)|w; if(w!=0xFFFFu){ goto B2BACA; } } }\n'
def fuse_glyphs(text):
 head='B2BACA: if((s32)(cyc-lim)>=0){ i=15; goto ex; }\n'
 code='/* CP28_NATIVE_GLYPH_BYTES: complete event-bounded 80/84/88-cycle iterations. */\n{\n u32 elapsed=md_native_glyph_bytes(lim-cyc,sr);\n if(elapsed){cyc+=elapsed;sr=C.sr;if((AD[0]&65535u)==65535u)goto B2BAEE;goto B2BACA;}\n}\n'
 if GLYPH_BODY not in text or 'CP28_NATIVE_GLYPH_BYTES' in text:return text,[]
 return text.replace(head,head+code),['B2BACA']

ROW_BODY = 'B2BC8A: if((s32)(cyc-lim)>=0){ i=2; goto ex; }\n{ cyc+=4;\nu32 ea0=AA[1];\ncyc+=8; u32 r2=R32(ea0);\nu32 mv=r2;\nu32 ea1=AA[3]; AA[3]+=4u;\ncyc+=8; W32(ea1,mv);\nNZL(31u,mv,0xFFFFFFFFu,0x80000000u); }\nB2BC8C: if((s32)(cyc-lim)>=0){ i=3; goto ex; }\n{ cyc+=4;\nAA[1]+=(u32)(s32)(s16)(AD[1]&0xFFFFu); }\nB2BC8E: if((s32)(cyc-lim)>=0){ i=4; goto ex; }\n{ cyc+=4;\nif(!0){ u32 w=(AD[0]-1u)&0xFFFFu; AD[0]=(AD[0]&0xFFFF0000u)|w; if(w!=0xFFFFu){ goto B2BC8A; } } }\n'
def fuse_rows(text):
 head='B2BC8A: if((s32)(cyc-lim)>=0){ i=2; goto ex; }\n'
 code='/* CP29_NATIVE_GATHER_ROWS: complete event-bounded 28-cycle iterations. */\n{\n u32 elapsed=md_native_gather_rows(lim-cyc,sr);\n if(elapsed){cyc+=elapsed;sr=C.sr;if((AD[0]&65535u)==65535u)goto B2BC92;goto B2BC8A;}\n}\n'
 if ROW_BODY not in text or 'CP29_NATIVE_GATHER_ROWS' in text:return text,[]
 return text.replace(head,head+code),['B2BC8A']

SHIFTED_BODY = 'B2BC4C: if((s32)(cyc-lim)>=0){ i=2; goto ex; }\n{ cyc+=4;\nu32 ea0=AA[1];\ncyc+=4; u32 r2=R8(ea0);\nu32 mv=r2;\nAD[2]=(AD[2]&~0xFFu)|((mv)&0xFFu);\nNZL(0u,mv,0xFFu,0x80u); }\nB2BC4E: if((s32)(cyc-lim)>=0){ i=3; goto ex; }\n{ cyc+=4;\nu32 ea0=AA[2];\ncyc+=4; u32 r2=R8(ea0);\nu32 mv=r2;\nAD[3]=(AD[3]&~0xFFu)|((mv)&0xFFu);\nNZL(0u,mv,0xFFu,0x80u); }\nB2BC50: if((s32)(cyc-lim)>=0){ i=4; goto ex; }\n{ cyc+=4;\n{ u32 r=f_shift(&sr,&cyc,1,0,AD[2],4,0xFFu,0x80u); AD[2]=(AD[2]&~0xFFu)|(r&0xFFu); } }\nB2BC52: if((s32)(cyc-lim)>=0){ i=5; goto ex; }\n{ cyc+=4;\nCMPL(20u,(AD[2]&0xFFu),AD[5],0xFFu,0x80u); }\nB2BC54: if((s32)(cyc-lim)>=0){ i=6; goto ex; }\n{ cyc+=4;\nif(CC(sr,7)){ cyc+=2;  goto B2BC5C; } }\nB2BC56: if((s32)(cyc-lim)>=0){ i=7; goto ex; }\n{ cyc+=4;\ncyc+=4;\nu32 r=(AD[3]&0xF0u)&0xFFu;\nNZL(16u,r,0xFFu,0x80u);\nAD[3]=(AD[3]&~0xFFu)|r; }\nB2BC5A: if((s32)(cyc-lim)>=0){ i=8; goto ex; }\n{ cyc+=4;\nu32 r=(AD[3]|(AD[2]&0xFFu))&0xFFu;\nNZL(16u,r,0xFFu,0x80u);\nAD[3]=(AD[3]&~0xFFu)|r; }\nB2BC5C: if((s32)(cyc-lim)>=0){ i=9; goto ex; }\n{ cyc+=4;\nu32 mv=(AD[3]&0xFFu);\nu32 ea1=AA[2]; AA[2]+=1u;\ncyc+=4; W8(ea1,mv);\nNZL(16u,mv,0xFFu,0x80u); }\nB2BC5E: if((s32)(cyc-lim)>=0){ i=10; goto ex; }\n{ cyc+=4;\nu32 ea0=AA[1]; AA[1]+=1u;\ncyc+=4; u32 r2=R8(ea0);\nu32 mv=r2;\nAD[2]=(AD[2]&~0xFFu)|((mv)&0xFFu);\nNZL(16u,mv,0xFFu,0x80u); }\nB2BC60: if((s32)(cyc-lim)>=0){ i=11; goto ex; }\n{ cyc+=4;\ncyc+=4;\nu32 r=(AD[2]&0xFu)&0xFFu;\nNZL(16u,r,0xFFu,0x80u);\nAD[2]=(AD[2]&~0xFFu)|r; }\nB2BC64: if((s32)(cyc-lim)>=0){ i=12; goto ex; }\n{ cyc+=4;\nCMPL(31u,(AD[2]&0xFFu),AD[5],0xFFu,0x80u); }\nB2BC66: if((s32)(cyc-lim)>=0){ i=13; goto ex; }\n{ cyc+=4;\nif(CC(sr,7)){ cyc+=2;  goto B2BC74; } }\nB2BC68: if((s32)(cyc-lim)>=0){ i=14; goto ex; }\n{ cyc+=4;\n{ u32 r=f_shift(&sr,&cyc,1,1,AD[2],4,0xFFu,0x80u); AD[2]=(AD[2]&~0xFFu)|(r&0xFFu); } }\nB2BC6A: if((s32)(cyc-lim)>=0){ i=15; goto ex; }\n{ cyc+=4;\nu32 ea0=AA[2];\ncyc+=4; u32 r2=R8(ea0);\nu32 mv=r2;\nAD[3]=(AD[3]&~0xFFu)|((mv)&0xFFu);\nNZL(16u,mv,0xFFu,0x80u); }\nB2BC6C: if((s32)(cyc-lim)>=0){ i=16; goto ex; }\n{ cyc+=4;\ncyc+=4;\nu32 r=(AD[3]&0xFu)&0xFFu;\nNZL(16u,r,0xFFu,0x80u);\nAD[3]=(AD[3]&~0xFFu)|r; }\nB2BC70: if((s32)(cyc-lim)>=0){ i=17; goto ex; }\n{ cyc+=4;\nu32 r=(AD[3]|(AD[2]&0xFFu))&0xFFu;\nNZL(16u,r,0xFFu,0x80u);\nAD[3]=(AD[3]&~0xFFu)|r; }\nB2BC72: if((s32)(cyc-lim)>=0){ i=18; goto ex; }\n{ cyc+=4;\nu32 mv=(AD[3]&0xFFu);\nu32 ea1=AA[2];\ncyc+=4; W8(ea1,mv);\nNZL(31u,mv,0xFFu,0x80u); }\nB2BC74: if((s32)(cyc-lim)>=0){ i=19; goto ex; }\n{ cyc+=4;\nif(!0){ u32 w=(AD[0]-1u)&0xFFFFu; AD[0]=(AD[0]&0xFFFF0000u)|w; if(w!=0xFFFFu){ goto B2BC4C; } } }\n'
def fuse_shifted(text):
 head='B2BC4C: if((s32)(cyc-lim)>=0){ i=2; goto ex; }\n'
 code='/* CP29_NATIVE_SHIFTED_NIBBLES: complete event-bounded 76..124-cycle iterations. */\n{\n u32 elapsed=md_native_shifted_nibbles(lim-cyc,sr);\n if(elapsed){cyc+=elapsed;sr=C.sr;if((AD[0]&65535u)==65535u)goto B2BC78;goto B2BC4C;}\n}\n'
 if SHIFTED_BODY not in text or 'CP29_NATIVE_SHIFTED_NIBBLES' in text:return text,[]
 return text.replace(head,head+code),['B2BC4C']

if __name__=='__main__':
 from pathlib import Path
 import sys
 p=Path(sys.argv[1]);s=p.read_text()
 if 'CP24_NATIVE_RAM_SPAN' in s:raise SystemExit('Native spans already applied; refusing a duplicate application.')
 s,sites=fuse(s);p.write_text(s);print('native byte spans',len(sites),','.join(sites))
