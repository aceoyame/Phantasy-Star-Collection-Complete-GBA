u32 aotf86(u32 entry){
 static const u32 xpc[] AOT_TAB={0x2E1A6u,0x2E1A8u,0x2E1ACu};
 u32 sr=C.sr, cyc=C.cycles, npc, i, rc=AOT_CONT; const u32 lim=aot_lim, hi=C.pc&0xFF000000u; 
 switch(entry){ case 0: goto B2E1A6; case 1: goto B2E1A8; default: npc=C.pc; goto out; }
B2E1A6: if((s32)(cyc-lim)>=0){ i=0; goto ex; }
{ cyc+=4;
u32 ea0=AA[0]; AA[0]+=4u;
cyc+=8; u32 r2=R32(ea0);
{ u32 r=ADDL(31u,r2,AD[0],0xFFFFFFFFu,0x80000000u,32,0,1,0); AD[0]=(AD[0]&~0xFFFFFFFFu)|r; } }
B2E1A8: if((s32)(cyc-lim)>=0){ i=1; goto ex; }
{ cyc+=4;
if(!0){ u32 w=(AD[1]-1u)&0xFFFFu; AD[1]=(AD[1]&0xFFFF0000u)|w; if(w!=0xFFFFu){ goto B2E1A6; } } }
i=2; goto ex;
ex: npc=hi|xpc[i];
out: C.sr=(u16)sr; C.cycles=cyc; C.pc=npc; return rc;
}
u32 aotf87(u32 entry){
 static const u32 xpc[] AOT_TAB={0x2E132u,0x2E134u,0x2E138u};
 u32 sr=C.sr, cyc=C.cycles, npc, i, rc=AOT_CONT; const u32 lim=aot_lim, hi=C.pc&0xFF000000u; 
 switch(entry){ case 0: goto B2E132; case 1: goto B2E134; default: npc=C.pc; goto out; }
B2E132: if((s32)(cyc-lim)>=0){ i=0; goto ex; }
{ cyc+=4;
u32 ea0=AA[0]; AA[0]+=4u;
cyc+=8; u32 r2=R32(ea0);
{ u32 r=ADDL(31u,r2,AD[0],0xFFFFFFFFu,0x80000000u,32,0,1,0); AD[0]=(AD[0]&~0xFFFFFFFFu)|r; } }
B2E134: if((s32)(cyc-lim)>=0){ i=1; goto ex; }
{ cyc+=4;
if(!0){ u32 w=(AD[1]-1u)&0xFFFFu; AD[1]=(AD[1]&0xFFFF0000u)|w; if(w!=0xFFFFu){ goto B2E132; } } }
i=2; goto ex;
ex: npc=hi|xpc[i];
out: C.sr=(u16)sr; C.cycles=cyc; C.pc=npc; return rc;
}
u32 aotf88(u32 entry){
 static const u32 xpc[] AOT_TAB={0x2E11Eu,0x2E120u,0x2E124u};
 u32 sr=C.sr, cyc=C.cycles, npc, i, rc=AOT_CONT; const u32 lim=aot_lim, hi=C.pc&0xFF000000u; 
 switch(entry){ case 0: goto B2E11E; case 1: goto B2E120; default: npc=C.pc; goto out; }
B2E11E: if((s32)(cyc-lim)>=0){ i=0; goto ex; }
{ cyc+=4;
u32 ea0=AA[0]; AA[0]+=4u;
cyc+=8; u32 r2=R32(ea0);
{ u32 r=ADDL(31u,r2,AD[0],0xFFFFFFFFu,0x80000000u,32,0,1,0); AD[0]=(AD[0]&~0xFFFFFFFFu)|r; } }
B2E120: if((s32)(cyc-lim)>=0){ i=1; goto ex; }
{ cyc+=4;
if(!0){ u32 w=(AD[1]-1u)&0xFFFFu; AD[1]=(AD[1]&0xFFFF0000u)|w; if(w!=0xFFFFu){ goto B2E11E; } } }
i=2; goto ex;
ex: npc=hi|xpc[i];
out: C.sr=(u16)sr; C.cycles=cyc; C.pc=npc; return rc;
}
u32 aotf89(u32 entry){
 static const u32 xpc[] AOT_TAB={0x2E05Eu,0x2E060u,0x2E064u};
 u32 sr=C.sr, cyc=C.cycles, npc, i, rc=AOT_CONT; const u32 lim=aot_lim, hi=C.pc&0xFF000000u; 
 switch(entry){ case 0: goto B2E05E; case 1: goto B2E060; default: npc=C.pc; goto out; }
B2E05E: if((s32)(cyc-lim)>=0){ i=0; goto ex; }
{ cyc+=4;
u32 ea0=AA[0]; AA[0]+=4u;
cyc+=8; u32 r2=R32(ea0);
{ u32 r=ADDL(31u,r2,AD[0],0xFFFFFFFFu,0x80000000u,32,0,1,0); AD[0]=(AD[0]&~0xFFFFFFFFu)|r; } }
B2E060: if((s32)(cyc-lim)>=0){ i=1; goto ex; }
{ cyc+=4;
if(!0){ u32 w=(AD[1]-1u)&0xFFFFu; AD[1]=(AD[1]&0xFFFF0000u)|w; if(w!=0xFFFFu){ goto B2E05E; } } }
i=2; goto ex;
ex: npc=hi|xpc[i];
out: C.sr=(u16)sr; C.cycles=cyc; C.pc=npc; return rc;
}
u32 aotf90(u32 entry){
 static const u32 xpc[] AOT_TAB={0x2E1A2u,0x2E1A4u,0x2E1A8u};
 u32 sr=C.sr, cyc=C.cycles, npc, i, rc=AOT_CONT; const u32 lim=aot_lim, hi=C.pc&0xFF000000u; 
 switch(entry){ case 0: goto B2E1A2; case 1: goto B2E1A4; default: npc=C.pc; goto out; }
B2E1A2: if((s32)(cyc-lim)>=0){ i=0; goto ex; }
{ cyc+=4;
u32 ea0=AA[0]; AA[0]+=4u;
cyc+=8; u32 r2=R32(ea0);
{ u32 r=ADDL(31u,r2,AD[0],0xFFFFFFFFu,0x80000000u,32,0,1,0); AD[0]=(AD[0]&~0xFFFFFFFFu)|r; } }
B2E1A4: if((s32)(cyc-lim)>=0){ i=1; goto ex; }
{ cyc+=4;
if(!0){ u32 w=(AD[1]-1u)&0xFFFFu; AD[1]=(AD[1]&0xFFFF0000u)|w; if(w!=0xFFFFu){ goto B2E1A2; } } }
i=2; goto ex;
ex: npc=hi|xpc[i];
out: C.sr=(u16)sr; C.cycles=cyc; C.pc=npc; return rc;
}
u32 aotf91(u32 entry){
 static const u32 xpc[] AOT_TAB={0x2D010u,0x2E4F8u,0x2E4FCu,0x2E506u,0x2E4FEu,0x2E502u,0x2E504u};
 u32 sr=C.sr, cyc=C.cycles, npc, i, rc=AOT_CONT; const u32 lim=aot_lim, hi=C.pc&0xFF000000u; 
 switch(entry){ case 0: goto B2D010; case 1: goto B2E4F8; case 2: goto B2E4FC; case 3: goto B2E4FE; case 4: goto B2E502; case 5: goto B2E504; default: npc=C.pc; goto out; }
B2D010: if((s32)(cyc-lim)>=0){ i=0; goto ex; }
{ cyc+=4;
cyc+=2;

goto B2E4F8; }
B2E4F8: if((s32)(cyc-lim)>=0){ i=1; goto ex; }
{ cyc+=4;
cyc+=4;
cyc+=4; u32 r2=RAM16(0xBB4Eu);
NZL(31u,r2,0xFFFFu,0x8000u); }
B2E4FC: if((s32)(cyc-lim)>=0){ i=2; goto ex; }
{ cyc+=4;
if(CC(sr,6)){ cyc+=2;  i=3; goto ex; } }
B2E4FE: if((s32)(cyc-lim)>=0){ i=4; goto ex; }
{ cyc+=4;
cyc+=4;
cyc+=4; u32 r2=RAM16(0xC00Cu);
NZL(31u,r2,0xFFFFu,0x8000u); }
B2E502: if((s32)(cyc-lim)>=0){ i=5; goto ex; }
{ cyc+=4;
if(CC(sr,7)){ cyc+=2;  i=3; goto ex; } }
B2E504: if((s32)(cyc-lim)>=0){ i=6; goto ex; }
{ cyc+=4;
{ u32 t=R32(AA[7]); AA[7]+=4; cyc+=8;
npc=t; goto out;
} }
ex: npc=hi|xpc[i];
out: C.sr=(u16)sr; C.cycles=cyc; C.pc=npc; return rc;
}
u32 aotf92(u32 entry){
 static const u32 xpc[] AOT_TAB={0x2E19Eu,0x2E1A0u,0x2E1A4u};
 u32 sr=C.sr, cyc=C.cycles, npc, i, rc=AOT_CONT; const u32 lim=aot_lim, hi=C.pc&0xFF000000u; 
 switch(entry){ case 0: goto B2E19E; case 1: goto B2E1A0; default: npc=C.pc; goto out; }
B2E19E: if((s32)(cyc-lim)>=0){ i=0; goto ex; }
{ cyc+=4;
u32 ea0=AA[0]; AA[0]+=4u;
cyc+=8; u32 r2=R32(ea0);
{ u32 r=ADDL(31u,r2,AD[0],0xFFFFFFFFu,0x80000000u,32,0,1,0); AD[0]=(AD[0]&~0xFFFFFFFFu)|r; } }
B2E1A0: if((s32)(cyc-lim)>=0){ i=1; goto ex; }
{ cyc+=4;
if(!0){ u32 w=(AD[1]-1u)&0xFFFFu; AD[1]=(AD[1]&0xFFFF0000u)|w; if(w!=0xFFFFu){ goto B2E19E; } } }
i=2; goto ex;
ex: npc=hi|xpc[i];
out: C.sr=(u16)sr; C.cycles=cyc; C.pc=npc; return rc;
}
u32 aotf93(u32 entry){
 static const u32 xpc[] AOT_TAB={0x3018u,0x301Au,0x301Cu,0x301Eu,0x3020u,0x3022u,0x3024u,0x3028u,0x302Au,0x302Cu,0x302Eu,0x3030u,0x3032u,0x3034u,0x3036u,0x303Au,0x303Cu};
 u32 sr=C.sr, cyc=C.cycles, npc, i, rc=AOT_CONT; const u32 lim=aot_lim, hi=C.pc&0xFF000000u; 
 switch(entry){ case 0: goto B3018; case 1: goto B301A; case 2: goto B301C; case 3: goto B301E; case 4: goto B3020; case 5: goto B3022; case 6: goto B3024; case 7: goto B3028; case 8: goto B302A; case 9: goto B302C; case 10: goto B302E; case 11: goto B3030; case 12: goto B3032; case 13: goto B3034; case 14: goto B3036; case 15: goto B303A; case 16: goto B303C; default: npc=C.pc; goto out; }
B3018: if((s32)(cyc-lim)>=0){ i=0; goto ex; }
{ cyc+=4;
{ u32 v=AD[0]; v=(v>>16)|(v<<16); AD[0]=v; NZL(0u,v,0xFFFFFFFFu,0x80000000u); } }
B301A: if((s32)(cyc-lim)>=0){ i=1; goto ex; }
{ cyc+=4;
u32 mv=(AD[1]&0xFFFFu);
AD[0]=(AD[0]&~0xFFFFu)|((mv)&0xFFFFu);
NZL(0u,mv,0xFFFFu,0x8000u); }
B301C: if((s32)(cyc-lim)>=0){ i=2; goto ex; }
{ cyc+=4;
AD[1]=0;
sr=(sr&~(FN_|FV_|FC_))|FZ_; }
B301E: if((s32)(cyc-lim)>=0){ i=3; goto ex; }
{ cyc+=4;
u32 mv=(AD[0]&0xFFFFu);
AD[1]=(AD[1]&~0xFFFFu)|((mv)&0xFFFFu);
NZL(0u,mv,0xFFFFu,0x8000u); }
B3020: if((s32)(cyc-lim)>=0){ i=4; goto ex; }
{ cyc+=4;
{ u32 r=aot_lshift_imm(&sr,&cyc,AD[1],2u,0xFFFFFFFFu,0x80000000u,1u); AD[1]=(AD[1]&~0xFFFFFFFFu)|(r&0xFFFFFFFFu); } }
B3022: if((s32)(cyc-lim)>=0){ i=5; goto ex; }
{ cyc+=4;
u32 mv=(AD[0]&0xFFFFu);
AD[1]=(AD[1]&~0xFFFFu)|((mv)&0xFFFFu);
NZL(0u,mv,0xFFFFu,0x8000u); }
B3024: if((s32)(cyc-lim)>=0){ i=6; goto ex; }
{ cyc+=4;
u32 r=(AD[1]&0xFFFFu)&0x3FFFu;
NZL(0u,r,0xFFFFu,0x8000u);
AD[1]=(AD[1]&~0xFFFFu)|((r)&0xFFFFu); }
B3028: if((s32)(cyc-lim)>=0){ i=7; goto ex; }
{ cyc+=4;
{ u32 v=AD[1]; v=(v>>16)|(v<<16); AD[1]=v; NZL(0u,v,0xFFFFFFFFu,0x80000000u); } }
B302A: if((s32)(cyc-lim)>=0){ i=8; goto ex; }
{ cyc+=4;
u32 mv=(AD[1]&0xFFFFFFFFu);
u32 ea1=AA[6];
cyc+=8; W32(ea1,mv);
NZL(0u,mv,0xFFFFFFFFu,0x80000000u); }
B302C: if((s32)(cyc-lim)>=0){ i=9; goto ex; }
{ cyc+=4;
{ u32 v=AD[0]; v=(v>>16)|(v<<16); AD[0]=v; NZL(0u,v,0xFFFFFFFFu,0x80000000u); } }
B302E: if((s32)(cyc-lim)>=0){ i=10; goto ex; }
{ cyc+=4;
u32 r=SUBL(0u,1u,(AD[0]&0xFFFFu),0xFFFFu,0x8000u,0,1,0);
AD[0]=(AD[0]&~0xFFFFu)|((r)&0xFFFFu); }
B3030: if((s32)(cyc-lim)>=0){ i=11; goto ex; }
{ cyc+=4;
{ u32 r=aot_lshift_imm(&sr,&cyc,AD[0],2u,0xFFFFu,0x8000u,0u); AD[0]=(AD[0]&~0xFFFFu)|(r&0xFFFFu); } }
B3032: if((s32)(cyc-lim)>=0){ i=12; goto ex; }
{ cyc+=4;
AA[6]-=4u; }
B3034: if((s32)(cyc-lim)>=0){ i=13; goto ex; }
{ cyc+=4;
u32 ea0=AA[6];
cyc+=8; u32 r2=R32(ea0);
u32 mv=r2;
u32 ea1=AA[0]; AA[0]+=4u;
cyc+=8; W32(ea1,mv);
NZL(31u,mv,0xFFFFFFFFu,0x80000000u); }
B3036: if((s32)(cyc-lim)>=0){ i=14; goto ex; }
{ cyc+=4;
if(!0){ u32 w=(AD[0]-1u)&0xFFFFu; AD[0]=(AD[0]&0xFFFF0000u)|w; if(w!=0xFFFFu){ goto B3034; } } }
B303A: if((s32)(cyc-lim)>=0){ i=15; goto ex; }
{ cyc+=4;
AA[6]+=4u; }
B303C: if((s32)(cyc-lim)>=0){ i=16; goto ex; }
{ cyc+=4;
{ u32 t=R32(AA[7]); AA[7]+=4; cyc+=8;
npc=t; goto out;
} }
ex: npc=hi|xpc[i];
out: C.sr=(u16)sr; C.cycles=cyc; C.pc=npc; return rc;
}
u32 aotf94(u32 entry){
 static const u32 xpc[] AOT_TAB={0x36E0u,0x36E2u,0x36F8u,0x36FCu,0x36FEu,0x3702u,0x3706u,0x3708u,0x370Au,0x370Cu,0x370Eu,0x3710u,0x3712u,0x3714u,0x3716u,0x371Au,0x372Au,0x372Cu,0x372Eu,0x3730u,0x3732u};
 u32 sr=C.sr, cyc=C.cycles, npc, i, rc=AOT_CONT; const u32 lim=aot_lim, hi=C.pc&0xFF000000u; 
 switch(entry){ case 0: goto B36E0; case 1: goto B36E2; case 2: goto B36F8; case 3: goto B36FC; case 4: goto B36FE; case 5: goto B3702; case 6: goto B3706; case 7: goto B3708; case 8: goto B370A; case 9: goto B370C; case 10: goto B370E; case 11: goto B3710; case 12: goto B3712; case 13: goto B3714; case 14: goto B3716; case 15: goto B371A; case 16: goto B372A; case 17: goto B372C; case 18: goto B372E; case 19: goto B3730; default: npc=C.pc; goto out; }
B36E0: if((s32)(cyc-lim)>=0){ i=0; goto ex; }
{ cyc+=4;
AD[0]=0x8u;
NZL(31u,0x8u,0xFFFFFFFFu,0x80000000u); }
B36E2: if((s32)(cyc-lim)>=0){ i=1; goto ex; }
{ cyc+=4;
cyc+=4;
AA[7]-=4; W32(AA[7],(hi|0x36E6u));
cyc+=8;
npc=0xFFFFFA9Au; goto out; }
B36F8: if((s32)(cyc-lim)>=0){ i=2; goto ex; }
{ cyc+=4;
u32 ea0=AA[1]+((u32)(s32)(s16)AD[1]+0x1u); cyc+=6;
cyc+=4; u32 r2=R8(ea0);
u32 mv=r2;
AD[1]=(AD[1]&~0xFFu)|((mv)&0xFFu);
NZL(0u,mv,0xFFu,0x80u); }
B36FC: if((s32)(cyc-lim)>=0){ i=3; goto ex; }
{ cyc+=4;
u32 mv=(AD[1]&0xFFFFu);
AD[0]=(AD[0]&~0xFFFFu)|((mv)&0xFFFFu);
NZL(0u,mv,0xFFFFu,0x8000u); }
B36FE: if((s32)(cyc-lim)>=0){ i=4; goto ex; }
{ cyc+=4;
u32 r=(AD[1]&0xFFFFu)&0xFu;
NZL(0u,r,0xFFFFu,0x8000u);
AD[1]=(AD[1]&~0xFFFFu)|((r)&0xFFFFu); }
B3702: if((s32)(cyc-lim)>=0){ i=5; goto ex; }
{ cyc+=4;
u32 r=(AD[0]&0xFFFFu)&0xF0u;
NZL(0u,r,0xFFFFu,0x8000u);
AD[0]=(AD[0]&~0xFFFFu)|((r)&0xFFFFu); }
B3706: if((s32)(cyc-lim)>=0){ i=6; goto ex; }
{ cyc+=4;
{ u32 r=aot_lshift_imm(&sr,&cyc,AD[0],4u,0xFFFFu,0x8000u,0u); AD[0]=(AD[0]&~0xFFFFu)|(r&0xFFFFu); } }
B3708: if((s32)(cyc-lim)>=0){ i=7; goto ex; }
{ cyc+=4;
{ u32 r=aot_lshift_imm(&sr,&cyc,AD[4],4u,0xFFFFFFFFu,0x80000000u,1u); AD[4]=(AD[4]&~0xFFFFFFFFu)|(r&0xFFFFFFFFu); } }
B370A: if((s32)(cyc-lim)>=0){ i=8; goto ex; }
{ cyc+=4;
u32 r=(AD[4]|(AD[1]&0xFFu))&0xFFu;
NZL(0u,r,0xFFu,0x80u);
AD[4]=(AD[4]&~0xFFu)|r; }
B370C: if((s32)(cyc-lim)>=0){ i=9; goto ex; }
{ cyc+=4;
u32 r=SUBL(31u,1u,(AD[3]&0xFFFFu),0xFFFFu,0x8000u,0,1,0);
AD[3]=(AD[3]&~0xFFFFu)|((r)&0xFFFFu); }
B370E: if((s32)(cyc-lim)>=0){ i=10; goto ex; }
{ cyc+=4;
if(CC(sr,6)){ cyc+=2;  goto B3716; } }
B3710: if((s32)(cyc-lim)>=0){ i=11; goto ex; }
{ cyc+=4;
u32 ea0=AA[3];
npc=ea0; goto out; }
B3712: if((s32)(cyc-lim)>=0){ i=12; goto ex; }
{ cyc+=4;
AD[4]=0x0u;
NZL(16u,0x0u,0xFFFFFFFFu,0x80000000u); }
B3714: if((s32)(cyc-lim)>=0){ i=13; goto ex; }
{ cyc+=4;
AD[3]=0x8u;
NZL(16u,0x8u,0xFFFFFFFFu,0x80000000u); }
B3716: if((s32)(cyc-lim)>=0){ i=14; goto ex; }
{ cyc+=4;
if(!0){ u32 w=(AD[0]-1u)&0xFFFFu; AD[0]=(AD[0]&0xFFFF0000u)|w; if(w!=0xFFFFu){ goto B3708; } } }
B371A: if((s32)(cyc-lim)>=0){ i=15; goto ex; }
{ cyc+=4;
cyc+=2;

goto B36E0; }
B372A: if((s32)(cyc-lim)>=0){ i=16; goto ex; }
{ cyc+=4;
u32 mv=(AD[4]&0xFFFFFFFFu);
u32 ea1=AA[4];
cyc+=8; W32(ea1,mv);
NZL(16u,mv,0xFFFFFFFFu,0x80000000u); }
B372C: if((s32)(cyc-lim)>=0){ i=17; goto ex; }
{ cyc+=4;
AA[5]-=1u; }
B372E: if((s32)(cyc-lim)>=0){ i=18; goto ex; }
{ cyc+=4;
u32 mv=(AA[5]&0xFFFFu);
AD[4]=(AD[4]&~0xFFFFu)|((mv)&0xFFFFu);
NZL(31u,mv,0xFFFFu,0x8000u); }
B3730: if((s32)(cyc-lim)>=0){ i=19; goto ex; }
{ cyc+=4;
if(CC(sr,6)){ cyc+=2;  goto B3712; } }
i=20; goto ex;
ex: npc=hi|xpc[i];
out: C.sr=(u16)sr; C.cycles=cyc; C.pc=npc; return rc;
}
u32 aotf95(u32 entry){
 static const u32 xpc[] AOT_TAB={0x37A0u,0x37A4u,0x37A6u,0x37AAu};
 u32 sr=C.sr, cyc=C.cycles, npc, i, rc=AOT_CONT; const u32 lim=aot_lim, hi=C.pc&0xFF000000u; 
 switch(entry){ case 0: goto B37A0; case 1: goto B37A4; case 2: goto B37A6; default: npc=C.pc; goto out; }
B37A0: if((s32)(cyc-lim)>=0){ i=0; goto ex; }
{ cyc+=4;
u32 mv=(AD[7]&0xFFFFu);
u32 ea1=AA[1]+((u32)(s32)(s16)AD[0]+0x0u); cyc+=6;
cyc+=4; W16(ea1,mv);
NZL(0u,mv,0xFFFFu,0x8000u); }
B37A4: if((s32)(cyc-lim)>=0){ i=1; goto ex; }
{ cyc+=4;
u32 r=ADDL(31u,2u,(AD[0]&0xFFFFu),0xFFFFu,0x8000u,16,0,1,0);
AD[0]=(AD[0]&~0xFFFFu)|((r)&0xFFFFu); }
B37A6: if((s32)(cyc-lim)>=0){ i=2; goto ex; }
{ cyc+=4;
if(!0){ u32 w=(AD[5]-1u)&0xFFFFu; AD[5]=(AD[5]&0xFFFF0000u)|w; if(w!=0xFFFFu){ goto B37A0; } } }
i=3; goto ex;
ex: npc=hi|xpc[i];
out: C.sr=(u16)sr; C.cycles=cyc; C.pc=npc; return rc;
}
u32 aotf96(u32 entry){
 static const u32 xpc[] AOT_TAB={0x38B2u,0x38B4u};
 u32 sr=C.sr, cyc=C.cycles, npc, i, rc=AOT_CONT; const u32 lim=aot_lim, hi=C.pc&0xFF000000u; 
 switch(entry){ case 0: goto B38B2; default: npc=C.pc; goto out; }
B38B2: if((s32)(cyc-lim)>=0){ i=0; goto ex; }
{ cyc+=4;
AD[6]=0x7u;
NZL(31u,0x7u,0xFFFFFFFFu,0x80000000u); }
i=1; goto ex;
ex: npc=hi|xpc[i];
out: C.sr=(u16)sr; C.cycles=cyc; C.pc=npc; return rc;
}
u32 aotf97(u32 entry){
 static const u32 xpc[] AOT_TAB={0x399Eu,0x39A0u,0x39C8u,0x39CAu,0x39CCu,0x39CEu,0x39D0u};
 u32 sr=C.sr, cyc=C.cycles, npc, i, rc=AOT_CONT; const u32 lim=aot_lim, hi=C.pc&0xFF000000u; 
 switch(entry){ case 0: goto B399E; case 1: goto B39A0; case 2: goto B39C8; case 3: goto B39CA; case 4: goto B39CC; case 5: goto B39CE; default: npc=C.pc; goto out; }
B399E: if((s32)(cyc-lim)>=0){ i=0; goto ex; }
{ cyc+=4;
AD[0]=0x7u;
NZL(31u,0x7u,0xFFFFFFFFu,0x80000000u); }
B39A0: if((s32)(cyc-lim)>=0){ i=1; goto ex; }
{ cyc+=4;
cyc+=4;
AA[7]-=4; W32(AA[7],(hi|0x39A4u));
cyc+=8;
npc=0xFFFFFA9Au; goto out; }
B39C8: if((s32)(cyc-lim)>=0){ i=2; goto ex; }
{ cyc+=4;
u32 r=((AD[4]&0xFFu)^AD[1])&0xFFu;
NZL(16u,r,0xFFu,0x80u);
AD[4]=(AD[4]&~0xFFu)|((r)&0xFFu); }
B39CA: if((s32)(cyc-lim)>=0){ i=3; goto ex; }
{ cyc+=4;
u32 mv=(AD[4]&0xFFu);
u32 ea1=AA[4]; AA[4]+=1u;
cyc+=4; W8(ea1,mv);
NZL(16u,mv,0xFFu,0x80u); }
B39CC: if((s32)(cyc-lim)>=0){ i=4; goto ex; }
{ cyc+=4;
CMPL(31u,(AA[3]&0xFFFFFFFFu),AA[4],0xFFFFFFFFu,0x80000000u); }
B39CE: if((s32)(cyc-lim)>=0){ i=5; goto ex; }
{ cyc+=4;
if(CC(sr,5)){ cyc+=2;  goto B399E; } }
i=6; goto ex;
ex: npc=hi|xpc[i];
out: C.sr=(u16)sr; C.cycles=cyc; C.pc=npc; return rc;
}
u32 aotf98(u32 entry){
 static const u32 xpc[] AOT_TAB={0x39A4u,0x39A8u,0x39BCu,0x39AAu};
 u32 sr=C.sr, cyc=C.cycles, npc, i, rc=AOT_CONT; const u32 lim=aot_lim, hi=C.pc&0xFF000000u; 
 switch(entry){ case 0: goto B39A4; case 1: goto B39A8; default: npc=C.pc; goto out; }
B39A4: if((s32)(cyc-lim)>=0){ i=0; goto ex; }
{ cyc+=4;
CMPL(31u,0x60u,(AD[1]&0xFFFFu),0xFFFFu,0x8000u); }
B39A8: if((s32)(cyc-lim)>=0){ i=1; goto ex; }
{ cyc+=4;
if(CC(sr,4)){ cyc+=2;  i=2; goto ex; } }
i=3; goto ex;
ex: npc=hi|xpc[i];
out: C.sr=(u16)sr; C.cycles=cyc; C.pc=npc; return rc;
}
u32 aotf99(u32 entry){
 static const u32 xpc[] AOT_TAB={0x3ADAu,0x3ADCu,0x3AE0u,0x3AE2u,0x3AF4u,0x3AF8u,0x3AA2u,0x3AFAu,0x3AFCu,0x3AFEu,0x3B00u,0x3B02u,0x3B04u,0x3B06u,0x3B08u,0x3B0Au,0x3B0Cu,0x3B0Eu,0x3B10u,0x3B12u,0x3B14u,0x3B16u,0x3B18u,0x3B1Au,0x3B1Cu,0x3B1Eu,0x3B20u,0x3B22u,0x3B28u,0x3B2Au,0x3B2Cu,0x3B2Eu,0x3B30u,0x3B32u,0x3B34u,0x3B3Au,0x3B3Cu,0x3B3Eu};
 u32 sr=C.sr, cyc=C.cycles, npc, i, rc=AOT_CONT; const u32 lim=aot_lim, hi=C.pc&0xFF000000u; 
 switch(entry){ case 0: goto B3ADA; case 1: goto B3ADC; case 2: goto B3AE0; case 3: goto B3AF4; case 4: goto B3AF8; case 5: goto B3AFA; case 6: goto B3AFC; case 7: goto B3AFE; case 8: goto B3B00; case 9: goto B3B02; case 10: goto B3B04; case 11: goto B3B06; case 12: goto B3B08; case 13: goto B3B0A; case 14: goto B3B0C; case 15: goto B3B0E; case 16: goto B3B10; case 17: goto B3B12; case 18: goto B3B14; case 19: goto B3B16; case 20: goto B3B18; case 21: goto B3B1A; case 22: goto B3B1C; case 23: goto B3B1E; case 24: goto B3B20; case 25: goto B3B28; case 26: goto B3B2A; case 27: goto B3B2C; case 28: goto B3B2E; case 29: goto B3B30; case 30: goto B3B32; case 31: goto B3B3A; case 32: goto B3B3C; case 33: goto B3B3E; default: npc=C.pc; goto out; }
B3ADA: if((s32)(cyc-lim)>=0){ i=0; goto ex; }
{ cyc+=4;
u32 mv=(AA[6]&0xFFFFu);
AD[0]=(AD[0]&~0xFFFFu)|((mv)&0xFFFFu);
NZL(0u,mv,0xFFFFu,0x8000u); }
B3ADC: if((s32)(cyc-lim)>=0){ i=1; goto ex; }
{ cyc+=4;
u32 r=SUBL(31u,0xE630u,(AD[0]&0xFFFFu),0xFFFFu,0x8000u,0,1,0);
AD[0]=(AD[0]&~0xFFFFu)|((r)&0xFFFFu); }
B3AE0: if((s32)(cyc-lim)>=0){ i=2; goto ex; }
{ cyc+=4;
if(CC(sr,6)){ cyc+=2;  goto B3AF4; } }
i=3; goto ex;
B3AF4: if((s32)(cyc-lim)>=0){ i=4; goto ex; }
{ cyc+=4;
CMPL(31u,0x400u,(AD[0]&0xFFFFu),0xFFFFu,0x8000u); }
B3AF8: if((s32)(cyc-lim)>=0){ i=5; goto ex; }
{ cyc+=4;
if(CC(sr,4)){ cyc+=2;  i=6; goto ex; } }
B3AFA: if((s32)(cyc-lim)>=0){ i=7; goto ex; }
{ cyc+=4;
AD[0]=0x1u;
NZL(16u,0x1u,0xFFFFFFFFu,0x80000000u); }
B3AFC: if((s32)(cyc-lim)>=0){ i=8; goto ex; }
{ cyc+=4;
AA[6]-=2u; u32 ea0=AA[6]; cyc+=2;
cyc+=4; u32 r2=R16(ea0);
AA[0]=(u32)(s32)(s16)r2; }
B3AFE: if((s32)(cyc-lim)>=0){ i=9; goto ex; }
{ cyc+=4;
AA[0]-=1u; u32 ea0=AA[0]; cyc+=2;
cyc+=4; u32 r2=R8(ea0);
CMPL(20u,r2,AD[0],0xFFu,0x80u); }
B3B00: if((s32)(cyc-lim)>=0){ i=10; goto ex; }
{ cyc+=4;
if(CC(sr,6)){ cyc+=2;  goto B3ADA; } }
B3B02: if((s32)(cyc-lim)>=0){ i=11; goto ex; }
{ cyc+=4;
AA[0]-=1u; u32 ea0=AA[0]; cyc+=2;
cyc+=4; u32 r2=R8(ea0);
CMPL(20u,r2,AD[0],0xFFu,0x80u); }
B3B04: if((s32)(cyc-lim)>=0){ i=12; goto ex; }
{ cyc+=4;
if(CC(sr,7)){ cyc+=2;  goto B3B02; } }
B3B06: if((s32)(cyc-lim)>=0){ i=13; goto ex; }
{ cyc+=4;
AA[0]+=1u; }
B3B08: if((s32)(cyc-lim)>=0){ i=14; goto ex; }
{ cyc+=4;
AA[1]=(u32)(s32)(s16)(AA[0]&0xFFFFu); }
B3B0A: if((s32)(cyc-lim)>=0){ i=15; goto ex; }
{ cyc+=4;
AA[2]=(u32)(s32)(s16)(AA[0]&0xFFFFu); }
B3B0C: if((s32)(cyc-lim)>=0){ i=16; goto ex; }
{ cyc+=4;
AA[1]-=(u32)(s32)(s16)(AD[5]&0xFFFFu); }
B3B0E: if((s32)(cyc-lim)>=0){ i=17; goto ex; }
{ cyc+=4;
AA[2]+=(u32)(s32)(s16)(AD[5]&0xFFFFu); }
B3B10: if((s32)(cyc-lim)>=0){ i=18; goto ex; }
{ cyc+=4;
AD[1]&=~0xFFFFu;
sr=(sr&~(FN_|FV_|FC_))|FZ_; }
B3B12: if((s32)(cyc-lim)>=0){ i=19; goto ex; }
{ cyc+=4;
AD[2]&=~0xFFFFu;
sr=(sr&~(FN_|FV_|FC_))|FZ_; }
B3B14: if((s32)(cyc-lim)>=0){ i=20; goto ex; }
{ cyc+=4;
u32 ea0=AA[0];
cyc+=4; u32 r2=R8(ea0);
CMPL(20u,r2,AD[0],0xFFu,0x80u); }
B3B16: if((s32)(cyc-lim)>=0){ i=21; goto ex; }
{ cyc+=4;
if(CC(sr,6)){ cyc+=2;  goto B3ADA; } }
B3B18: if((s32)(cyc-lim)>=0){ i=22; goto ex; }
{ cyc+=4;
u32 mv=(AD[7]&0xFFu);
u32 ea1=AA[0]; AA[0]+=1u;
cyc+=4; W8(ea1,mv);
NZL(16u,mv,0xFFu,0x80u); }
B3B1A: if((s32)(cyc-lim)>=0){ i=23; goto ex; }
{ cyc+=4;
NZL(20u,(AD[1]&0xFFu),0xFFu,0x80u); }
B3B1C: if((s32)(cyc-lim)>=0){ i=24; goto ex; }
{ cyc+=4;
if(CC(sr,6)){ cyc+=2;  goto B3B28; } }
B3B1E: if((s32)(cyc-lim)>=0){ i=25; goto ex; }
{ cyc+=4;
u32 ea0=AA[1]; AA[1]+=1u;
cyc+=4; u32 r2=R8(ea0);
CMPL(31u,r2,AD[0],0xFFu,0x80u); }
B3B20: if((s32)(cyc-lim)>=0){ i=26; goto ex; }
{ cyc+=4;
if(CC(sr,6)){ cyc+=2;  goto B3B2C; } }
i=27; goto ex;
B3B28: if((s32)(cyc-lim)>=0){ i=28; goto ex; }
{ cyc+=4;
u32 ea0=AA[1]; AA[1]+=1u;
cyc+=4; u32 r2=R8(ea0);
CMPL(20u,r2,AD[0],0xFFu,0x80u); }
B3B2A: if((s32)(cyc-lim)>=0){ i=29; goto ex; }
{ cyc+=4;
AD[1]=(AD[1]&~0xFFu)|(((CC(sr,7)?0xFFu:0u))&0xFFu); }
B3B2C: if((s32)(cyc-lim)>=0){ i=30; goto ex; }
{ cyc+=4;
NZL(20u,(AD[2]&0xFFu),0xFFu,0x80u); }
B3B2E: if((s32)(cyc-lim)>=0){ i=31; goto ex; }
{ cyc+=4;
if(CC(sr,6)){ cyc+=2;  goto B3B3A; } }
B3B30: if((s32)(cyc-lim)>=0){ i=32; goto ex; }
{ cyc+=4;
u32 ea0=AA[2]; AA[2]+=1u;
cyc+=4; u32 r2=R8(ea0);
CMPL(31u,r2,AD[0],0xFFu,0x80u); }
B3B32: if((s32)(cyc-lim)>=0){ i=33; goto ex; }
{ cyc+=4;
if(CC(sr,6)){ cyc+=2;  goto B3B14; } }
i=34; goto ex;
B3B3A: if((s32)(cyc-lim)>=0){ i=35; goto ex; }
{ cyc+=4;
u32 ea0=AA[2]; AA[2]+=1u;
cyc+=4; u32 r2=R8(ea0);
CMPL(20u,r2,AD[0],0xFFu,0x80u); }
B3B3C: if((s32)(cyc-lim)>=0){ i=36; goto ex; }
{ cyc+=4;
AD[2]=(AD[2]&~0xFFu)|(((CC(sr,7)?0xFFu:0u))&0xFFu); }
B3B3E: if((s32)(cyc-lim)>=0){ i=37; goto ex; }
{ cyc+=4;
cyc+=2;

goto B3B14; }
ex: npc=hi|xpc[i];
out: C.sr=(u16)sr; C.cycles=cyc; C.pc=npc; return rc;
}
u32 aotf100(u32 entry){
 static const u32 xpc[] AOT_TAB={0x3FE2u,0x3FE6u,0x3FEAu,0x3FEEu,0x3FF2u,0x3FF6u,0x3FFAu,0x3FFEu,0x4000u,0x4012u,0x4014u,0x4016u,0x4018u,0x401Cu,0x401Eu,0x4078u,0x407Au,0x407Eu,0x4080u,0x4084u,0x4086u,0x408Au,0x4090u,0x4092u,0x4094u,0x4096u,0x4098u,0x409Au,0x409Cu,0x409Eu,0x40A0u,0x40A4u,0x40A8u,0x40AAu,0x40B6u,0x40B8u,0x40BAu,0x40BCu,0x40BEu,0x40C2u,0x40C4u,0x40C8u,0x40CAu,0x40D8u,0x40DCu,0x40DEu,0x40E2u,0x40E6u,0x40EAu,0x40EEu};
 u32 sr=C.sr, cyc=C.cycles, npc, i, rc=AOT_CONT; const u32 lim=aot_lim, hi=C.pc&0xFF000000u; 
 switch(entry){ case 0: goto B3FE2; case 1: goto B3FE6; case 2: goto B3FEA; case 3: goto B3FEE; case 4: goto B3FF2; case 5: goto B3FF6; case 6: goto B3FFA; case 7: goto B3FFE; case 8: goto B4012; case 9: goto B4014; case 10: goto B4016; case 11: goto B4018; case 12: goto B401C; case 13: goto B4078; case 14: goto B407A; case 15: goto B407E; case 16: goto B4080; case 17: goto B4084; case 18: goto B4086; case 19: goto B408A; case 20: goto B4090; case 21: goto B4092; case 22: goto B4094; case 23: goto B4096; case 24: goto B4098; case 25: goto B409A; case 26: goto B409C; case 27: goto B409E; case 28: goto B40A0; case 29: goto B40A4; case 30: goto B40A8; case 31: goto B40B6; case 32: goto B40B8; case 33: goto B40BA; case 34: goto B40BC; case 35: goto B40BE; case 36: goto B40C2; case 37: goto B40C4; case 38: goto B40C8; case 39: goto B40D8; case 40: goto B40DC; case 41: goto B40E2; case 42: goto B40E6; case 43: goto B40EA; case 44: goto B40EE; default: npc=C.pc; goto out; }
B3FE2: if((s32)(cyc-lim)>=0){ i=0; goto ex; }
{ cyc+=4;
{ u32 a=AA[7];
u32 native_n=m68k_movem_ram(a,0x7CC0u,2u); if(native_n){a-=4u*native_n;cyc+=8u*native_n;}else{
a-=4; W32(a,AA[1]); cyc+=8;
a-=4; W32(a,AA[0]); cyc+=8;
a-=4; W32(a,AD[5]); cyc+=8;
a-=4; W32(a,AD[4]); cyc+=8;
a-=4; W32(a,AD[3]); cyc+=8;
a-=4; W32(a,AD[2]); cyc+=8;
a-=4; W32(a,AD[1]); cyc+=8;
}
AA[7]=a; } }
B3FE6: if((s32)(cyc-lim)>=0){ i=1; goto ex; }
{ cyc+=4;
cyc+=4;
u32 mv=0xFFFFu;
AD[0]=(AD[0]&~0xFFFFu)|((mv)&0xFFFFu);
NZL(16u,mv,0xFFFFu,0x8000u); }
B3FEA: if((s32)(cyc-lim)>=0){ i=2; goto ex; }
{ cyc+=4;
cyc+=4;
cyc+=8; u32 r2=(((u32)RAM16(0xFFB0u)<<16)|RAM16(0xFFB2u));
NZL(31u,r2,0xFFFFFFFFu,0x80000000u); }
B3FEE: if((s32)(cyc-lim)>=0){ i=3; goto ex; }
{ cyc+=4;
if(CC(sr,7)){ cyc+=2;  goto B40EA; } }
B3FF2: if((s32)(cyc-lim)>=0){ i=4; goto ex; }
{ cyc+=4;
cyc+=4;
cyc+=8; u32 r2=(((u32)RAM16(0xFFB0u)<<16)|RAM16(0xFFB2u));
AA[0]=r2; }
B3FF6: if((s32)(cyc-lim)>=0){ i=5; goto ex; }
{ cyc+=4;
cyc+=4;
cyc+=4; u32 r2=RAM16(0xFFB4u);
u32 mv=r2;
AD[2]=(AD[2]&~0xFFFFu)|((mv)&0xFFFFu);
NZL(16u,mv,0xFFFFu,0x8000u); }
B3FFA: if((s32)(cyc-lim)>=0){ i=6; goto ex; }
{ cyc+=4;
cyc+=4;
cyc+=4; u32 r2=RAM16(0xFFB6u);
NZL(31u,r2,0xFFFFu,0x8000u); }
B3FFE: if((s32)(cyc-lim)>=0){ i=7; goto ex; }
{ cyc+=4;
if(CC(sr,6)){ cyc+=2;  goto B4012; } }
i=8; goto ex;
B4012: if((s32)(cyc-lim)>=0){ i=9; goto ex; }
{ cyc+=4;
u32 ea0=AA[0];
cyc+=8; u32 r2=R32(ea0);
u32 mv=r2;
AD[3]=mv;
NZL(16u,mv,0xFFFFFFFFu,0x80000000u); }
B4014: if((s32)(cyc-lim)>=0){ i=10; goto ex; }
{ cyc+=4;
{ u32 r=f_shift(&sr,&cyc,1,0,AD[3],(int)(AD[2]&63u),0xFFFFFFFFu,0x80000000u); AD[3]=(AD[3]&~0xFFFFFFFFu)|(r&0xFFFFFFFFu); } }
B4016: if((s32)(cyc-lim)>=0){ i=11; goto ex; }
{ cyc+=4;
u32 mv=(AD[3]&0xFFFFu);
AD[0]=(AD[0]&~0xFFFFu)|((mv)&0xFFFFu);
NZL(16u,mv,0xFFFFu,0x8000u); }
B4018: if((s32)(cyc-lim)>=0){ i=12; goto ex; }
{ cyc+=4;
CMPL(31u,0xF000u,(AD[0]&0xFFFFu),0xFFFFu,0x8000u); }
B401C: if((s32)(cyc-lim)>=0){ i=13; goto ex; }
{ cyc+=4;
if(CC(sr,5)){ cyc+=2;  goto B4078; } }
i=14; goto ex;
B4078: if((s32)(cyc-lim)>=0){ i=15; goto ex; }
{ cyc+=4;
AD[4]=0x0u;
NZL(0u,0x0u,0xFFFFFFFFu,0x80000000u); }
B407A: if((s32)(cyc-lim)>=0){ i=16; goto ex; }
{ cyc+=4;
u32 ea0=(hi|0x407Cu)+((u32)(s32)(s16)AD[4]+0x76u); cyc+=6;
cyc+=4; u32 r2=R16(ea0);
CMPL(1u,r2,AD[0],0xFFFFu,0x8000u); }
B407E: if((s32)(cyc-lim)>=0){ i=17; goto ex; }
{ cyc+=4;
if(CC(sr,4)){ cyc+=2;  goto B40B6; } }
B4080: if((s32)(cyc-lim)>=0){ i=18; goto ex; }
{ cyc+=4;
u32 ea0=(hi|0x4082u)+((u32)(s32)(s16)AD[4]+0x6Eu); cyc+=6;
cyc+=4; u32 r2=R16(ea0);
{ u32 r=SUBL(0u,r2,AD[0],0xFFFFu,0x8000u,0,1,0); AD[0]=(AD[0]&~0xFFFFu)|r; } }
B4084: if((s32)(cyc-lim)>=0){ i=19; goto ex; }
{ cyc+=4;
AD[1]=0;
sr=(sr&~(FN_|FV_|FC_))|FZ_; }
B4086: if((s32)(cyc-lim)>=0){ i=20; goto ex; }
{ cyc+=4;
u32 ea0=(hi|0x4088u)+((u32)(s32)(s16)AD[4]+0x76u); cyc+=6;
cyc+=4; u32 r2=R16(ea0);
u32 mv=r2;
AD[1]=(AD[1]&~0xFFFFu)|((mv)&0xFFFFu);
NZL(0u,mv,0xFFFFu,0x8000u); }
B408A: if((s32)(cyc-lim)>=0){ i=21; goto ex; }
{ cyc+=4;
u32 r=ADDL(0u,0x46E0u,(AD[1]&0xFFFFFFFFu),0xFFFFFFFFu,0x80000000u,32,0,1,0);
AD[1]=r; }
B4090: if((s32)(cyc-lim)>=0){ i=22; goto ex; }
{ cyc+=4;
AA[1]=(AD[1]&0xFFFFFFFFu); }
B4092: if((s32)(cyc-lim)>=0){ i=23; goto ex; }
{ cyc+=4;
{ u32 r=aot_lshift_imm(&sr,&cyc,AD[4],1u,0xFFFFu,0x8000u,0u); AD[4]=(AD[4]&~0xFFFFu)|(r&0xFFFFu); } }
B4094: if((s32)(cyc-lim)>=0){ i=24; goto ex; }
{ cyc+=4;
u32 mv=(AD[4]&0xFFFFu);
AD[5]=(AD[5]&~0xFFFFu)|((mv)&0xFFFFu);
NZL(0u,mv,0xFFFFu,0x8000u); }
B4096: if((s32)(cyc-lim)>=0){ i=25; goto ex; }
{ cyc+=4;
u32 r=ADDL(0u,5u,(AD[5]&0xFFFFu),0xFFFFu,0x8000u,16,0,1,0);
AD[5]=(AD[5]&~0xFFFFu)|((r)&0xFFFFu); }
B4098: if((s32)(cyc-lim)>=0){ i=26; goto ex; }
{ cyc+=4;
AD[1]=0xBu;
NZL(0u,0xBu,0xFFFFFFFFu,0x80000000u); }
B409A: if((s32)(cyc-lim)>=0){ i=27; goto ex; }
{ cyc+=4;
{ u32 r=SUBL(16u,(AD[4]&0xFFFFu),AD[1],0xFFFFu,0x8000u,0,1,0); AD[1]=(AD[1]&~0xFFFFu)|r; } }
B409C: if((s32)(cyc-lim)>=0){ i=28; goto ex; }
{ cyc+=4;
{ u32 r=f_shift(&sr,&cyc,1,0,AD[0],(int)(AD[1]&63u),0xFFFFu,0x8000u); AD[0]=(AD[0]&~0xFFFFu)|(r&0xFFFFu); } }
B409E: if((s32)(cyc-lim)>=0){ i=29; goto ex; }
{ cyc+=4;
{ u32 r=ADDL(16u,(AD[0]&0xFFFFu),AD[0],0xFFFFu,0x8000u,16,0,1,0); AD[0]=(AD[0]&~0xFFFFu)|r; } }
B40A0: if((s32)(cyc-lim)>=0){ i=30; goto ex; }
{ cyc+=4;
u32 ea0=AA[1]+((u32)(s32)(s16)AD[0]+0x0u); cyc+=6;
cyc+=4; u32 r2=R16(ea0);
u32 mv=r2;
AD[0]=(AD[0]&~0xFFFFu)|((mv)&0xFFFFu);
NZL(16u,mv,0xFFFFu,0x8000u); }
B40A4: if((s32)(cyc-lim)>=0){ i=31; goto ex; }
{ cyc+=4;
CMPL(31u,0xFFE0u,(AD[0]&0xFFFFu),0xFFFFu,0x8000u); }
B40A8: if((s32)(cyc-lim)>=0){ i=32; goto ex; }
{ cyc+=4;
if(CC(sr,6)){ cyc+=2;  goto B40BA; } }
i=33; goto ex;
B40B6: if((s32)(cyc-lim)>=0){ i=34; goto ex; }
{ cyc+=4;
u32 r=ADDL(0u,2u,(AD[4]&0xFFFFu),0xFFFFu,0x8000u,16,0,1,0);
AD[4]=(AD[4]&~0xFFFFu)|((r)&0xFFFFu); }
B40B8: if((s32)(cyc-lim)>=0){ i=35; goto ex; }
{ cyc+=4;
cyc+=2;

goto B407A; }
B40BA: if((s32)(cyc-lim)>=0){ i=36; goto ex; }
{ cyc+=4;
{ u32 r=SUBL(21u,(AD[5]&0xFFFFu),AD[2],0xFFFFu,0x8000u,0,1,0); AD[2]=(AD[2]&~0xFFFFu)|r; } }
B40BC: if((s32)(cyc-lim)>=0){ i=37; goto ex; }
{ cyc+=4;
if(CC(sr,2)){ cyc+=2;  goto B40C4; } }
B40BE: if((s32)(cyc-lim)>=0){ i=38; goto ex; }
{ cyc+=4;
u32 r=ADDL(16u,0x10u,(AD[2]&0xFFFFu),0xFFFFu,0x8000u,16,0,1,0);
AD[2]=(AD[2]&~0xFFFFu)|((r)&0xFFFFu); }
B40C2: if((s32)(cyc-lim)>=0){ i=39; goto ex; }
{ cyc+=4;
AA[0]+=2u; }
B40C4: if((s32)(cyc-lim)>=0){ i=40; goto ex; }
{ cyc+=4;
CMPL(31u,0xFFF4u,(AD[0]&0xFFFFu),0xFFFFu,0x8000u); }
B40C8: if((s32)(cyc-lim)>=0){ i=41; goto ex; }
{ cyc+=4;
if(CC(sr,6)){ cyc+=2;  goto B40D8; } }
i=42; goto ex;
B40D8: if((s32)(cyc-lim)>=0){ i=43; goto ex; }
{ cyc+=4;
CMPL(31u,0xFFFFu,(AD[0]&0xFFFFu),0xFFFFu,0x8000u); }
B40DC: if((s32)(cyc-lim)>=0){ i=44; goto ex; }
{ cyc+=4;
if(CC(sr,6)){ cyc+=2;  goto B40E2; } }
i=45; goto ex;
B40E2: if((s32)(cyc-lim)>=0){ i=46; goto ex; }
{ cyc+=4;
u32 mv=(AA[0]&0xFFFFFFFFu);
cyc+=4;
cyc+=8; { u32 _w=(mv); RAM16(0xFFB0u)=(u16)(_w>>16); RAM16(0xFFB2u)=(u16)_w; };
NZL(16u,mv,0xFFFFFFFFu,0x80000000u); }
B40E6: if((s32)(cyc-lim)>=0){ i=47; goto ex; }
{ cyc+=4;
u32 mv=(AD[2]&0xFFFFu);
cyc+=4;
cyc+=4; RAM16(0xFFB4u)=(u16)(mv);;
NZL(31u,mv,0xFFFFu,0x8000u); }
B40EA: if((s32)(cyc-lim)>=0){ i=48; goto ex; }
{ cyc+=4;
{ u32 a=AA[7];
u32 native_n=m68k_movem_ram(a,0x33Eu,1u); if(native_n){a+=4u*native_n;cyc+=8u*native_n;}else{
AD[1]=R32(a); a+=4; cyc+=8;
AD[2]=R32(a); a+=4; cyc+=8;
AD[3]=R32(a); a+=4; cyc+=8;
AD[4]=R32(a); a+=4; cyc+=8;
AD[5]=R32(a); a+=4; cyc+=8;
AA[0]=R32(a); a+=4; cyc+=8;
AA[1]=R32(a); a+=4; cyc+=8;
}
AA[7]=a;
} }
B40EE: if((s32)(cyc-lim)>=0){ i=49; goto ex; }
{ cyc+=4;
{ u32 t=R32(AA[7]); AA[7]+=4; cyc+=8;
npc=t; goto out;
} }
ex: npc=hi|xpc[i];
out: C.sr=(u16)sr; C.cycles=cyc; C.pc=npc; return rc;
}
u32 aotf101(u32 entry){
 static const u32 xpc[] AOT_TAB={0x419Cu,0x41A0u,0x41A6u,0x41AAu,0x41DAu,0x41ACu,0x41B0u,0x41B2u,0x41B8u,0x41BCu,0x41BEu,0x41C4u,0x41C8u,0x41CAu,0x41CEu,0x41D2u,0x41D4u,0x41DEu,0x41E0u,0x41E4u,0x41E6u,0x41ECu};
 u32 sr=C.sr, cyc=C.cycles, npc, i, rc=AOT_CONT; const u32 lim=aot_lim, hi=C.pc&0xFF000000u; 
 switch(entry){ case 0: goto B419C; case 1: goto B41A0; case 2: goto B41A6; case 3: goto B41AA; case 4: goto B41AC; case 5: goto B41B0; case 6: goto B41B8; case 7: goto B41BC; case 8: goto B41C4; case 9: goto B41C8; case 10: goto B41CE; case 11: goto B41D2; case 12: goto B41DE; case 13: goto B41E0; case 14: goto B41E4; case 15: goto B41EC; default: npc=C.pc; goto out; }
B419C: if((s32)(cyc-lim)>=0){ i=0; goto ex; }
{ cyc+=4;
{ u32 a=AA[7];
u32 native_n=m68k_movem_ram(a,0x3F0Cu,2u); if(native_n){a-=4u*native_n;cyc+=8u*native_n;}else{
a-=4; W32(a,AA[5]); cyc+=8;
a-=4; W32(a,AA[4]); cyc+=8;
a-=4; W32(a,AD[7]); cyc+=8;
a-=4; W32(a,AD[6]); cyc+=8;
a-=4; W32(a,AD[5]); cyc+=8;
a-=4; W32(a,AD[4]); cyc+=8;
a-=4; W32(a,AD[3]); cyc+=8;
a-=4; W32(a,AD[2]); cyc+=8;
}
AA[7]=a; } }
B41A0: if((s32)(cyc-lim)>=0){ i=1; goto ex; }
{ cyc+=4;
cyc+=8;
AA[6]=0xC00004u; }
B41A6: if((s32)(cyc-lim)>=0){ i=2; goto ex; }
{ cyc+=4;
CMPL(31u,0xFFF4u,(AD[0]&0xFFFFu),0xFFFFu,0x8000u); }
B41AA: if((s32)(cyc-lim)>=0){ i=3; goto ex; }
{ cyc+=4;
if(CC(sr,7)){ cyc+=2;  i=4; goto ex; } }
B41AC: if((s32)(cyc-lim)>=0){ i=5; goto ex; }
{ cyc+=4;
CMPL(31u,0xFFFEu,(AD[0]&0xFFFFu),0xFFFFu,0x8000u); }
B41B0: if((s32)(cyc-lim)>=0){ i=6; goto ex; }
{ cyc+=4;
if(CC(sr,6)){ cyc+=2;  goto B41B8; } }
i=7; goto ex;
B41B8: if((s32)(cyc-lim)>=0){ i=8; goto ex; }
{ cyc+=4;
CMPL(31u,0xFFFDu,(AD[0]&0xFFFFu),0xFFFFu,0x8000u); }
B41BC: if((s32)(cyc-lim)>=0){ i=9; goto ex; }
{ cyc+=4;
if(CC(sr,6)){ cyc+=2;  goto B41C4; } }
i=10; goto ex;
B41C4: if((s32)(cyc-lim)>=0){ i=11; goto ex; }
{ cyc+=4;
CMPL(31u,0xFFFBu,(AD[0]&0xFFFFu),0xFFFFu,0x8000u); }
B41C8: if((s32)(cyc-lim)>=0){ i=12; goto ex; }
{ cyc+=4;
if(CC(sr,6)){ cyc+=2;  goto B41CE; } }
i=13; goto ex;
B41CE: if((s32)(cyc-lim)>=0){ i=14; goto ex; }
{ cyc+=4;
CMPL(31u,0xFFFCu,(AD[0]&0xFFFFu),0xFFFFu,0x8000u); }
B41D2: if((s32)(cyc-lim)>=0){ i=15; goto ex; }
{ cyc+=4;
if(CC(sr,6)){ cyc+=2;  goto B41DE; } }
i=16; goto ex;
B41DE: if((s32)(cyc-lim)>=0){ i=17; goto ex; }
{ cyc+=4;
u32 mv=(AD[0]&0xFFFFu);
AD[1]=(AD[1]&~0xFFFFu)|((mv)&0xFFFFu);
NZL(0u,mv,0xFFFFu,0x8000u); }
B41E0: if((s32)(cyc-lim)>=0){ i=18; goto ex; }
{ cyc+=4;
u32 r=SUBL(31u,0xFFE0u,(AD[1]&0xFFFFu),0xFFFFu,0x8000u,0,1,0);
AD[1]=(AD[1]&~0xFFFFu)|((r)&0xFFFFu); }
B41E4: if((s32)(cyc-lim)>=0){ i=19; goto ex; }
{ cyc+=4;
if(CC(sr,5)){ cyc+=2;  goto B41EC; } }
i=20; goto ex;
B41EC: if((s32)(cyc-lim)>=0){ i=21; goto ex; }
{ cyc+=4;
cyc+=4;
AA[7]-=4; W32(AA[7],(hi|0x41F0u));
cyc+=8;
npc=0xFFFFFB30u; goto out; }
ex: npc=hi|xpc[i];
out: C.sr=(u16)sr; C.cycles=cyc; C.pc=npc; return rc;
}
u32 aotf102(u32 entry){
 static const u32 xpc[] AOT_TAB={0x41F0u,0x41F2u,0x41F6u,0x41F8u,0x41FAu,0x41FCu,0x4200u,0x4204u,0x4206u,0x420Au,0x420Eu,0x4212u,0x4216u,0x421Au,0x421Eu,0x4220u,0x4222u,0x4224u,0x4226u,0x422Au,0x422Eu,0x4232u,0x4236u,0x4238u,0x423Cu,0x423Eu,0x4240u,0x4242u,0x4244u,0x4246u,0x4248u,0x424Au,0x424Cu,0x424Eu,0x4250u,0x4252u,0x4256u,0x4258u,0x425Au,0x425Cu,0x425Eu,0x4262u,0x4266u,0x4268u,0x426Au,0x426Eu,0x4270u,0x4272u,0x4276u,0x4278u,0x427Au,0x427Cu,0x427Eu,0x4280u,0x4284u,0x4288u,0x428Au,0x428Eu,0x4292u,0x4296u,0x4298u,0x429Au,0x429Cu,0x42A0u,0x42A2u,0x42A4u,0x42A8u,0x42AAu,0x42AEu,0x42B0u,0x42B4u,0x42B8u,0x42BCu,0x42BEu,0x42C2u,0x42C4u,0x42E6u,0x42E8u,0x4318u,0x42EAu};
 u32 sr=C.sr, cyc=C.cycles, npc, i, rc=AOT_CONT; const u32 lim=aot_lim, hi=C.pc&0xFF000000u; 
 switch(entry){ case 0: goto B41F0; case 1: goto B41F2; case 2: goto B41F6; case 3: goto B41F8; case 4: goto B41FA; case 5: goto B41FC; case 6: goto B4200; case 7: goto B4204; case 8: goto B420A; case 9: goto B420E; case 10: goto B4212; case 11: goto B4216; case 12: goto B421A; case 13: goto B421E; case 14: goto B4220; case 15: goto B4222; case 16: goto B4224; case 17: goto B4226; case 18: goto B422A; case 19: goto B422E; case 20: goto B4232; case 21: goto B4236; case 22: goto B4238; case 23: goto B423C; case 24: goto B423E; case 25: goto B4240; case 26: goto B4242; case 27: goto B4244; case 28: goto B4246; case 29: goto B4248; case 30: goto B424A; case 31: goto B424C; case 32: goto B424E; case 33: goto B4250; case 34: goto B4252; case 35: goto B4256; case 36: goto B4258; case 37: goto B425A; case 38: goto B425C; case 39: goto B425E; case 40: goto B4262; case 41: goto B4266; case 42: goto B4268; case 43: goto B426A; case 44: goto B426E; case 45: goto B4270; case 46: goto B4272; case 47: goto B4276; case 48: goto B4278; case 49: goto B427A; case 50: goto B427C; case 51: goto B427E; case 52: goto B4280; case 53: goto B4284; case 54: goto B4288; case 55: goto B428A; case 56: goto B428E; case 57: goto B4292; case 58: goto B4296; case 59: goto B4298; case 60: goto B429A; case 61: goto B429C; case 62: goto B42A0; case 63: goto B42A2; case 64: goto B42A4; case 65: goto B42A8; case 66: goto B42AA; case 67: goto B42AE; case 68: goto B42B0; case 69: goto B42B4; case 70: goto B42B8; case 71: goto B42BC; case 72: goto B42BE; case 73: goto B42C2; case 74: goto B42E6; case 75: goto B42E8; default: npc=C.pc; goto out; }
B41F0: if((s32)(cyc-lim)>=0){ i=0; goto ex; }
{ cyc+=4;
u32 mv=(AD[0]&0xFFFFu);
AD[2]=(AD[2]&~0xFFFFu)|((mv)&0xFFFFu);
NZL(0u,mv,0xFFFFu,0x8000u); }
B41F2: if((s32)(cyc-lim)>=0){ i=1; goto ex; }
{ cyc+=4;
CMPL(4u,0xAu,(AD[0]&0xFFFFu),0xFFFFu,0x8000u); }
B41F6: if((s32)(cyc-lim)>=0){ i=2; goto ex; }
{ cyc+=4;
AD[1]=(AD[1]&~0xFFu)|(((CC(sr,7)?0xFFu:0u))&0xFFu); }
B41F8: if((s32)(cyc-lim)>=0){ i=3; goto ex; }
{ cyc+=4;
{ u32 v=(u32)(s32)(s8)AD[1]; AD[1]=(AD[1]&0xFFFF0000u)|(v&0xFFFFu); NZL(0u,v,0xFFFFu,0x8000u); } }
B41FA: if((s32)(cyc-lim)>=0){ i=4; goto ex; }
{ cyc+=4;
u32 mv=(AD[1]&0xFFFFu);
AA[7]-=2u; u32 ea1=AA[7]; cyc+=2;
cyc+=4; W16(ea1,mv);
NZL(0u,mv,0xFFFFu,0x8000u); }
B41FC: if((s32)(cyc-lim)>=0){ i=5; goto ex; }
{ cyc+=4;
u32 ea0=AA[2]+0x14u; cyc+=4;
cyc+=4; u32 r2=R16(ea0);
{ u32 r=ADDL(16u,r2,AD[0],0xFFFFu,0x8000u,16,0,1,0); AD[0]=(AD[0]&~0xFFFFu)|r; } }
B4200: if((s32)(cyc-lim)>=0){ i=6; goto ex; }
{ cyc+=4;
u32 ea0=AA[2]+0xCu; cyc+=4;
cyc+=4; u32 r2=R16(ea0);
CMPL(31u,r2,AD[0],0xFFFFu,0x8000u); }
B4204: if((s32)(cyc-lim)>=0){ i=7; goto ex; }
{ cyc+=4;
if(CC(sr,3)){ cyc+=2;  goto B420A; } }
i=8; goto ex;
B420A: if((s32)(cyc-lim)>=0){ i=9; goto ex; }
{ cyc+=4;
cyc+=4;
AA[1]=0xFFFFE630u; }
B420E: if((s32)(cyc-lim)>=0){ i=10; goto ex; }
{ cyc+=4;
u32 ea0=AA[2]+0x10u; cyc+=4;
cyc+=4; u32 r2=R16(ea0);
u32 mv=r2;
AD[7]=(AD[7]&~0xFFFFu)|((mv)&0xFFFFu);
NZL(0u,mv,0xFFFFu,0x8000u); }
B4212: if((s32)(cyc-lim)>=0){ i=11; goto ex; }
{ cyc+=4;
cyc+=4;
{ u32 r=(AD[7]&0xFFFFu)*((0x11u)&0xFFFFu);
AD[7]=r; NZL(0u,r,0xFFFFFFFFu,0x80000000u); cyc+=50; } }
B4216: if((s32)(cyc-lim)>=0){ i=12; goto ex; }
{ cyc+=4;
u32 ea0=AA[2]+0x14u; cyc+=4;
cyc+=4; u32 r2=R16(ea0);
u32 mv=r2;
AD[0]=(AD[0]&~0xFFFFu)|((mv)&0xFFFFu);
NZL(0u,mv,0xFFFFu,0x8000u); }
B421A: if((s32)(cyc-lim)>=0){ i=13; goto ex; }
{ cyc+=4;
u32 r=(AD[0]&0xFFFFu)&0x6u;
NZL(0u,r,0xFFFFu,0x8000u);
AD[0]=(AD[0]&~0xFFFFu)|((r)&0xFFFFu); }
B421E: if((s32)(cyc-lim)>=0){ i=14; goto ex; }
{ cyc+=4;
{ u32 r=aot_lshift_imm(&sr,&cyc,AD[0],1u,0xFFFFu,0x8000u,0u); AD[0]=(AD[0]&~0xFFFFu)|(r&0xFFFFu); } }
B4220: if((s32)(cyc-lim)>=0){ i=15; goto ex; }
{ cyc+=4;
AA[5]=(u32)(s32)(s16)(AD[0]&0xFFFFu); }
B4222: if((s32)(cyc-lim)>=0){ i=16; goto ex; }
{ cyc+=4;
cyc+=2;

goto B4226; }
B4224: if((s32)(cyc-lim)>=0){ i=17; goto ex; }
/* CP24_NATIVE_RAM_SPAN: exact 12-cycle MOVE/DBRA pairs. */
{
 u32 n=aot_div12_arm(lim-cyc),count=(AD[0]&65535u)+1u;if(n>count)n=count;
 if(n>=4u&&md_native_ram_bytes(0u,AA[1],n,(AD[7]&255u))){
  AA[1]+=n;cyc+=n*12u;AD[0]=(AD[0]&0xFFFF0000u)|((AD[0]-n)&65535u);
  if((AD[0]&65535u)==65535u)goto NATIVE_DONE_B4224;
  goto B4224;
 }
}
{ cyc+=4;
u32 mv=(AD[7]&0xFFu);
u32 ea1=AA[1]; AA[1]+=1u;
cyc+=4; W8(ea1,mv);
NZL(16u,mv,0xFFu,0x80u); }
B4226: if((s32)(cyc-lim)>=0){ i=18; goto ex; }
{ cyc+=4;
if(!0){ u32 w=(AD[0]-1u)&0xFFFFu; AD[0]=(AD[0]&0xFFFF0000u)|w; if(w!=0xFFFFu){ goto B4224; } } }
NATIVE_DONE_B4224:;
B422A: if((s32)(cyc-lim)>=0){ i=19; goto ex; }
{ cyc+=4;
cyc+=4;
u32 mv=0xFu;
AD[3]=(AD[3]&~0xFFFFu)|((mv)&0xFFFFu);
NZL(0u,mv,0xFFFFu,0x8000u); }
B422E: if((s32)(cyc-lim)>=0){ i=20; goto ex; }
{ cyc+=4;
u32 ea0=AA[2]+0x10u; cyc+=4;
cyc+=4; u32 r2=R16(ea0);
u32 mv=r2;
AD[4]=(AD[4]&~0xFFFFu)|((mv)&0xFFFFu);
NZL(0u,mv,0xFFFFu,0x8000u); }
B4232: if((s32)(cyc-lim)>=0){ i=21; goto ex; }
{ cyc+=4;
u32 ea0=AA[2]+0xEu; cyc+=4;
cyc+=4; u32 r2=R16(ea0);
u32 mv=r2;
AD[5]=(AD[5]&~0xFFFFu)|((mv)&0xFFFFu);
NZL(0u,mv,0xFFFFu,0x8000u); }
B4236: if((s32)(cyc-lim)>=0){ i=22; goto ex; }
{ cyc+=4;
u32 ea0=AA[0]; AA[0]+=2u;
cyc+=4; u32 r2=R16(ea0);
u32 mv=r2;
AD[0]=(AD[0]&~0xFFFFu)|((mv)&0xFFFFu);
NZL(0u,mv,0xFFFFu,0x8000u); }
B4238: if((s32)(cyc-lim)>=0){ i=23; goto ex; }
{ cyc+=4;
cyc+=4;
u32 mv=0x7u;
AD[6]=(AD[6]&~0xFFFFu)|((mv)&0xFFFFu);
NZL(0u,mv,0xFFFFu,0x8000u); }
B423C: if((s32)(cyc-lim)>=0){ i=24; goto ex; }
{ cyc+=4;
u32 mv=(AD[4]&0xFFu);
AD[1]=(AD[1]&~0xFFu)|((mv)&0xFFu);
NZL(0u,mv,0xFFu,0x80u); }
B423E: if((s32)(cyc-lim)>=0){ i=25; goto ex; }
{ cyc+=4;
{ u32 r=ADDL(1u,(AD[0]&0xFFFFu),AD[0],0xFFFFu,0x8000u,16,0,1,0); AD[0]=(AD[0]&~0xFFFFu)|r; } }
B4240: if((s32)(cyc-lim)>=0){ i=26; goto ex; }
{ cyc+=4;
if(CC(sr,4)){ cyc+=2;  goto B4244; } }
B4242: if((s32)(cyc-lim)>=0){ i=27; goto ex; }
{ cyc+=4;
u32 mv=(AD[5]&0xFFu);
AD[1]=(AD[1]&~0xFFu)|((mv)&0xFFu);
NZL(0u,mv,0xFFu,0x80u); }
B4244: if((s32)(cyc-lim)>=0){ i=28; goto ex; }
{ cyc+=4;
{ u32 r=aot_lshift_imm(&sr,&cyc,AD[1],4u,0xFFu,0x80u,1u); AD[1]=(AD[1]&~0xFFu)|(r&0xFFu); } }
B4246: if((s32)(cyc-lim)>=0){ i=29; goto ex; }
{ cyc+=4;
{ u32 r=ADDL(1u,(AD[0]&0xFFFFu),AD[0],0xFFFFu,0x8000u,16,0,1,0); AD[0]=(AD[0]&~0xFFFFu)|r; } }
B4248: if((s32)(cyc-lim)>=0){ i=30; goto ex; }
{ cyc+=4;
if(CC(sr,5)){ cyc+=2;  goto B424E; } }
B424A: if((s32)(cyc-lim)>=0){ i=31; goto ex; }
{ cyc+=4;
u32 r=(AD[1]|(AD[4]&0xFFu))&0xFFu;
NZL(0u,r,0xFFu,0x80u);
AD[1]=(AD[1]&~0xFFu)|r; }
B424C: if((s32)(cyc-lim)>=0){ i=32; goto ex; }
{ cyc+=4;
cyc+=2;

goto B4250; }
B424E: if((s32)(cyc-lim)>=0){ i=33; goto ex; }
{ cyc+=4;
u32 r=(AD[1]|(AD[5]&0xFFu))&0xFFu;
NZL(0u,r,0xFFu,0x80u);
AD[1]=(AD[1]&~0xFFu)|r; }
B4250: if((s32)(cyc-lim)>=0){ i=34; goto ex; }
{ cyc+=4;
u32 mv=(AD[1]&0xFFu);
u32 ea1=AA[1]; AA[1]+=1u;
cyc+=4; W8(ea1,mv);
NZL(0u,mv,0xFFu,0x80u); }
B4252: if((s32)(cyc-lim)>=0){ i=35; goto ex; }
{ cyc+=4;
if(!0){ u32 w=(AD[6]-1u)&0xFFFFu; AD[6]=(AD[6]&0xFFFF0000u)|w; if(w!=0xFFFFu){ goto B423C; } } }
B4256: if((s32)(cyc-lim)>=0){ i=36; goto ex; }
{ cyc+=4;
u32 mv=(AD[7]&0xFFu);
u32 ea1=AA[1]; AA[1]+=1u;
cyc+=4; W8(ea1,mv);
NZL(0u,mv,0xFFu,0x80u); }
B4258: if((s32)(cyc-lim)>=0){ i=37; goto ex; }
{ cyc+=4;
u32 mv=(AD[7]&0xFFu);
u32 ea1=AA[1]; AA[1]+=1u;
cyc+=4; W8(ea1,mv);
NZL(0u,mv,0xFFu,0x80u); }
B425A: if((s32)(cyc-lim)>=0){ i=38; goto ex; }
{ cyc+=4;
u32 mv=(AD[7]&0xFFu);
u32 ea1=AA[1]; AA[1]+=1u;
cyc+=4; W8(ea1,mv);
NZL(0u,mv,0xFFu,0x80u); }
B425C: if((s32)(cyc-lim)>=0){ i=39; goto ex; }
{ cyc+=4;
u32 mv=(AD[7]&0xFFu);
u32 ea1=AA[1]; AA[1]+=1u;
cyc+=4; W8(ea1,mv);
NZL(0u,mv,0xFFu,0x80u); }
B425E: if((s32)(cyc-lim)>=0){ i=40; goto ex; }
{ cyc+=4;
if(!0){ u32 w=(AD[3]-1u)&0xFFFFu; AD[3]=(AD[3]&0xFFFF0000u)|w; if(w!=0xFFFFu){ goto B4236; } } }
B4262: if((s32)(cyc-lim)>=0){ i=41; goto ex; }
{ cyc+=4;
u32 ea0=AA[2]+0x8u; cyc+=4;
cyc+=4; u32 r2=R16(ea0);
u32 mv=r2;
AD[0]=(AD[0]&~0xFFFFu)|((mv)&0xFFFFu);
NZL(0u,mv,0xFFFFu,0x8000u); }
B4266: if((s32)(cyc-lim)>=0){ i=42; goto ex; }
{ cyc+=4;
{ u32 r=aot_lshift_imm(&sr,&cyc,AD[0],5u,0xFFFFu,0x8000u,1u); AD[0]=(AD[0]&~0xFFFFu)|(r&0xFFFFu); } }
B4268: if((s32)(cyc-lim)>=0){ i=43; goto ex; }
{ cyc+=4;
u32 mv=(AD[0]&0xFFFFu);
AD[7]=(AD[7]&~0xFFFFu)|((mv)&0xFFFFu);
NZL(0u,mv,0xFFFFu,0x8000u); }
B426A: if((s32)(cyc-lim)>=0){ i=44; goto ex; }
{ cyc+=4;
u32 ea0=AA[2]+0x14u; cyc+=4;
cyc+=4; u32 r2=R16(ea0);
u32 mv=r2;
AD[0]=(AD[0]&~0xFFFFu)|((mv)&0xFFFFu);
NZL(0u,mv,0xFFFFu,0x8000u); }
B426E: if((s32)(cyc-lim)>=0){ i=45; goto ex; }
{ cyc+=4;
u32 mv=(AD[0]&0xFFFFu);
AD[1]=(AD[1]&~0xFFFFu)|((mv)&0xFFFFu);
NZL(0u,mv,0xFFFFu,0x8000u); }
B4270: if((s32)(cyc-lim)>=0){ i=46; goto ex; }
{ cyc+=4;
{ u32 r=ADDL(0u,(AD[2]&0xFFFFu),AD[1],0xFFFFu,0x8000u,16,0,1,0); AD[1]=(AD[1]&~0xFFFFu)|r; } }
B4272: if((s32)(cyc-lim)>=0){ i=47; goto ex; }
{ cyc+=4;
u32 mv=(AD[1]&0xFFFFu);
u32 ea1=AA[2]+0x14u; cyc+=4;
cyc+=4; W16(ea1,mv);
NZL(0u,mv,0xFFFFu,0x8000u); }
B4276: if((s32)(cyc-lim)>=0){ i=48; goto ex; }
{ cyc+=4;
{ u32 r=aot_lshift_imm(&sr,&cyc,AD[0],3u,0xFFFFu,0x8000u,0u); AD[0]=(AD[0]&~0xFFFFu)|(r&0xFFFFu); } }
B4278: if((s32)(cyc-lim)>=0){ i=49; goto ex; }
{ cyc+=4;
u32 r=SUBL(0u,1u,(AD[1]&0xFFFFu),0xFFFFu,0x8000u,0,1,0);
AD[1]=(AD[1]&~0xFFFFu)|((r)&0xFFFFu); }
B427A: if((s32)(cyc-lim)>=0){ i=50; goto ex; }
{ cyc+=4;
{ u32 r=aot_lshift_imm(&sr,&cyc,AD[1],3u,0xFFFFu,0x8000u,0u); AD[1]=(AD[1]&~0xFFFFu)|(r&0xFFFFu); } }
B427C: if((s32)(cyc-lim)>=0){ i=51; goto ex; }
{ cyc+=4;
{ u32 r=SUBL(0u,(AD[0]&0xFFFFu),AD[1],0xFFFFu,0x8000u,0,1,0); AD[1]=(AD[1]&~0xFFFFu)|r; } }
B427E: if((s32)(cyc-lim)>=0){ i=52; goto ex; }
{ cyc+=4;
u32 mv=(AD[1]&0xFFFFu);
AD[5]=(AD[5]&~0xFFFFu)|((mv)&0xFFFFu);
NZL(0u,mv,0xFFFFu,0x8000u); }
B4280: if((s32)(cyc-lim)>=0){ i=53; goto ex; }
{ cyc+=4;
u32 ea0=AA[2]+0x8u; cyc+=4;
cyc+=4; u32 r2=R16(ea0);
{ u32 r=(AD[0]&0xFFFFu)*((r2)&0xFFFFu);
AD[0]=r; NZL(0u,r,0xFFFFFFFFu,0x80000000u); cyc+=50; } }
B4284: if((s32)(cyc-lim)>=0){ i=54; goto ex; }
{ cyc+=4;
u32 ea0=AA[2]+0x4u; cyc+=4;
cyc+=4; u32 r2=R16(ea0);
{ u32 r=ADDL(0u,r2,AD[0],0xFFFFu,0x8000u,16,0,1,0); AD[0]=(AD[0]&~0xFFFFu)|r; } }
B4288: if((s32)(cyc-lim)>=0){ i=55; goto ex; }
{ cyc+=4;
{ u32 r=aot_lshift_imm(&sr,&cyc,AD[0],5u,0xFFFFu,0x8000u,1u); AD[0]=(AD[0]&~0xFFFFu)|(r&0xFFFFu); } }
B428A: if((s32)(cyc-lim)>=0){ i=56; goto ex; }
{ cyc+=4;
u32 ea0=AA[2]+0x18u; cyc+=4;
cyc+=4; u32 r2=R16(ea0);
{ u32 r=ADDL(0u,r2,AD[0],0xFFFFu,0x8000u,16,0,1,0); AD[0]=(AD[0]&~0xFFFFu)|r; } }
B428E: if((s32)(cyc-lim)>=0){ i=57; goto ex; }
{ cyc+=4;
u32 ea0=AA[2]+0x16u; cyc+=4;
cyc+=4; u32 r2=R16(ea0);
u32 mv=r2;
AD[6]=(AD[6]&~0xFFFFu)|((mv)&0xFFFFu);
NZL(0u,mv,0xFFFFu,0x8000u); }
B4292: if((s32)(cyc-lim)>=0){ i=58; goto ex; }
{ cyc+=4;
cyc+=4;
{ u32 r=(AD[6]&0xFFFFu)*((0x48u)&0xFFFFu);
AD[6]=r; NZL(0u,r,0xFFFFFFFFu,0x80000000u); cyc+=50; } }
B4296: if((s32)(cyc-lim)>=0){ i=59; goto ex; }
{ cyc+=4;
{ u32 r=ADDL(16u,(AD[0]&0xFFFFu),AD[6],0xFFFFu,0x8000u,16,0,1,0); AD[6]=(AD[6]&~0xFFFFu)|r; } }
B4298: if((s32)(cyc-lim)>=0){ i=60; goto ex; }
{ cyc+=4;
u32 mv=(AD[6]&0xFFFFu);
AD[4]=(AD[4]&~0xFFFFu)|((mv)&0xFFFFu);
NZL(16u,mv,0xFFFFu,0x8000u); }
B429A: if((s32)(cyc-lim)>=0){ i=61; goto ex; }
{ cyc+=4;
u32 mv=(AD[5]&0xFFFFu);
AD[3]=(AD[3]&~0xFFFFu)|((mv)&0xFFFFu);
NZL(16u,mv,0xFFFFu,0x8000u); }
B429C: if((s32)(cyc-lim)>=0){ i=62; goto ex; }
{ cyc+=4;
cyc+=4;
AA[0]=0xFFFFE730u; }
B42A0: if((s32)(cyc-lim)>=0){ i=63; goto ex; }
{ cyc+=4;
u32 mv=(AD[4]&0xFFFFu);
AD[1]=(AD[1]&~0xFFFFu)|((mv)&0xFFFFu);
NZL(16u,mv,0xFFFFu,0x8000u); }
B42A2: if((s32)(cyc-lim)>=0){ i=64; goto ex; }
{ cyc+=4;
AD[0]=0x40u;
NZL(31u,0x40u,0xFFFFFFFFu,0x80000000u); }
B42A4: if((s32)(cyc-lim)>=0){ i=65; goto ex; }
{ cyc+=4;
cyc+=4;
AA[7]-=4; W32(AA[7],(hi|0x42A8u));
cyc+=8;
npc=0xFFFFFB84u; goto out; }
B42A8: if((s32)(cyc-lim)>=0){ i=66; goto ex; }
{ cyc+=4;
{ u32 r=ADDL(16u,(AD[7]&0xFFFFu),AD[4],0xFFFFu,0x8000u,16,0,1,0); AD[4]=(AD[4]&~0xFFFFu)|r; } }
B42AA: if((s32)(cyc-lim)>=0){ i=67; goto ex; }
{ cyc+=4;
if(!0){ u32 w=(AD[3]-1u)&0xFFFFu; AD[3]=(AD[3]&0xFFFF0000u)|w; if(w!=0xFFFFu){ goto B42A0; } } }
B42AE: if((s32)(cyc-lim)>=0){ i=68; goto ex; }
{ cyc+=4;
AD[0]=0xFu;
NZL(16u,0xFu,0xFFFFFFFFu,0x80000000u); }
B42B0: if((s32)(cyc-lim)>=0){ i=69; goto ex; }
{ cyc+=4;
cyc+=4;
AA[0]=0xFFFFE630u; }
B42B4: if((s32)(cyc-lim)>=0){ i=70; goto ex; }
{ cyc+=4;
u32 ea0=AA[0]+0x100u; cyc+=4;
AA[1]=ea0; }
B42B8: if((s32)(cyc-lim)>=0){ i=71; goto ex; }
{ cyc+=4;
u32 ea0=AA[0]+0x140u; cyc+=4;
AA[4]=ea0; }
B42BC: if((s32)(cyc-lim)>=0){ i=72; goto ex; }
{ cyc+=4;
u32 mv=(AA[5]&0xFFFFu);
AD[1]=(AD[1]&~0xFFFFu)|((mv)&0xFFFFu);
NZL(31u,mv,0xFFFFu,0x8000u); }
B42BE: if((s32)(cyc-lim)>=0){ i=73; goto ex; }
{ cyc+=4;
u32 ea0=AA[0]+0x180u; cyc+=4;
AA[5]=ea0; }
B42C2: if((s32)(cyc-lim)>=0){ i=74; goto ex; }
{ cyc+=4;
if(CC(sr,6)){ cyc+=2;  goto B42E6; } }
i=75; goto ex;
B42E6: if((s32)(cyc-lim)>=0){ i=76; goto ex; }
{ cyc+=4;
u32 r=SUBL(31u,2u,(AD[1]&0xFFFFu),0xFFFFu,0x8000u,0,1,0);
AD[1]=(AD[1]&~0xFFFFu)|((r)&0xFFFFu); }
B42E8: if((s32)(cyc-lim)>=0){ i=77; goto ex; }
{ cyc+=4;
if(CC(sr,7)){ cyc+=2;  i=78; goto ex; } }
i=79; goto ex;
ex: npc=hi|xpc[i];
out: C.sr=(u16)sr; C.cycles=cyc; C.pc=npc; return rc;
}
u32 aotf103(u32 entry){
 static const u32 xpc[] AOT_TAB={0x42C8u,0x42CAu,0x42CCu,0x42CEu,0x42D2u};
 u32 sr=C.sr, cyc=C.cycles, npc, i, rc=AOT_CONT; const u32 lim=aot_lim, hi=C.pc&0xFF000000u; 
 switch(entry){ case 0: goto B42C8; case 1: goto B42CA; case 2: goto B42CC; case 3: goto B42CE; default: npc=C.pc; goto out; }
B42C8: if((s32)(cyc-lim)>=0){ i=0; goto ex; }
{ cyc+=4;
u32 ea0=AA[0]; AA[0]+=4u;
cyc+=8; u32 r2=R32(ea0);
u32 mv=r2;
u32 ea1=AA[1]; AA[1]+=4u;
cyc+=8; W32(ea1,mv);
NZL(16u,mv,0xFFFFFFFFu,0x80000000u); }
B42CA: if((s32)(cyc-lim)>=0){ i=1; goto ex; }
{ cyc+=4;
u32 ea0=AA[0]; AA[0]+=4u;
cyc+=8; u32 r2=R32(ea0);
u32 mv=r2;
u32 ea1=AA[4]; AA[4]+=4u;
cyc+=8; W32(ea1,mv);
NZL(31u,mv,0xFFFFFFFFu,0x80000000u); }
B42CC: if((s32)(cyc-lim)>=0){ i=2; goto ex; }
{ cyc+=4;
AA[0]+=4u; }
B42CE: if((s32)(cyc-lim)>=0){ i=3; goto ex; }
{ cyc+=4;
if(!0){ u32 w=(AD[0]-1u)&0xFFFFu; AD[0]=(AD[0]&0xFFFF0000u)|w; if(w!=0xFFFFu){ goto B42C8; } } }
i=4; goto ex;
ex: npc=hi|xpc[i];
out: C.sr=(u16)sr; C.cycles=cyc; C.pc=npc; return rc;
}
u32 aotf104(u32 entry){
 static const u32 xpc[] AOT_TAB={0x42D6u,0x42D8u,0x42DAu,0x42DCu,0x42DEu,0x42E2u};
 u32 sr=C.sr, cyc=C.cycles, npc, i, rc=AOT_CONT; const u32 lim=aot_lim, hi=C.pc&0xFF000000u; 
 switch(entry){ case 0: goto B42D6; case 1: goto B42D8; case 2: goto B42DA; case 3: goto B42DC; case 4: goto B42DE; default: npc=C.pc; goto out; }
B42D6: if((s32)(cyc-lim)>=0){ i=0; goto ex; }
{ cyc+=4;
u32 ea0=AA[0]; AA[0]+=4u;
cyc+=8; u32 r2=R32(ea0);
u32 mv=r2;
u32 ea1=AA[1]; AA[1]+=4u;
cyc+=8; W32(ea1,mv);
NZL(16u,mv,0xFFFFFFFFu,0x80000000u); }
B42D8: if((s32)(cyc-lim)>=0){ i=1; goto ex; }
{ cyc+=4;
u32 ea0=AA[0]; AA[0]+=1u;
cyc+=4; u32 r2=R8(ea0);
u32 mv=r2;
u32 ea1=AA[4]; AA[4]+=1u;
cyc+=4; W8(ea1,mv);
NZL(31u,mv,0xFFu,0x80u); }
B42DA: if((s32)(cyc-lim)>=0){ i=2; goto ex; }
{ cyc+=4;
AA[4]+=3u; }
B42DC: if((s32)(cyc-lim)>=0){ i=3; goto ex; }
{ cyc+=4;
AA[0]+=7u; }
B42DE: if((s32)(cyc-lim)>=0){ i=4; goto ex; }
{ cyc+=4;
if(!0){ u32 w=(AD[0]-1u)&0xFFFFu; AD[0]=(AD[0]&0xFFFF0000u)|w; if(w!=0xFFFFu){ goto B42D6; } } }
i=5; goto ex;
ex: npc=hi|xpc[i];
out: C.sr=(u16)sr; C.cycles=cyc; C.pc=npc; return rc;
}
u32 aotf105(u32 entry){
 static const u32 xpc[] AOT_TAB={0x42F2u,0x42F4u,0x42F6u,0x42F8u,0x42FAu,0x42FCu,0x42FEu,0x4300u,0x4304u};
 u32 sr=C.sr, cyc=C.cycles, npc, i, rc=AOT_CONT; const u32 lim=aot_lim, hi=C.pc&0xFF000000u; 
 switch(entry){ case 0: goto B42F2; case 1: goto B42F4; case 2: goto B42F6; case 3: goto B42F8; case 4: goto B42FA; case 5: goto B42FC; case 6: goto B42FE; case 7: goto B4300; default: npc=C.pc; goto out; }
B42F2: if((s32)(cyc-lim)>=0){ i=0; goto ex; }
{ cyc+=4;
AA[1]+=1u; }
B42F4: if((s32)(cyc-lim)>=0){ i=1; goto ex; }
{ cyc+=4;
u32 ea0=AA[0]; AA[0]+=1u;
cyc+=4; u32 r2=R8(ea0);
u32 mv=r2;
u32 ea1=AA[1]; AA[1]+=1u;
cyc+=4; W8(ea1,mv);
NZL(16u,mv,0xFFu,0x80u); }
B42F6: if((s32)(cyc-lim)>=0){ i=2; goto ex; }
{ cyc+=4;
u32 ea0=AA[0]; AA[0]+=2u;
cyc+=4; u32 r2=R16(ea0);
u32 mv=r2;
u32 ea1=AA[1]; AA[1]+=2u;
cyc+=4; W16(ea1,mv);
NZL(16u,mv,0xFFFFu,0x8000u); }
B42F8: if((s32)(cyc-lim)>=0){ i=3; goto ex; }
{ cyc+=4;
u32 ea0=AA[0]; AA[0]+=4u;
cyc+=8; u32 r2=R32(ea0);
u32 mv=r2;
u32 ea1=AA[4]; AA[4]+=4u;
cyc+=8; W32(ea1,mv);
NZL(16u,mv,0xFFFFFFFFu,0x80000000u); }
B42FA: if((s32)(cyc-lim)>=0){ i=4; goto ex; }
{ cyc+=4;
u32 ea0=AA[0]; AA[0]+=1u;
cyc+=4; u32 r2=R8(ea0);
u32 mv=r2;
u32 ea1=AA[5]; AA[5]+=1u;
cyc+=4; W8(ea1,mv);
NZL(31u,mv,0xFFu,0x80u); }
B42FC: if((s32)(cyc-lim)>=0){ i=5; goto ex; }
{ cyc+=4;
AA[5]+=3u; }
B42FE: if((s32)(cyc-lim)>=0){ i=6; goto ex; }
{ cyc+=4;
AA[0]+=4u; }
B4300: if((s32)(cyc-lim)>=0){ i=7; goto ex; }
{ cyc+=4;
if(!0){ u32 w=(AD[0]-1u)&0xFFFFu; AD[0]=(AD[0]&0xFFFF0000u)|w; if(w!=0xFFFFu){ goto B42F2; } } }
i=8; goto ex;
ex: npc=hi|xpc[i];
out: C.sr=(u16)sr; C.cycles=cyc; C.pc=npc; return rc;
}
u32 aotf106(u32 entry){
 static const u32 xpc[] AOT_TAB={0x4306u,0x4308u,0x430Au,0x430Cu,0x430Eu,0x4310u,0x4312u,0x4316u};
 u32 sr=C.sr, cyc=C.cycles, npc, i, rc=AOT_CONT; const u32 lim=aot_lim, hi=C.pc&0xFF000000u; 
 switch(entry){ case 0: goto B4306; case 1: goto B4308; case 2: goto B430A; case 3: goto B430C; case 4: goto B430E; case 5: goto B4310; case 6: goto B4312; default: npc=C.pc; goto out; }
B4306: if((s32)(cyc-lim)>=0){ i=0; goto ex; }
{ cyc+=4;
AA[1]+=1u; }
B4308: if((s32)(cyc-lim)>=0){ i=1; goto ex; }
{ cyc+=4;
u32 ea0=AA[0]; AA[0]+=1u;
cyc+=4; u32 r2=R8(ea0);
u32 mv=r2;
u32 ea1=AA[1]; AA[1]+=1u;
cyc+=4; W8(ea1,mv);
NZL(16u,mv,0xFFu,0x80u); }
B430A: if((s32)(cyc-lim)>=0){ i=2; goto ex; }
{ cyc+=4;
u32 ea0=AA[0]; AA[0]+=2u;
cyc+=4; u32 r2=R16(ea0);
u32 mv=r2;
u32 ea1=AA[1]; AA[1]+=2u;
cyc+=4; W16(ea1,mv);
NZL(16u,mv,0xFFFFu,0x8000u); }
B430C: if((s32)(cyc-lim)>=0){ i=3; goto ex; }
{ cyc+=4;
u32 ea0=AA[0]; AA[0]+=2u;
cyc+=4; u32 r2=R16(ea0);
u32 mv=r2;
u32 ea1=AA[4]; AA[4]+=2u;
cyc+=4; W16(ea1,mv);
NZL(31u,mv,0xFFFFu,0x8000u); }
B430E: if((s32)(cyc-lim)>=0){ i=4; goto ex; }
{ cyc+=4;
AA[4]+=2u; }
B4310: if((s32)(cyc-lim)>=0){ i=5; goto ex; }
{ cyc+=4;
AA[0]+=7u; }
B4312: if((s32)(cyc-lim)>=0){ i=6; goto ex; }
{ cyc+=4;
if(!0){ u32 w=(AD[0]-1u)&0xFFFFu; AD[0]=(AD[0]&0xFFFF0000u)|w; if(w!=0xFFFFu){ goto B4306; } } }
i=7; goto ex;
ex: npc=hi|xpc[i];
out: C.sr=(u16)sr; C.cycles=cyc; C.pc=npc; return rc;
}
u32 aotf107(u32 entry){
 static const u32 xpc[] AOT_TAB={0x431Eu,0x4320u,0x4322u,0x4324u,0x4326u,0x4328u,0x432Au,0x432Eu};
 u32 sr=C.sr, cyc=C.cycles, npc, i, rc=AOT_CONT; const u32 lim=aot_lim, hi=C.pc&0xFF000000u; 
 switch(entry){ case 0: goto B431E; case 1: goto B4320; case 2: goto B4322; case 3: goto B4324; case 4: goto B4326; case 5: goto B4328; case 6: goto B432A; default: npc=C.pc; goto out; }
B431E: if((s32)(cyc-lim)>=0){ i=0; goto ex; }
{ cyc+=4;
AA[1]+=2u; }
B4320: if((s32)(cyc-lim)>=0){ i=1; goto ex; }
{ cyc+=4;
u32 ea0=AA[0]; AA[0]+=2u;
cyc+=4; u32 r2=R16(ea0);
u32 mv=r2;
u32 ea1=AA[1]; AA[1]+=2u;
cyc+=4; W16(ea1,mv);
NZL(16u,mv,0xFFFFu,0x8000u); }
B4322: if((s32)(cyc-lim)>=0){ i=2; goto ex; }
{ cyc+=4;
u32 ea0=AA[0]; AA[0]+=4u;
cyc+=8; u32 r2=R32(ea0);
u32 mv=r2;
u32 ea1=AA[4]; AA[4]+=4u;
cyc+=8; W32(ea1,mv);
NZL(16u,mv,0xFFFFFFFFu,0x80000000u); }
B4324: if((s32)(cyc-lim)>=0){ i=3; goto ex; }
{ cyc+=4;
u32 ea0=AA[0]; AA[0]+=2u;
cyc+=4; u32 r2=R16(ea0);
u32 mv=r2;
u32 ea1=AA[5]; AA[5]+=2u;
cyc+=4; W16(ea1,mv);
NZL(31u,mv,0xFFFFu,0x8000u); }
B4326: if((s32)(cyc-lim)>=0){ i=4; goto ex; }
{ cyc+=4;
AA[5]+=2u; }
B4328: if((s32)(cyc-lim)>=0){ i=5; goto ex; }
{ cyc+=4;
AA[0]+=4u; }
B432A: if((s32)(cyc-lim)>=0){ i=6; goto ex; }
{ cyc+=4;
if(!0){ u32 w=(AD[0]-1u)&0xFFFFu; AD[0]=(AD[0]&0xFFFF0000u)|w; if(w!=0xFFFFu){ goto B431E; } } }
i=7; goto ex;
ex: npc=hi|xpc[i];
out: C.sr=(u16)sr; C.cycles=cyc; C.pc=npc; return rc;
}
u32 aotf108(u32 entry){
 static const u32 xpc[] AOT_TAB={0x4330u,0x4332u,0x4334u,0x4336u,0x4338u,0x433Au,0x433Cu,0x4340u};
 u32 sr=C.sr, cyc=C.cycles, npc, i, rc=AOT_CONT; const u32 lim=aot_lim, hi=C.pc&0xFF000000u; 
 switch(entry){ case 0: goto B4330; case 1: goto B4332; case 2: goto B4334; case 3: goto B4336; case 4: goto B4338; case 5: goto B433A; case 6: goto B433C; default: npc=C.pc; goto out; }
B4330: if((s32)(cyc-lim)>=0){ i=0; goto ex; }
{ cyc+=4;
AA[1]+=2u; }
B4332: if((s32)(cyc-lim)>=0){ i=1; goto ex; }
{ cyc+=4;
u32 ea0=AA[0]; AA[0]+=2u;
cyc+=4; u32 r2=R16(ea0);
u32 mv=r2;
u32 ea1=AA[1]; AA[1]+=2u;
cyc+=4; W16(ea1,mv);
NZL(16u,mv,0xFFFFu,0x8000u); }
B4334: if((s32)(cyc-lim)>=0){ i=2; goto ex; }
{ cyc+=4;
u32 ea0=AA[0]; AA[0]+=2u;
cyc+=4; u32 r2=R16(ea0);
u32 mv=r2;
u32 ea1=AA[4]; AA[4]+=2u;
cyc+=4; W16(ea1,mv);
NZL(16u,mv,0xFFFFu,0x8000u); }
B4336: if((s32)(cyc-lim)>=0){ i=3; goto ex; }
{ cyc+=4;
u32 ea0=AA[0]; AA[0]+=1u;
cyc+=4; u32 r2=R8(ea0);
u32 mv=r2;
u32 ea1=AA[4]; AA[4]+=1u;
cyc+=4; W8(ea1,mv);
NZL(31u,mv,0xFFu,0x80u); }
B4338: if((s32)(cyc-lim)>=0){ i=4; goto ex; }
{ cyc+=4;
AA[4]+=1u; }
B433A: if((s32)(cyc-lim)>=0){ i=5; goto ex; }
{ cyc+=4;
AA[0]+=7u; }
B433C: if((s32)(cyc-lim)>=0){ i=6; goto ex; }
{ cyc+=4;
if(!0){ u32 w=(AD[0]-1u)&0xFFFFu; AD[0]=(AD[0]&0xFFFF0000u)|w; if(w!=0xFFFFu){ goto B4330; } } }
i=7; goto ex;
ex: npc=hi|xpc[i];
out: C.sr=(u16)sr; C.cycles=cyc; C.pc=npc; return rc;
}
u32 aotf109(u32 entry){
 static const u32 xpc[] AOT_TAB={0x435Cu,0x435Eu,0x4360u,0x4362u,0x4364u,0x4368u,0x436Eu,0x4370u,0x4372u,0x4374u,0x4378u,0x437Au,0x437Eu,0x4382u,0x4386u};
 u32 sr=C.sr, cyc=C.cycles, npc, i, rc=AOT_CONT; const u32 lim=aot_lim, hi=C.pc&0xFF000000u; 
 switch(entry){ case 0: goto B435C; case 1: goto B435E; case 2: goto B4360; case 3: goto B4362; case 4: goto B4364; case 5: goto B4368; case 6: goto B436E; case 7: goto B4370; case 8: goto B4372; case 9: goto B4374; case 10: goto B4378; case 11: goto B437A; case 12: goto B437E; case 13: goto B4382; case 14: goto B4386; default: npc=C.pc; goto out; }
B435C: if((s32)(cyc-lim)>=0){ i=0; goto ex; }
{ cyc+=4;
AA[1]+=3u; }
B435E: if((s32)(cyc-lim)>=0){ i=1; goto ex; }
{ cyc+=4;
u32 ea0=AA[0]; AA[0]+=1u;
cyc+=4; u32 r2=R8(ea0);
u32 mv=r2;
u32 ea1=AA[1]; AA[1]+=1u;
cyc+=4; W8(ea1,mv);
NZL(16u,mv,0xFFu,0x80u); }
B4360: if((s32)(cyc-lim)>=0){ i=2; goto ex; }
{ cyc+=4;
u32 ea0=AA[0]; AA[0]+=4u;
cyc+=8; u32 r2=R32(ea0);
u32 mv=r2;
u32 ea1=AA[4]; AA[4]+=4u;
cyc+=8; W32(ea1,mv);
NZL(16u,mv,0xFFFFFFFFu,0x80000000u); }
B4362: if((s32)(cyc-lim)>=0){ i=3; goto ex; }
{ cyc+=4;
AA[0]+=7u; }
B4364: if((s32)(cyc-lim)>=0){ i=4; goto ex; }
{ cyc+=4;
if(!0){ u32 w=(AD[0]-1u)&0xFFFFu; AD[0]=(AD[0]&0xFFFF0000u)|w; if(w!=0xFFFFu){ goto B435C; } } }
B4368: if((s32)(cyc-lim)>=0){ i=5; goto ex; }
{ cyc+=4;
cyc+=4;
u32 mv=0xE730u;
AD[4]=mv;
NZL(16u,mv,0xFFFFFFFFu,0x80000000u); }
B436E: if((s32)(cyc-lim)>=0){ i=6; goto ex; }
{ cyc+=4;
u32 mv=(AD[6]&0xFFFFu);
AD[2]=(AD[2]&~0xFFFFu)|((mv)&0xFFFFu);
NZL(16u,mv,0xFFFFu,0x8000u); }
B4370: if((s32)(cyc-lim)>=0){ i=7; goto ex; }
{ cyc+=4;
u32 mv=(AD[4]&0xFFFFu);
AD[1]=(AD[1]&~0xFFFFu)|((mv)&0xFFFFu);
NZL(16u,mv,0xFFFFu,0x8000u); }
B4372: if((s32)(cyc-lim)>=0){ i=8; goto ex; }
{ cyc+=4;
AD[0]=0x40u;
NZL(31u,0x40u,0xFFFFFFFFu,0x80000000u); }
B4374: if((s32)(cyc-lim)>=0){ i=9; goto ex; }
{ cyc+=4;
cyc+=4;
AA[7]-=4; W32(AA[7],(hi|0x4378u));
cyc+=8;
npc=0xFFFFFB90u; goto out; }
B4378: if((s32)(cyc-lim)>=0){ i=10; goto ex; }
{ cyc+=4;
{ u32 r=ADDL(0u,(AD[7]&0xFFFFu),AD[6],0xFFFFu,0x8000u,16,0,1,0); AD[6]=(AD[6]&~0xFFFFu)|r; } }
B437A: if((s32)(cyc-lim)>=0){ i=11; goto ex; }
{ cyc+=4;
u32 r=ADDL(31u,0x40u,(AD[4]&0xFFFFu),0xFFFFu,0x8000u,16,0,1,0);
AD[4]=(AD[4]&~0xFFFFu)|((r)&0xFFFFu); }
B437E: if((s32)(cyc-lim)>=0){ i=12; goto ex; }
{ cyc+=4;
if(!0){ u32 w=(AD[5]-1u)&0xFFFFu; AD[5]=(AD[5]&0xFFFF0000u)|w; if(w!=0xFFFFu){ goto B436E; } } }
B4382: if((s32)(cyc-lim)>=0){ i=13; goto ex; }
{ cyc+=4;
{ u32 a=AA[7];
u32 native_n=m68k_movem_ram(a,0x30FCu,1u); if(native_n){a+=4u*native_n;cyc+=8u*native_n;}else{
AD[2]=R32(a); a+=4; cyc+=8;
AD[3]=R32(a); a+=4; cyc+=8;
AD[4]=R32(a); a+=4; cyc+=8;
AD[5]=R32(a); a+=4; cyc+=8;
AD[6]=R32(a); a+=4; cyc+=8;
AD[7]=R32(a); a+=4; cyc+=8;
AA[4]=R32(a); a+=4; cyc+=8;
AA[5]=R32(a); a+=4; cyc+=8;
}
AA[7]=a;
} }
B4386: if((s32)(cyc-lim)>=0){ i=14; goto ex; }
{ cyc+=4;
{ u32 t=R32(AA[7]); AA[7]+=4; cyc+=8;
npc=t; goto out;
} }
ex: npc=hi|xpc[i];
out: C.sr=(u16)sr; C.cycles=cyc; C.pc=npc; return rc;
}
u32 aotf110(u32 entry){
 static const u32 xpc[] AOT_TAB={0x43A0u,0x43A2u,0x43A6u,0x43A8u,0x43B4u,0x43B6u,0x43BAu,0x43BCu,0x43C0u,0x4428u,0x43C2u,0x43C4u,0x43C6u,0x43CAu,0x43CEu,0x43D0u,0x43D4u,0x4432u,0x43D6u,0x43DAu,0x43DCu,0x43E0u,0x43E2u,0x43E4u,0x43E8u,0x43EAu,0x43FAu,0x43FCu,0x43FEu,0x4400u,0x4402u,0x4406u,0x4408u,0x440Au,0x440Eu,0x4410u,0x4412u,0x4414u,0x4416u,0x441Cu,0x441Eu,0x4420u,0x4422u,0x4426u,0x448Cu,0x4492u,0x4494u,0x4496u,0x4498u};
 u32 sr=C.sr, cyc=C.cycles, npc, i, rc=AOT_CONT; const u32 lim=aot_lim, hi=C.pc&0xFF000000u; 
 switch(entry){ case 0: goto B43A0; case 1: goto B43A2; case 2: goto B43A6; case 3: goto B43B4; case 4: goto B43B6; case 5: goto B43BA; case 6: goto B43BC; case 7: goto B43C0; case 8: goto B43C2; case 9: goto B43C4; case 10: goto B43C6; case 11: goto B43CA; case 12: goto B43CE; case 13: goto B43D0; case 14: goto B43D4; case 15: goto B43D6; case 16: goto B43DA; case 17: goto B43DC; case 18: goto B43E0; case 19: goto B43E2; case 20: goto B43E4; case 21: goto B43E8; case 22: goto B43FA; case 23: goto B43FC; case 24: goto B43FE; case 25: goto B4400; case 26: goto B4402; case 27: goto B4406; case 28: goto B4408; case 29: goto B440A; case 30: goto B440E; case 31: goto B4410; case 32: goto B4412; case 33: goto B4414; case 34: goto B4416; case 35: goto B441C; case 36: goto B441E; case 37: goto B4420; case 38: goto B4422; case 39: goto B4426; case 40: goto B448C; case 41: goto B4492; case 42: goto B4494; case 43: goto B4496; case 44: goto B4498; default: npc=C.pc; goto out; }
B43A0: if((s32)(cyc-lim)>=0){ i=0; goto ex; }
{ cyc+=4;
AD[3]=0x10u;
NZL(16u,0x10u,0xFFFFFFFFu,0x80000000u); }
B43A2: if((s32)(cyc-lim)>=0){ i=1; goto ex; }
{ cyc+=4;
CMPL(31u,0x791u,(AD[0]&0xFFFFu),0xFFFFu,0x8000u); }
B43A6: if((s32)(cyc-lim)>=0){ i=2; goto ex; }
{ cyc+=4;
if(CC(sr,4)){ cyc+=2;  goto B43B4; } }
i=3; goto ex;
B43B4: if((s32)(cyc-lim)>=0){ i=4; goto ex; }
{ cyc+=4;
u32 mv=(AD[0]&0xFFFFu);
AD[1]=(AD[1]&~0xFFFFu)|((mv)&0xFFFFu);
NZL(0u,mv,0xFFFFu,0x8000u); }
B43B6: if((s32)(cyc-lim)>=0){ i=5; goto ex; }
{ cyc+=4;
u32 r=(AD[1]&0xFFFFu)&0xFFu;
NZL(0u,r,0xFFFFu,0x8000u);
AD[1]=(AD[1]&~0xFFFFu)|((r)&0xFFFFu); }
B43BA: if((s32)(cyc-lim)>=0){ i=6; goto ex; }
{ cyc+=4;
{ u32 r=aot_lshift_imm(&sr,&cyc,AD[0],8u,0xFFFFu,0x8000u,0u); AD[0]=(AD[0]&~0xFFFFu)|(r&0xFFFFu); } }
B43BC: if((s32)(cyc-lim)>=0){ i=7; goto ex; }
{ cyc+=4;
CMPL(31u,0x7Fu,(AD[1]&0xFFFFu),0xFFFFu,0x8000u); }
B43C0: if((s32)(cyc-lim)>=0){ i=8; goto ex; }
{ cyc+=4;
if(CC(sr,7)){ cyc+=2;  i=9; goto ex; } }
B43C2: if((s32)(cyc-lim)>=0){ i=10; goto ex; }
{ cyc+=4;
if(CC(sr,5)){ cyc+=2;  goto B43C6; } }
B43C4: if((s32)(cyc-lim)>=0){ i=11; goto ex; }
{ cyc+=4;
u32 r=SUBL(0u,1u,(AD[1]&0xFFFFu),0xFFFFu,0x8000u,0,1,0);
AD[1]=(AD[1]&~0xFFFFu)|((r)&0xFFFFu); }
B43C6: if((s32)(cyc-lim)>=0){ i=12; goto ex; }
{ cyc+=4;
u32 r=SUBL(16u,0x40u,(AD[1]&0xFFFFu),0xFFFFu,0x8000u,0,1,0);
AD[1]=(AD[1]&~0xFFFFu)|((r)&0xFFFFu); }
B43CA: if((s32)(cyc-lim)>=0){ i=13; goto ex; }
{ cyc+=4;
CMPL(31u,0xBCu,(AD[1]&0xFFFFu),0xFFFFu,0x8000u); }
B43CE: if((s32)(cyc-lim)>=0){ i=14; goto ex; }
{ cyc+=4;
if(CC(sr,4)){ cyc+=2;  i=9; goto ex; } }
B43D0: if((s32)(cyc-lim)>=0){ i=15; goto ex; }
{ cyc+=4;
CMPL(31u,0x88u,(AD[0]&0xFFFFu),0xFFFFu,0x8000u); }
B43D4: if((s32)(cyc-lim)>=0){ i=16; goto ex; }
{ cyc+=4;
if(CC(sr,4)){ cyc+=2;  i=17; goto ex; } }
B43D6: if((s32)(cyc-lim)>=0){ i=18; goto ex; }
{ cyc+=4;
u32 r=SUBL(31u,0x81u,(AD[0]&0xFFFFu),0xFFFFu,0x8000u,0,1,0);
AD[0]=(AD[0]&~0xFFFFu)|((r)&0xFFFFu); }
B43DA: if((s32)(cyc-lim)>=0){ i=19; goto ex; }
{ cyc+=4;
if(CC(sr,5)){ cyc+=2;  i=9; goto ex; } }
B43DC: if((s32)(cyc-lim)>=0){ i=20; goto ex; }
{ cyc+=4;
u32 ea0=hi|0x449Au; cyc+=4;
AA[0]=ea0; }
B43E0: if((s32)(cyc-lim)>=0){ i=21; goto ex; }
{ cyc+=4;
if(CC(sr,7)){ cyc+=2;  goto B43FA; } }
B43E2: if((s32)(cyc-lim)>=0){ i=22; goto ex; }
{ cyc+=4;
u32 r=SUBL(31u,1u,(AD[0]&0xFFFFu),0xFFFFu,0x8000u,0,1,0);
AD[0]=(AD[0]&~0xFFFFu)|((r)&0xFFFFu); }
B43E4: if((s32)(cyc-lim)>=0){ i=23; goto ex; }
{ cyc+=4;
u32 ea0=hi|0x44BAu; cyc+=4;
AA[0]=ea0; }
B43E8: if((s32)(cyc-lim)>=0){ i=24; goto ex; }
{ cyc+=4;
if(CC(sr,7)){ cyc+=2;  goto B43FA; } }
i=25; goto ex;
B43FA: if((s32)(cyc-lim)>=0){ i=26; goto ex; }
{ cyc+=4;
u32 ea0=AA[0]; AA[0]+=1u;
cyc+=4; u32 r2=R8(ea0);
CMPL(31u,r2,AD[1],0xFFu,0x80u); }
B43FC: if((s32)(cyc-lim)>=0){ i=27; goto ex; }
{ cyc+=4;
if(CC(sr,5)){ cyc+=2;  i=9; goto ex; } }
B43FE: if((s32)(cyc-lim)>=0){ i=28; goto ex; }
{ cyc+=4;
u32 ea0=AA[0]; AA[0]+=1u;
cyc+=4; u32 r2=R8(ea0);
CMPL(21u,r2,AD[1],0xFFu,0x80u); }
B4400: if((s32)(cyc-lim)>=0){ i=29; goto ex; }
{ cyc+=4;
if(CC(sr,2)){ cyc+=2;  goto B441E; } }
B4402: if((s32)(cyc-lim)>=0){ i=30; goto ex; }
{ cyc+=4;
u32 ea0=AA[0]+0xFFFFFFFEu; cyc+=4;
cyc+=4; u32 r2=R8(ea0);
{ u32 r=SUBL(0u,r2,AD[1],0xFFu,0x80u,0,1,0); AD[1]=(AD[1]&~0xFFu)|r; } }
B4406: if((s32)(cyc-lim)>=0){ i=31; goto ex; }
{ cyc+=4;
u32 ea0=AA[0]; AA[0]+=2u;
cyc+=4; u32 r2=R16(ea0);
u32 mv=r2;
AD[2]=(AD[2]&~0xFFFFu)|((mv)&0xFFFFu);
NZL(8u,mv,0xFFFFu,0x8000u); }
B4408: if((s32)(cyc-lim)>=0){ i=32; goto ex; }
{ cyc+=4;
if(CC(sr,10)){ cyc+=2;  goto B4414; } }
B440A: if((s32)(cyc-lim)>=0){ i=33; goto ex; }
{ cyc+=4;
u32 ea0=AA[2]+0x2u; cyc+=4;
cyc+=4; u32 r2=R16(ea0);
NZL(4u,r2,0xFFFFu,0x8000u); }
B440E: if((s32)(cyc-lim)>=0){ i=34; goto ex; }
{ cyc+=4;
if(CC(sr,6)){ cyc+=2;  goto B4414; } }
B4410: if((s32)(cyc-lim)>=0){ i=35; goto ex; }
{ cyc+=4;
u32 ea0=AA[0];
cyc+=4; u32 r2=R16(ea0);
u32 mv=r2;
AD[2]=(AD[2]&~0xFFFFu)|((mv)&0xFFFFu);
NZL(0u,mv,0xFFFFu,0x8000u); }
B4412: if((s32)(cyc-lim)>=0){ i=36; goto ex; }
{ cyc+=4;
AD[3]=0xAu;
NZL(0u,0xAu,0xFFFFFFFFu,0x80000000u); }
B4414: if((s32)(cyc-lim)>=0){ i=37; goto ex; }
{ cyc+=4;
{ u32 r=ADDL(0u,(AD[1]&0xFFFFu),AD[2],0xFFFFu,0x8000u,16,0,1,0); AD[2]=(AD[2]&~0xFFFFu)|r; } }
B4416: if((s32)(cyc-lim)>=0){ i=38; goto ex; }
{ cyc+=4;
cyc+=8;
AA[0]=0xBB98u; }
B441C: if((s32)(cyc-lim)>=0){ i=39; goto ex; }
{ cyc+=4;
cyc+=2;

goto B448C; }
B441E: if((s32)(cyc-lim)>=0){ i=40; goto ex; }
{ cyc+=4;
u32 ea0=AA[0]; AA[0]+=2u;
cyc+=4; u32 r2=R16(ea0);
u32 mv=r2;
AD[2]=(AD[2]&~0xFFFFu)|((mv)&0xFFFFu);
NZL(24u,mv,0xFFFFu,0x8000u); }
B4420: if((s32)(cyc-lim)>=0){ i=41; goto ex; }
{ cyc+=4;
if(CC(sr,10)){ cyc+=2;  goto B43FA; } }
B4422: if((s32)(cyc-lim)>=0){ i=42; goto ex; }
{ cyc+=4;
u32 ea0=AA[0]+0x2u; cyc+=4;
AA[0]=ea0; }
B4426: if((s32)(cyc-lim)>=0){ i=43; goto ex; }
{ cyc+=4;
cyc+=2;

goto B43FA; }
B448C: if((s32)(cyc-lim)>=0){ i=44; goto ex; }
{ cyc+=4;
u32 r=(AD[2]&0xFFFFFFFFu)&0x7FFFu;
NZL(0u,r,0xFFFFFFFFu,0x80000000u);
AD[2]=r; }
B4492: if((s32)(cyc-lim)>=0){ i=45; goto ex; }
{ cyc+=4;
{ u32 r=aot_lshift_imm(&sr,&cyc,AD[2],5u,0xFFFFFFFFu,0x80000000u,1u); AD[2]=(AD[2]&~0xFFFFFFFFu)|(r&0xFFFFFFFFu); } }
B4494: if((s32)(cyc-lim)>=0){ i=46; goto ex; }
{ cyc+=4;
AA[0]+=(AD[2]&0xFFFFFFFFu); }
B4496: if((s32)(cyc-lim)>=0){ i=47; goto ex; }
{ cyc+=4;
u32 mv=(AD[3]&0xFFFFu);
AD[0]=(AD[0]&~0xFFFFu)|((mv)&0xFFFFu);
NZL(31u,mv,0xFFFFu,0x8000u); }
B4498: if((s32)(cyc-lim)>=0){ i=48; goto ex; }
{ cyc+=4;
{ u32 t=R32(AA[7]); AA[7]+=4; cyc+=8;
npc=t; goto out;
} }
ex: npc=hi|xpc[i];
out: C.sr=(u16)sr; C.cycles=cyc; C.pc=npc; return rc;
}
u32 aotf111(u32 entry){
 static const u32 xpc[] AOT_TAB={0x4914u,0x4916u,0x4918u,0x491Au,0x491Eu,0x4922u,0x4924u,0x4926u,0x4928u,0x492Au,0x492Eu,0x4930u,0x4934u,0x4936u,0x4938u,0x493Cu,0x493Eu,0x4942u,0x4944u,0x4946u,0x4948u,0x494Cu,0x494Eu,0x4950u,0x4952u,0x4954u,0x4956u,0x4974u,0x4958u,0x495Cu,0x495Eu,0x4962u,0x4964u,0x4966u,0x496Au,0x496Cu,0x4970u,0x4972u,0x4976u};
 u32 sr=C.sr, cyc=C.cycles, npc, i, rc=AOT_CONT; const u32 lim=aot_lim, hi=C.pc&0xFF000000u; 
 switch(entry){ case 0: goto B4914; case 1: goto B4916; case 2: goto B4918; case 3: goto B491A; case 4: goto B491E; case 5: goto B4922; case 6: goto B4924; case 7: goto B4926; case 8: goto B4928; case 9: goto B492A; case 10: goto B492E; case 11: goto B4930; case 12: goto B4934; case 13: goto B4936; case 14: goto B4938; case 15: goto B493C; case 16: goto B493E; case 17: goto B4942; case 18: goto B4944; case 19: goto B4946; case 20: goto B4948; case 21: goto B494C; case 22: goto B494E; case 23: goto B4950; case 24: goto B4952; case 25: goto B4954; case 26: goto B4956; case 27: goto B4958; case 28: goto B495C; case 29: goto B495E; case 30: goto B4962; case 31: goto B4964; case 32: goto B4966; case 33: goto B496A; case 34: goto B4970; case 35: goto B4972; case 36: goto B4976; default: npc=C.pc; goto out; }
B4914: if((s32)(cyc-lim)>=0){ i=0; goto ex; }
{ cyc+=4;
u32 mv=(AD[4]&0xFFFFu);
AD[0]=(AD[0]&~0xFFFFu)|((mv)&0xFFFFu);
NZL(0u,mv,0xFFFFu,0x8000u); }
B4916: if((s32)(cyc-lim)>=0){ i=1; goto ex; }
{ cyc+=4;
{ u32 r=ADDL(0u,(AD[0]&0xFFFFu),AD[0],0xFFFFu,0x8000u,16,0,1,0); AD[0]=(AD[0]&~0xFFFFu)|r; } }
B4918: if((s32)(cyc-lim)>=0){ i=2; goto ex; }
{ cyc+=4;
{ u32 r=ADDL(0u,(AD[0]&0xFFFFu),AD[0],0xFFFFu,0x8000u,16,0,1,0); AD[0]=(AD[0]&~0xFFFFu)|r; } }
B491A: if((s32)(cyc-lim)>=0){ i=3; goto ex; }
{ cyc+=4;
cyc+=4;
cyc+=8; u32 r2=(((u32)RAM16(0xF7D0u)<<16)|RAM16(0xF7D2u));
AA[0]=r2; }
B491E: if((s32)(cyc-lim)>=0){ i=4; goto ex; }
{ cyc+=4;
u32 ea0=AA[0]+((u32)(s32)(s16)AD[0]+0x0u); cyc+=6;
AA[0]=ea0; }
B4922: if((s32)(cyc-lim)>=0){ i=5; goto ex; }
{ cyc+=4;
u32 ea0=AA[0]; AA[0]+=1u;
cyc+=4; u32 r2=R8(ea0);
u32 mv=r2;
AD[0]=(AD[0]&~0xFFu)|((mv)&0xFFu);
NZL(0u,mv,0xFFu,0x80u); }
B4924: if((s32)(cyc-lim)>=0){ i=6; goto ex; }
{ cyc+=4;
{ u32 v=(u32)(s32)(s8)AD[0]; AD[0]=(AD[0]&0xFFFF0000u)|(v&0xFFFFu); NZL(0u,v,0xFFFFu,0x8000u); } }
B4926: if((s32)(cyc-lim)>=0){ i=7; goto ex; }
{ cyc+=4;
{ u32 r=ADDL(31u,(AD[3]&0xFFFFu),AD[0],0xFFFFu,0x8000u,16,0,1,0); AD[0]=(AD[0]&~0xFFFFu)|r; } }
B4928: if((s32)(cyc-lim)>=0){ i=8; goto ex; }
{ cyc+=4;
if(CC(sr,11)){ cyc+=2;  goto B4976; } }
B492A: if((s32)(cyc-lim)>=0){ i=9; goto ex; }
{ cyc+=4;
CMPL(31u,0x60u,(AD[0]&0xFFFFu),0xFFFFu,0x8000u); }
B492E: if((s32)(cyc-lim)>=0){ i=10; goto ex; }
{ cyc+=4;
if(CC(sr,5)){ cyc+=2;  goto B4976; } }
B4930: if((s32)(cyc-lim)>=0){ i=11; goto ex; }
{ cyc+=4;
CMPL(31u,0x160u,(AD[0]&0xFFFFu),0xFFFFu,0x8000u); }
B4934: if((s32)(cyc-lim)>=0){ i=12; goto ex; }
{ cyc+=4;
if(CC(sr,4)){ cyc+=2;  goto B4976; } }
B4936: if((s32)(cyc-lim)>=0){ i=13; goto ex; }
{ cyc+=4;
u32 mv=(AD[0]&0xFFFFu);
u32 ea1=AA[2]; AA[2]+=2u;
cyc+=4; W16(ea1,mv);
NZL(0u,mv,0xFFFFu,0x8000u); }
B4938: if((s32)(cyc-lim)>=0){ i=14; goto ex; }
{ cyc+=4;
u32 ea0=AA[0]+0x1u; cyc+=4;
cyc+=4; u32 r2=R16(ea0);
u32 mv=r2;
AD[0]=(AD[0]&~0xFFFFu)|((mv)&0xFFFFu);
NZL(0u,mv,0xFFFFu,0x8000u); }
B493C: if((s32)(cyc-lim)>=0){ i=15; goto ex; }
{ cyc+=4;
u32 mv=(AD[0]&0xFFFFu);
AD[1]=(AD[1]&~0xFFFFu)|((mv)&0xFFFFu);
NZL(0u,mv,0xFFFFu,0x8000u); }
B493E: if((s32)(cyc-lim)>=0){ i=16; goto ex; }
{ cyc+=4;
u32 r=(AD[0]&0xFFFFu)&0xF000u;
NZL(0u,r,0xFFFFu,0x8000u);
AD[0]=(AD[0]&~0xFFFFu)|((r)&0xFFFFu); }
B4942: if((s32)(cyc-lim)>=0){ i=17; goto ex; }
{ cyc+=4;
{ u32 r=aot_lshift_imm(&sr,&cyc,AD[0],4u,0xFFFFu,0x8000u,0u); AD[0]=(AD[0]&~0xFFFFu)|(r&0xFFFFu); } }
B4944: if((s32)(cyc-lim)>=0){ i=18; goto ex; }
{ cyc+=4;
{ u32 r=ADDL(0u,(AA[3]&0xFFFFu),AD[0],0xFFFFu,0x8000u,16,0,1,0); AD[0]=(AD[0]&~0xFFFFu)|r; } }
B4946: if((s32)(cyc-lim)>=0){ i=19; goto ex; }
{ cyc+=4;
u32 mv=(AD[0]&0xFFFFu);
u32 ea1=AA[2]; AA[2]+=2u;
cyc+=4; W16(ea1,mv);
NZL(0u,mv,0xFFFFu,0x8000u); }
B4948: if((s32)(cyc-lim)>=0){ i=20; goto ex; }
{ cyc+=4;
u32 r=(AD[1]&0xFFFFu)&0x7FFu;
NZL(0u,r,0xFFFFu,0x8000u);
AD[1]=(AD[1]&~0xFFFFu)|((r)&0xFFFFu); }
B494C: if((s32)(cyc-lim)>=0){ i=21; goto ex; }
{ cyc+=4;
u32 r=(AD[1]|(AD[5]&0xFFFFu))&0xFFFFu;
NZL(0u,r,0xFFFFu,0x8000u);
AD[1]=(AD[1]&~0xFFFFu)|r; }
B494E: if((s32)(cyc-lim)>=0){ i=22; goto ex; }
{ cyc+=4;
u32 mv=(AD[1]&0xFFFFu);
u32 ea1=AA[2]; AA[2]+=2u;
cyc+=4; W16(ea1,mv);
NZL(0u,mv,0xFFFFu,0x8000u); }
B4950: if((s32)(cyc-lim)>=0){ i=23; goto ex; }
{ cyc+=4;
u32 ea0=AA[0];
cyc+=4; u32 r2=R8(ea0);
u32 mv=r2;
AD[0]=(AD[0]&~0xFFu)|((mv)&0xFFu);
NZL(0u,mv,0xFFu,0x80u); }
B4952: if((s32)(cyc-lim)>=0){ i=24; goto ex; }
{ cyc+=4;
{ u32 v=(u32)(s32)(s8)AD[0]; AD[0]=(AD[0]&0xFFFF0000u)|(v&0xFFFFu); NZL(0u,v,0xFFFFu,0x8000u); } }
B4954: if((s32)(cyc-lim)>=0){ i=25; goto ex; }
{ cyc+=4;
{ u32 r=ADDL(31u,(AD[2]&0xFFFFu),AD[0],0xFFFFu,0x8000u,16,0,1,0); AD[0]=(AD[0]&~0xFFFFu)|r; } }
B4956: if((s32)(cyc-lim)>=0){ i=26; goto ex; }
{ cyc+=4;
if(CC(sr,11)){ cyc+=2;  i=27; goto ex; } }
B4958: if((s32)(cyc-lim)>=0){ i=28; goto ex; }
{ cyc+=4;
CMPL(31u,0x60u,(AD[0]&0xFFFFu),0xFFFFu,0x8000u); }
B495C: if((s32)(cyc-lim)>=0){ i=29; goto ex; }
{ cyc+=4;
if(CC(sr,5)){ cyc+=2;  i=27; goto ex; } }
B495E: if((s32)(cyc-lim)>=0){ i=30; goto ex; }
{ cyc+=4;
CMPL(31u,0x1D0u,(AD[0]&0xFFFFu),0xFFFFu,0x8000u); }
B4962: if((s32)(cyc-lim)>=0){ i=31; goto ex; }
{ cyc+=4;
if(CC(sr,4)){ cyc+=2;  i=27; goto ex; } }
B4964: if((s32)(cyc-lim)>=0){ i=32; goto ex; }
{ cyc+=4;
u32 mv=(AD[0]&0xFFFFu);
u32 ea1=AA[2]; AA[2]+=2u;
cyc+=4; W16(ea1,mv);
NZL(16u,mv,0xFFFFu,0x8000u); }
B4966: if((s32)(cyc-lim)>=0){ i=33; goto ex; }
{ cyc+=4;
cyc+=4;
CMPL(31u,(u32)(s32)(s16)0x7Fu,AA[3],0xFFFFFFFFu,0x80000000u); }
B496A: if((s32)(cyc-lim)>=0){ i=34; goto ex; }
{ cyc+=4;
if(CC(sr,6)){ cyc+=2;  goto B4970; } }
i=35; goto ex;
B4970: if((s32)(cyc-lim)>=0){ i=36; goto ex; }
{ cyc+=4;
AA[3]+=1u; }
B4972: if((s32)(cyc-lim)>=0){ i=37; goto ex; }
{ cyc+=4;
{ u32 t=R32(AA[7]); AA[7]+=4; cyc+=8;
npc=t; goto out;
} }
B4976: if((s32)(cyc-lim)>=0){ i=38; goto ex; }
{ cyc+=4;
{ u32 t=R32(AA[7]); AA[7]+=4; cyc+=8;
npc=t; goto out;
} }
ex: npc=hi|xpc[i];
out: C.sr=(u16)sr; C.cycles=cyc; C.pc=npc; return rc;
}
u32 aotf112(u32 entry){
 static const u32 xpc[] AOT_TAB={0xFF000Eu,0xFF0010u,0xFF0012u,0xFF0014u};
 u32 sr=C.sr, cyc=C.cycles, npc, i, rc=AOT_CONT; const u32 lim=aot_lim, hi=C.pc&0xFF000000u; 
 switch(entry){ case 0: goto BFF000E; case 1: goto BFF0010; case 2: goto BFF0012; default: npc=C.pc; goto out; }
BFF000E: if((s32)(cyc-lim)>=0){ i=0; goto ex; }
{ cyc+=4;
u32 ea0=AA[0]; AA[0]+=2u;
cyc+=4; u32 r2=R16(ea0);
{ u32 r=ADDL(0u,r2,AD[1],0xFFFFu,0x8000u,16,0,1,0); AD[1]=(AD[1]&~0xFFFFu)|r; } }
BFF0010: if((s32)(cyc-lim)>=0){ i=1; goto ex; }
{ cyc+=4;
u32 r=SUBL(31u,2u,(AD[0]&0xFFFFu),0xFFFFu,0x8000u,0,1,0);
AD[0]=(AD[0]&~0xFFFFu)|((r)&0xFFFFu); }
BFF0012: if((s32)(cyc-lim)>=0){ i=2; goto ex; }
{ cyc+=4;
if(CC(sr,6)){ cyc+=2;  goto BFF000E; } }
i=3; goto ex;
ex: npc=hi|xpc[i];
out: C.sr=(u16)sr; C.cycles=cyc; C.pc=npc; return rc;
}
u32 aotf113(u32 entry){
 static const u32 xpc[] AOT_TAB={0xFF0070u,0xFF0076u,0xFF0078u,0xFF007Cu,0xFF0080u,0xFF0084u,0xFF0088u,0xFF008Cu};
 u32 sr=C.sr, cyc=C.cycles, npc, i, rc=AOT_CONT; const u32 lim=aot_lim, hi=C.pc&0xFF000000u; 
 switch(entry){ case 0: goto BFF0070; case 1: goto BFF0076; case 2: goto BFF007C; case 3: goto BFF0080; case 4: goto BFF0084; case 5: goto BFF0088; case 6: goto BFF008C; default: npc=C.pc; goto out; }
BFF0070: if((s32)(cyc-lim)>=0){ i=0; goto ex; }
{ cyc+=4;
cyc+=4;
cyc+=4; u32 r2=RAM16(0xFFE6u);
CMPL(31u,0x1235u,r2,0xFFFFu,0x8000u); }
BFF0076: if((s32)(cyc-lim)>=0){ i=1; goto ex; }
{ cyc+=4;
if(CC(sr,6)){ cyc+=2;  goto BFF007C; } }
i=2; goto ex;
BFF007C: if((s32)(cyc-lim)>=0){ i=3; goto ex; }
{ cyc+=4;
cyc+=4;
cyc+=4; u32 r2=RAM16(0xFFC0u);
u32 mv=r2;
AD[0]=(AD[0]&~0xFFFFu)|((mv)&0xFFFFu);
NZL(16u,mv,0xFFFFu,0x8000u); }
BFF0080: if((s32)(cyc-lim)>=0){ i=4; goto ex; }
{ cyc+=4;
u32 r=(AD[0]&0xFFFFu)&0xCu;
NZL(31u,r,0xFFFFu,0x8000u);
AD[0]=(AD[0]&~0xFFFFu)|((r)&0xFFFFu); }
BFF0084: if((s32)(cyc-lim)>=0){ i=5; goto ex; }
{ cyc+=4;
u32 ea0=(hi|0xFF0086u)+((u32)(s32)(s16)AD[0]+0x8u); cyc+=6;
AA[7]-=4; W32(AA[7],(hi|0xFF0088u));
cyc+=8;
npc=ea0; goto out; }
BFF0088: if((s32)(cyc-lim)>=0){ i=6; goto ex; }
{ cyc+=4;
cyc+=4;
cyc+=4; u32 r2=RAM16(0xFF92u);
u32 r=ADDL(16u,1u,r2,0xFFFFu,0x8000u,16,0,1,0);
cyc+=4; RAM16(0xFF92u)=(u16)(r);; }
BFF008C: if((s32)(cyc-lim)>=0){ i=7; goto ex; }
{ cyc+=4;
cyc+=2;

goto BFF0070; }
ex: npc=hi|xpc[i];
out: C.sr=(u16)sr; C.cycles=cyc; C.pc=npc; return rc;
}
u32 aotf114(u32 entry){
 static const u32 xpc[] AOT_TAB={0xFF009Au,0xFF04CCu,0xFF04D0u};
 u32 sr=C.sr, cyc=C.cycles, npc, i, rc=AOT_CONT; const u32 lim=aot_lim, hi=C.pc&0xFF000000u; 
 switch(entry){ case 0: goto BFF009A; case 1: goto BFF04CC; case 2: goto BFF04D0; default: npc=C.pc; goto out; }
BFF009A: if((s32)(cyc-lim)>=0){ i=0; goto ex; }
{ cyc+=4;
cyc+=2;

goto BFF04CC; }
BFF04CC: if((s32)(cyc-lim)>=0){ i=1; goto ex; }
{ cyc+=4;
cyc+=4;
cyc+=4; u32 r2=RAM16(0xFFACu);
u32 mv=r2;
AD[7]=(AD[7]&~0xFFFFu)|((mv)&0xFFFFu);
NZL(31u,mv,0xFFFFu,0x8000u); }
BFF04D0: if((s32)(cyc-lim)>=0){ i=2; goto ex; }
{ cyc+=4;
cyc+=4;
AA[7]-=4; W32(AA[7],(hi|0xFF04D4u));
cyc+=8;
npc=0xFFFFFD3Au; goto out; }
ex: npc=hi|xpc[i];
out: C.sr=(u16)sr; C.cycles=cyc; C.pc=npc; return rc;
}
u32 aotf115(u32 entry){
 static const u32 xpc[] AOT_TAB={0xFF00B4u,0xFF00B6u,0xFF00BAu};
 u32 sr=C.sr, cyc=C.cycles, npc, i, rc=AOT_CONT; const u32 lim=aot_lim, hi=C.pc&0xFF000000u; 
 switch(entry){ case 0: goto BFF00B4; case 1: goto BFF00B6; default: npc=C.pc; goto out; }
BFF00B4: if((s32)(cyc-lim)>=0){ i=0; goto ex; }
{ cyc+=4;
u32 mv=(AD[7]&0xFFFFFFFFu);
u32 ea1=AA[6]; AA[6]+=4u;
cyc+=8; W32(ea1,mv);
NZL(31u,mv,0xFFFFFFFFu,0x80000000u); }
BFF00B6: if((s32)(cyc-lim)>=0){ i=1; goto ex; }
{ cyc+=4;
if(!0){ u32 w=(AD[6]-1u)&0xFFFFu; AD[6]=(AD[6]&0xFFFF0000u)|w; if(w!=0xFFFFu){ goto BFF00B4; } } }
i=2; goto ex;
ex: npc=hi|xpc[i];
out: C.sr=(u16)sr; C.cycles=cyc; C.pc=npc; return rc;
}
u32 aotf116(u32 entry){
 static const u32 xpc[] AOT_TAB={0xFF00C2u,0xFF00C6u,0xFF00CCu,0xFF00D0u,0xFF00D6u,0xFF00D8u,0xFF00DCu};
 u32 sr=C.sr, cyc=C.cycles, npc, i, rc=AOT_CONT; const u32 lim=aot_lim, hi=C.pc&0xFF000000u; 
 switch(entry){ case 0: goto BFF00C2; case 1: goto BFF00C6; case 2: goto BFF00CC; case 3: goto BFF00D0; case 4: goto BFF00D6; case 5: goto BFF00D8; case 6: goto BFF00DC; default: npc=C.pc; goto out; }
BFF00C2: if((s32)(cyc-lim)>=0){ i=0; goto ex; }
{ cyc+=4;
u32 mv=(AA[3]&0xFFFFFFFFu);
cyc+=4;
cyc+=8; { u32 _w=(mv); RAM16(0xFFB0u)=(u16)(_w>>16); RAM16(0xFFB2u)=(u16)_w; };
NZL(16u,mv,0xFFFFFFFFu,0x80000000u); }
BFF00C6: if((s32)(cyc-lim)>=0){ i=1; goto ex; }
{ cyc+=4;
cyc+=4;
u32 mv=0x10u;
cyc+=4;
cyc+=4; RAM16(0xFFB4u)=(u16)(mv);;
NZL(16u,mv,0xFFFFu,0x8000u); }
BFF00CC: if((s32)(cyc-lim)>=0){ i=2; goto ex; }
{ cyc+=4;
cyc+=4;
cyc+=4; u32 r2=RAM16(0xFFB6u);
cyc+=4; RAM16(0xFFB6u)=(u16)(0);;
sr=(sr&~(FN_|FV_|FC_))|FZ_; }
BFF00D0: if((s32)(cyc-lim)>=0){ i=3; goto ex; }
{ cyc+=4;
cyc+=4;
u32 bn=(0u)&7u;
cyc+=4; u32 r3=RAM8(0xFFB3u);
if(r3&(1u<<bn)) sr&=~FZ_; else sr|=FZ_;
cyc+=4; RAM8(0xFFB3u)=(u8)(r3&~(1u<<bn));; }
BFF00D6: if((s32)(cyc-lim)>=0){ i=4; goto ex; }
{ cyc+=4;
if(CC(sr,6)){ cyc+=2;  goto BFF00DC; } }
BFF00D8: if((s32)(cyc-lim)>=0){ i=5; goto ex; }
{ cyc+=4;
cyc+=4;
cyc+=4; u32 r2=RAM16(0xFFB6u);
u32 r=ADDL(31u,1u,r2,0xFFFFu,0x8000u,16,0,1,0);
cyc+=4; RAM16(0xFFB6u)=(u16)(r);; }
BFF00DC: if((s32)(cyc-lim)>=0){ i=6; goto ex; }
{ cyc+=4;
{ u32 t=R32(AA[7]); AA[7]+=4; cyc+=8;
npc=t; goto out;
} }
ex: npc=hi|xpc[i];
out: C.sr=(u16)sr; C.cycles=cyc; C.pc=npc; return rc;
}
u32 aotf117(u32 entry){
 static const u32 xpc[] AOT_TAB={0xFF00E8u,0xFF00EAu,0xFF1BFCu,0xFF1C00u,0xFF1C04u,0xFF1C08u,0xFF1C0Cu,0xFF1C10u,0xFF1C14u};
 u32 sr=C.sr, cyc=C.cycles, npc, i, rc=AOT_CONT; const u32 lim=aot_lim, hi=C.pc&0xFF000000u; 
 switch(entry){ case 0: goto BFF00E8; case 1: goto BFF00EA; case 2: goto BFF1BFC; case 3: goto BFF1C00; case 4: goto BFF1C04; case 5: goto BFF1C08; case 6: goto BFF1C0C; case 7: goto BFF1C10; case 8: goto BFF1C14; default: npc=C.pc; goto out; }
BFF00E8: if((s32)(cyc-lim)>=0){ i=0; goto ex; }
{ cyc+=4;
AA[7]+=2u; }
BFF00EA: if((s32)(cyc-lim)>=0){ i=1; goto ex; }
{ cyc+=4;
cyc+=2;

goto BFF1BFC; }
BFF1BFC: if((s32)(cyc-lim)>=0){ i=2; goto ex; }
{ cyc+=4;
{ u32 a=AA[7];
u32 native_n=m68k_movem_ram(a,0x8040u,2u); if(native_n){a-=4u*native_n;cyc+=8u*native_n;}else{
a-=4; W32(a,AA[1]); cyc+=8;
a-=4; W32(a,AD[0]); cyc+=8;
}
AA[7]=a; } }
BFF1C00: if((s32)(cyc-lim)>=0){ i=3; goto ex; }
{ cyc+=4;
cyc+=4;
cyc+=4; u32 r2=RAM16(0xBF50u);
u32 mv=r2;
AD[0]=(AD[0]&~0xFFFFu)|((mv)&0xFFFFu);
NZL(0u,mv,0xFFFFu,0x8000u); }
BFF1C04: if((s32)(cyc-lim)>=0){ i=4; goto ex; }
{ cyc+=4;
cyc+=4;
AA[1]=0xFFFFBF52u; }
BFF1C08: if((s32)(cyc-lim)>=0){ i=5; goto ex; }
{ cyc+=4;
u32 mv=(AA[0]&0xFFFFu);
u32 ea1=AA[1]+((u32)(s32)(s16)AD[0]+0x0u); cyc+=6;
cyc+=4; W16(ea1,mv);
NZL(0u,mv,0xFFFFu,0x8000u); }
BFF1C0C: if((s32)(cyc-lim)>=0){ i=6; goto ex; }
{ cyc+=4;
cyc+=4;
cyc+=4; u32 r2=RAM16(0xBF50u);
u32 r=ADDL(31u,2u,r2,0xFFFFu,0x8000u,16,0,1,0);
cyc+=4; RAM16(0xBF50u)=(u16)(r);; }
BFF1C10: if((s32)(cyc-lim)>=0){ i=7; goto ex; }
{ cyc+=4;
{ u32 a=AA[7];
u32 native_n=m68k_movem_ram(a,0x201u,1u); if(native_n){a+=4u*native_n;cyc+=8u*native_n;}else{
AD[0]=R32(a); a+=4; cyc+=8;
AA[1]=R32(a); a+=4; cyc+=8;
}
AA[7]=a;
} }
BFF1C14: if((s32)(cyc-lim)>=0){ i=8; goto ex; }
{ cyc+=4;
{ u32 t=R32(AA[7]); AA[7]+=4; cyc+=8;
npc=t; goto out;
} }
ex: npc=hi|xpc[i];
out: C.sr=(u16)sr; C.cycles=cyc; C.pc=npc; return rc;
}
u32 aotf118(u32 entry){
 static const u32 xpc[] AOT_TAB={0xFF0322u,0xFF0324u,0xFF0328u,0xFF032Cu};
 u32 sr=C.sr, cyc=C.cycles, npc, i, rc=AOT_CONT; const u32 lim=aot_lim, hi=C.pc&0xFF000000u; 
 switch(entry){ case 0: goto BFF0322; case 1: goto BFF0324; case 2: goto BFF0328; default: npc=C.pc; goto out; }
BFF0322: if((s32)(cyc-lim)>=0){ i=0; goto ex; }
{ cyc+=4;
AD[0]=0xFFFFFF80u;
NZL(31u,0xFFFFFF80u,0xFFFFFFFFu,0x80000000u); }
BFF0324: if((s32)(cyc-lim)>=0){ i=1; goto ex; }
{ cyc+=4;
cyc+=4;
AA[7]-=4; W32(AA[7],(hi|0xFF0328u));
cyc+=8;
npc=0xFFFFFCB6u; goto out; }
BFF0328: if((s32)(cyc-lim)>=0){ i=2; goto ex; }
{ cyc+=4;
if(!CC(sr,7)){ u32 w=(AD[1]-1u)&0xFFFFu; AD[1]=(AD[1]&0xFFFF0000u)|w; if(w!=0xFFFFu){ goto BFF0322; } } }
i=3; goto ex;
ex: npc=hi|xpc[i];
out: C.sr=(u16)sr; C.cycles=cyc; C.pc=npc; return rc;
}
u32 aotf119(u32 entry){
 static const u32 xpc[] AOT_TAB={0xFF0412u,0xFF0414u,0xFF0416u,0xFF041Au};
 u32 sr=C.sr, cyc=C.cycles, npc, i, rc=AOT_CONT; const u32 lim=aot_lim, hi=C.pc&0xFF000000u; 
 switch(entry){ case 0: goto BFF0412; case 1: goto BFF0414; case 2: goto BFF0416; default: npc=C.pc; goto out; }
BFF0412: if((s32)(cyc-lim)>=0){ i=0; goto ex; }
{ cyc+=4;
u32 ea0=AA[0]; AA[0]+=1u;
cyc+=4; u32 r2=R8(ea0);
u32 mv=r2;
u32 ea1=AA[1]; AA[1]+=1u;
cyc+=4; W8(ea1,mv);
NZL(31u,mv,0xFFu,0x80u); }
BFF0414: if((s32)(cyc-lim)>=0){ i=1; goto ex; }
{ cyc+=4;
AA[0]+=1u; }
BFF0416: if((s32)(cyc-lim)>=0){ i=2; goto ex; }
{ cyc+=4;
if(!0){ u32 w=(AD[0]-1u)&0xFFFFu; AD[0]=(AD[0]&0xFFFF0000u)|w; if(w!=0xFFFFu){ goto BFF0412; } } }
i=3; goto ex;
ex: npc=hi|xpc[i];
out: C.sr=(u16)sr; C.cycles=cyc; C.pc=npc; return rc;
}
u32 aotf120(u32 entry){
 static const u32 xpc[] AOT_TAB={0xFF04D4u,0xFF04D8u,0xFF04DAu,0xFF04DCu,0xFF04E0u,0xFF04E2u,0xFF04E8u};
 u32 sr=C.sr, cyc=C.cycles, npc, i, rc=AOT_CONT; const u32 lim=aot_lim, hi=C.pc&0xFF000000u; 
 switch(entry){ case 0: goto BFF04D4; case 1: goto BFF04D8; case 2: goto BFF04DA; case 3: goto BFF04DC; case 4: goto BFF04E0; case 5: goto BFF04E2; case 6: goto BFF04E8; default: npc=C.pc; goto out; }
BFF04D4: if((s32)(cyc-lim)>=0){ i=0; goto ex; }
{ cyc+=4;
cyc+=4;
cyc+=4; u32 r2=RAM16(0xFFC2u);
u32 mv=r2;
AD[0]=(AD[0]&~0xFFFFu)|((mv)&0xFFFFu);
NZL(0u,mv,0xFFFFu,0x8000u); }
BFF04D8: if((s32)(cyc-lim)>=0){ i=1; goto ex; }
{ cyc+=4;
{ u32 r=aot_lshift_imm(&sr,&cyc,AD[0],1u,0xFFFFu,0x8000u,1u); AD[0]=(AD[0]&~0xFFFFu)|(r&0xFFFFu); } }
BFF04DA: if((s32)(cyc-lim)>=0){ i=2; goto ex; }
{ cyc+=4;
AD[1]=0xFFFFFFFFu;
NZL(16u,0xFFFFFFFFu,0xFFFFFFFFu,0x80000000u); }
BFF04DC: if((s32)(cyc-lim)>=0){ i=3; goto ex; }
{ cyc+=4;
u32 ea0=(hi|0xFF04DEu)+((u32)(s32)(s16)AD[0]+0x10u); cyc+=6;
cyc+=4; u32 r2=R16(ea0);
u32 mv=r2;
AD[1]=(AD[1]&~0xFFFFu)|((mv)&0xFFFFu);
NZL(16u,mv,0xFFFFu,0x8000u); }
BFF04E0: if((s32)(cyc-lim)>=0){ i=4; goto ex; }
{ cyc+=4;
AA[0]=(AD[1]&0xFFFFFFFFu); }
BFF04E2: if((s32)(cyc-lim)>=0){ i=5; goto ex; }
{ cyc+=4;
cyc+=4;
cyc+=4; u32 r2=RAM16(0xFFC2u);
u32 r=r2|0x1u;
NZL(31u,r,0xFFFFu,0x8000u);
cyc+=4; RAM16(0xFFC2u)=(u16)(r);; }
BFF04E8: if((s32)(cyc-lim)>=0){ i=6; goto ex; }
{ cyc+=4;
u32 ea0=AA[0];
AA[7]-=4; W32(AA[7],(hi|0xFF04EAu));
cyc+=8;
npc=ea0; goto out; }
ex: npc=hi|xpc[i];
out: C.sr=(u16)sr; C.cycles=cyc; C.pc=npc; return rc;
}
u32 aotf121(u32 entry){
 static const u32 xpc[] AOT_TAB={0xFF04EAu,0xFF1118u,0xFF111Cu,0xFF1120u,0xFF1124u,0xFF1128u,0xFF112Au,0xFF112Cu};
 u32 sr=C.sr, cyc=C.cycles, npc, i, rc=AOT_CONT; const u32 lim=aot_lim, hi=C.pc&0xFF000000u; 
 switch(entry){ case 0: goto BFF04EA; case 1: goto BFF1118; case 2: goto BFF111C; case 3: goto BFF1120; case 4: goto BFF1124; case 5: goto BFF1128; case 6: goto BFF112A; case 7: goto BFF112C; default: npc=C.pc; goto out; }
BFF04EA: if((s32)(cyc-lim)>=0){ i=0; goto ex; }
{ cyc+=4;
cyc+=2;

goto BFF1118; }
BFF1118: if((s32)(cyc-lim)>=0){ i=1; goto ex; }
{ cyc+=4;
cyc+=4;
u32 mv=0x170u;
AD[2]=(AD[2]&~0xFFFFu)|((mv)&0xFFFFu);
NZL(0u,mv,0xFFFFu,0x8000u); }
BFF111C: if((s32)(cyc-lim)>=0){ i=2; goto ex; }
{ cyc+=4;
cyc+=4;
cyc+=4; u32 r2=RAM16(0xBF4Au);
u32 mv=r2;
AD[3]=(AD[3]&~0xFFFFu)|((mv)&0xFFFFu);
NZL(0u,mv,0xFFFFu,0x8000u); }
BFF1120: if((s32)(cyc-lim)>=0){ i=3; goto ex; }
{ cyc+=4;
cyc+=4;
{ u32 r=(AD[3]&0xFFFFu)*((0x12u)&0xFFFFu);
AD[3]=r; NZL(0u,r,0xFFFFFFFFu,0x80000000u); cyc+=50; } }
BFF1124: if((s32)(cyc-lim)>=0){ i=4; goto ex; }
{ cyc+=4;
u32 r=ADDL(16u,0x98u,(AD[3]&0xFFFFu),0xFFFFu,0x8000u,16,0,1,0);
AD[3]=(AD[3]&~0xFFFFu)|((r)&0xFFFFu); }
BFF1128: if((s32)(cyc-lim)>=0){ i=5; goto ex; }
{ cyc+=4;
AD[4]=0x7u;
NZL(16u,0x7u,0xFFFFFFFFu,0x80000000u); }
BFF112A: if((s32)(cyc-lim)>=0){ i=6; goto ex; }
{ cyc+=4;
AD[5]=0x0u;
NZL(31u,0x0u,0xFFFFFFFFu,0x80000000u); }
BFF112C: if((s32)(cyc-lim)>=0){ i=7; goto ex; }
{ cyc+=4;
cyc+=4;
AA[7]-=4; W32(AA[7],(hi|0xFF1130u));
cyc+=8;
npc=0xFFFFFD40u; goto out; }
ex: npc=hi|xpc[i];
out: C.sr=(u16)sr; C.cycles=cyc; C.pc=npc; return rc;
}
u32 aotf122(u32 entry){
 static const u32 xpc[] AOT_TAB={0xFF081Cu,0xFF081Eu,0xFF0820u,0xFF087Cu,0xFF087Eu,0xFF0882u};
 u32 sr=C.sr, cyc=C.cycles, npc, i, rc=AOT_CONT; const u32 lim=aot_lim, hi=C.pc&0xFF000000u; 
 switch(entry){ case 0: goto BFF081C; case 1: goto BFF081E; case 2: goto BFF087C; case 3: goto BFF087E; default: npc=C.pc; goto out; }
BFF081C: if((s32)(cyc-lim)>=0){ i=0; goto ex; }
{ cyc+=4;
u32 ea0=AA[2]; AA[2]+=2u;
cyc+=4; u32 r2=R16(ea0);
u32 mv=r2;
AD[0]=(AD[0]&~0xFFFFu)|((mv)&0xFFFFu);
NZL(31u,mv,0xFFFFu,0x8000u); }
BFF081E: if((s32)(cyc-lim)>=0){ i=1; goto ex; }
{ cyc+=4;
if(CC(sr,11)){ cyc+=2;  goto BFF087C; } }
i=2; goto ex;
BFF087C: if((s32)(cyc-lim)>=0){ i=3; goto ex; }
{ cyc+=4;
AA[2]+=2u; }
BFF087E: if((s32)(cyc-lim)>=0){ i=4; goto ex; }
{ cyc+=4;
if(!0){ u32 w=(AD[7]-1u)&0xFFFFu; AD[7]=(AD[7]&0xFFFF0000u)|w; if(w!=0xFFFFu){ goto BFF081C; } } }
i=5; goto ex;
ex: npc=hi|xpc[i];
out: C.sr=(u16)sr; C.cycles=cyc; C.pc=npc; return rc;
}
u32 aotf123(u32 entry){
 static const u32 xpc[] AOT_TAB={0xFF095Cu,0xFF095Eu,0xFF0962u,0xFF0964u,0xFF096Au,0xFF096Eu,0xFF0970u,0xFF0976u,0xFF097Au,0xFF097Cu,0xFF0990u,0xFF0994u,0xFF0996u,0xFF0998u,0xFF099Au,0xFF099Cu,0xFF09A2u,0xFF09A6u,0xFF09A8u,0xFF09B0u,0xFF09B4u,0xFF09B6u,0xFF09B8u,0xFF09BAu,0xFF09BCu,0xFF09BEu,0xFF09C2u,0xFF09C8u};
 u32 sr=C.sr, cyc=C.cycles, npc, i, rc=AOT_CONT; const u32 lim=aot_lim, hi=C.pc&0xFF000000u; 
 switch(entry){ case 0: goto BFF095C; case 1: goto BFF095E; case 2: goto BFF0962; case 3: goto BFF096A; case 4: goto BFF096E; case 5: goto BFF0976; case 6: goto BFF097A; case 7: goto BFF0990; case 8: goto BFF0994; case 9: goto BFF0996; case 10: goto BFF0998; case 11: goto BFF099A; case 12: goto BFF09A2; case 13: goto BFF09A6; case 14: goto BFF09B0; case 15: goto BFF09B4; case 16: goto BFF09B6; case 17: goto BFF09B8; case 18: goto BFF09BA; case 19: goto BFF09BC; case 20: goto BFF09BE; case 21: goto BFF09C2; case 22: goto BFF09C8; default: npc=C.pc; goto out; }
BFF095C: if((s32)(cyc-lim)>=0){ i=0; goto ex; }
{ cyc+=4;
AD[6]=0x1Cu;
NZL(16u,0x1Cu,0xFFFFFFFFu,0x80000000u); }
BFF095E: if((s32)(cyc-lim)>=0){ i=1; goto ex; }
{ cyc+=4;
CMPL(31u,0x40u,(AD[7]&0xFFFFu),0xFFFFu,0x8000u); }
BFF0962: if((s32)(cyc-lim)>=0){ i=2; goto ex; }
{ cyc+=4;
if(CC(sr,4)){ cyc+=2;  goto BFF096A; } }
i=3; goto ex;
BFF096A: if((s32)(cyc-lim)>=0){ i=4; goto ex; }
{ cyc+=4;
cyc+=4;
cyc+=4; u32 r2=RAM8(0xFF8Du);
NZL(31u,r2,0xFFu,0x80u); }
BFF096E: if((s32)(cyc-lim)>=0){ i=5; goto ex; }
{ cyc+=4;
if(CC(sr,7)){ cyc+=2;  goto BFF0976; } }
i=6; goto ex;
BFF0976: if((s32)(cyc-lim)>=0){ i=7; goto ex; }
{ cyc+=4;
cyc+=4;
cyc+=4; u32 r2=RAM16(0xBF4Cu);
CMPL(31u,r2,AD[6],0xFFFFu,0x8000u); }
BFF097A: if((s32)(cyc-lim)>=0){ i=8; goto ex; }
{ cyc+=4;
if(CC(sr,7)){ cyc+=2;  goto BFF0990; } }
i=9; goto ex;
BFF0990: if((s32)(cyc-lim)>=0){ i=10; goto ex; }
{ cyc+=4;
cyc+=4;
cyc+=4; u32 r2=RAM8(0xFF8Eu);
u32 mv=r2;
AD[0]=(AD[0]&~0xFFu)|((mv)&0xFFu);
NZL(16u,mv,0xFFu,0x80u); }
BFF0994: if((s32)(cyc-lim)>=0){ i=11; goto ex; }
{ cyc+=4;
AD[1]=0x70u;
NZL(16u,0x70u,0xFFFFFFFFu,0x80000000u); }
BFF0996: if((s32)(cyc-lim)>=0){ i=12; goto ex; }
{ cyc+=4;
u32 r=(AD[0]&(AD[1]&0xFFu))&0xFFu;
NZL(16u,r,0xFFu,0x80u);
AD[0]=(AD[0]&~0xFFu)|r; }
BFF0998: if((s32)(cyc-lim)>=0){ i=13; goto ex; }
{ cyc+=4;
CMPL(31u,(AD[1]&0xFFu),AD[0],0xFFu,0x80u); }
BFF099A: if((s32)(cyc-lim)>=0){ i=14; goto ex; }
{ cyc+=4;
if(CC(sr,6)){ cyc+=2;  goto BFF09A2; } }
i=15; goto ex;
BFF09A2: if((s32)(cyc-lim)>=0){ i=16; goto ex; }
{ cyc+=4;
cyc+=4;
cyc+=4; u32 r2=RAM8(0xFF8Fu);
NZL(31u,r2,0xFFu,0x80u); }
BFF09A6: if((s32)(cyc-lim)>=0){ i=17; goto ex; }
{ cyc+=4;
if(CC(sr,10)){ cyc+=2;  goto BFF09B0; } }
i=18; goto ex;
BFF09B0: if((s32)(cyc-lim)>=0){ i=19; goto ex; }
{ cyc+=4;
cyc+=4;
cyc+=4; u32 r2=RAM16(0xBF4Cu);
u32 mv=r2;
AD[0]=(AD[0]&~0xFFFFu)|((mv)&0xFFFFu);
NZL(16u,mv,0xFFFFu,0x8000u); }
BFF09B4: if((s32)(cyc-lim)>=0){ i=20; goto ex; }
{ cyc+=4;
CMPL(17u,(AD[6]&0xFFFFu),AD[0],0xFFFFu,0x8000u); }
BFF09B6: if((s32)(cyc-lim)>=0){ i=21; goto ex; }
{ cyc+=4;
if(CC(sr,5)){ cyc+=2;  goto BFF09BE; } }
BFF09B8: if((s32)(cyc-lim)>=0){ i=22; goto ex; }
{ cyc+=4;
{ u32 r=SUBL(0u,(AD[6]&0xFFFFu),AD[0],0xFFFFu,0x8000u,0,1,0); AD[0]=(AD[0]&~0xFFFFu)|r; } }
BFF09BA: if((s32)(cyc-lim)>=0){ i=23; goto ex; }
{ cyc+=4;
u32 r=SUBL(0u,(AD[0]&0xFFFFu),0,0xFFFFu,0x8000u,0,1,0);
AD[0]=(AD[0]&~0xFFFFu)|((r)&0xFFFFu); }
BFF09BC: if((s32)(cyc-lim)>=0){ i=24; goto ex; }
{ cyc+=4;
{ u32 r=ADDL(16u,(AD[6]&0xFFFFu),AD[0],0xFFFFu,0x8000u,16,0,1,0); AD[0]=(AD[0]&~0xFFFFu)|r; } }
BFF09BE: if((s32)(cyc-lim)>=0){ i=25; goto ex; }
{ cyc+=4;
u32 r=(AD[0]&0xFFFFu)|0x9200u;
NZL(16u,r,0xFFFFu,0x8000u);
AD[0]=(AD[0]&~0xFFFFu)|((r)&0xFFFFu); }
BFF09C2: if((s32)(cyc-lim)>=0){ i=26; goto ex; }
{ cyc+=4;
u32 mv=(AD[0]&0xFFFFu);
cyc+=8;
cyc+=4; W16(0xC00004u,mv);
NZL(31u,mv,0xFFFFu,0x8000u); }
BFF09C8: if((s32)(cyc-lim)>=0){ i=27; goto ex; }
{ cyc+=4;
{ u32 t=R32(AA[7]); AA[7]+=4; cyc+=8;
npc=t; goto out;
} }
ex: npc=hi|xpc[i];
out: C.sr=(u16)sr; C.cycles=cyc; C.pc=npc; return rc;
}
u32 aotf124(u32 entry){
 static const u32 xpc[] AOT_TAB={0xFF09CAu,0xFF09CEu,0xFF09D0u,0xFF09E4u,0xFF09E8u,0xFF09DEu,0xFF09EAu,0xFF09EEu,0xFF09F0u,0xFF09F4u,0xFF09F6u,0xFF09FAu,0xFF09FCu,0xFF0A02u,0xFF0A06u,0xFF0A08u,0xFF0A10u};
 u32 sr=C.sr, cyc=C.cycles, npc, i, rc=AOT_CONT; const u32 lim=aot_lim, hi=C.pc&0xFF000000u; 
 switch(entry){ case 0: goto BFF09CA; case 1: goto BFF09CE; case 2: goto BFF09E4; case 3: goto BFF09E8; case 4: goto BFF09EA; case 5: goto BFF09EE; case 6: goto BFF09F0; case 7: goto BFF09F4; case 8: goto BFF09F6; case 9: goto BFF09FA; case 10: goto BFF0A02; case 11: goto BFF0A06; case 12: goto BFF0A10; default: npc=C.pc; goto out; }
BFF09CA: if((s32)(cyc-lim)>=0){ i=0; goto ex; }
{ cyc+=4;
CMPL(31u,0x40u,(AD[7]&0xFFFFu),0xFFFFu,0x8000u); }
BFF09CE: if((s32)(cyc-lim)>=0){ i=1; goto ex; }
{ cyc+=4;
if(CC(sr,7)){ cyc+=2;  goto BFF09E4; } }
i=2; goto ex;
BFF09E4: if((s32)(cyc-lim)>=0){ i=3; goto ex; }
{ cyc+=4;
cyc+=4;
cyc+=4; u32 r2=RAM16(0xACC8u);
NZL(31u,r2,0xFFFFu,0x8000u); }
BFF09E8: if((s32)(cyc-lim)>=0){ i=4; goto ex; }
{ cyc+=4;
if(CC(sr,6)){ cyc+=2;  i=5; goto ex; } }
BFF09EA: if((s32)(cyc-lim)>=0){ i=6; goto ex; }
{ cyc+=4;
cyc+=4;
cyc+=4; u32 r2=RAM16(0xAC60u);
NZL(20u,r2,0xFFFFu,0x8000u); }
BFF09EE: if((s32)(cyc-lim)>=0){ i=7; goto ex; }
{ cyc+=4;
if(CC(sr,6)){ cyc+=2;  goto BFF09F6; } }
BFF09F0: if((s32)(cyc-lim)>=0){ i=8; goto ex; }
{ cyc+=4;
cyc+=4;
cyc+=4; u32 r2=RAM16(0xC00Cu);
NZL(31u,r2,0xFFFFu,0x8000u); }
BFF09F4: if((s32)(cyc-lim)>=0){ i=9; goto ex; }
{ cyc+=4;
if(CC(sr,7)){ cyc+=2;  i=5; goto ex; } }
BFF09F6: if((s32)(cyc-lim)>=0){ i=10; goto ex; }
{ cyc+=4;
cyc+=4;
cyc+=4; u32 r2=RAM16(0xBF4Eu);
NZL(31u,r2,0xFFFFu,0x8000u); }
BFF09FA: if((s32)(cyc-lim)>=0){ i=11; goto ex; }
{ cyc+=4;
if(CC(sr,7)){ cyc+=2;  goto BFF0A02; } }
i=12; goto ex;
BFF0A02: if((s32)(cyc-lim)>=0){ i=13; goto ex; }
{ cyc+=4;
cyc+=4;
cyc+=4; u32 r2=RAM8(0xFF8Fu);
NZL(31u,r2,0xFFu,0x80u); }
BFF0A06: if((s32)(cyc-lim)>=0){ i=14; goto ex; }
{ cyc+=4;
if(CC(sr,10)){ cyc+=2;  goto BFF0A10; } }
i=15; goto ex;
BFF0A10: if((s32)(cyc-lim)>=0){ i=16; goto ex; }
{ cyc+=4;
cyc+=4;
AA[7]-=4; W32(AA[7],(hi|0xFF0A14u));
cyc+=8;
npc=0xFFFFFB5Au; goto out; }
ex: npc=hi|xpc[i];
out: C.sr=(u16)sr; C.cycles=cyc; C.pc=npc; return rc;
}
u32 aotf125(u32 entry){
 static const u32 xpc[] AOT_TAB={0xFF0A14u,0xFF0A1Au,0xFF0A1Eu,0xFF0A22u,0xFF0ABEu};
 u32 sr=C.sr, cyc=C.cycles, npc, i, rc=AOT_CONT; const u32 lim=aot_lim, hi=C.pc&0xFF000000u; 
 switch(entry){ case 0: goto BFF0A14; case 1: goto BFF0A1A; case 2: goto BFF0A1E; case 3: goto BFF0ABE; default: npc=C.pc; goto out; }
BFF0A14: if((s32)(cyc-lim)>=0){ i=0; goto ex; }
{ cyc+=4;
cyc+=4;
cyc+=4; u32 r2=RAM16(0xC372u);
u32 mv=r2;
cyc+=4;
cyc+=4; RAM16(0xBF4Au)=(u16)(mv);;
NZL(16u,mv,0xFFFFu,0x8000u); }
BFF0A1A: if((s32)(cyc-lim)>=0){ i=1; goto ex; }
{ cyc+=4;
cyc+=4;
cyc+=4; u32 r2=RAM8(0xFF8Au);
NZL(31u,r2,0xFFu,0x80u); }
BFF0A1E: if((s32)(cyc-lim)>=0){ i=2; goto ex; }
{ cyc+=4;
if(CC(sr,7)){ cyc+=2;  goto BFF0ABE; } }
i=3; goto ex;
BFF0ABE: if((s32)(cyc-lim)>=0){ i=4; goto ex; }
{ cyc+=4;
{ u32 t=R32(AA[7]); AA[7]+=4; cyc+=8;
npc=t; goto out;
} }
ex: npc=hi|xpc[i];
out: C.sr=(u16)sr; C.cycles=cyc; C.pc=npc; return rc;
}
u32 aotf126(u32 entry){
 static const u32 xpc[] AOT_TAB={0xFF0AC0u,0xFF134Cu};
 u32 sr=C.sr, cyc=C.cycles, npc, i, rc=AOT_CONT; const u32 lim=aot_lim, hi=C.pc&0xFF000000u; 
 switch(entry){ case 0: goto BFF0AC0; default: npc=C.pc; goto out; }
BFF0AC0: if((s32)(cyc-lim)>=0){ i=0; goto ex; }
{ cyc+=4;
AA[7]-=4; W32(AA[7],(hi|0xFF0AC4u));
cyc+=8;
i=1; goto ex; }
ex: npc=hi|xpc[i];
out: C.sr=(u16)sr; C.cycles=cyc; C.pc=npc; return rc;
}
u32 aotf127(u32 entry){
 static const u32 xpc[] AOT_TAB={0xFF0AC4u,0xFF0AC8u,0xFF0ACAu,0xFF0ADEu,0xFF0AE4u,0xFF0AE6u,0xFF0AF8u,0xFF0AFCu,0xFF0AFEu,0xFF0B16u,0xFF0B1Au,0xFF0B02u,0xFF0B1Cu,0xFF0B20u,0xFF0B22u,0xFF0B36u,0xFF0B3Au,0xFF0B3Eu,0xFF0B42u};
 u32 sr=C.sr, cyc=C.cycles, npc, i, rc=AOT_CONT; const u32 lim=aot_lim, hi=C.pc&0xFF000000u; 
 switch(entry){ case 0: goto BFF0AC4; case 1: goto BFF0AC8; case 2: goto BFF0ADE; case 3: goto BFF0AE4; case 4: goto BFF0AF8; case 5: goto BFF0AFC; case 6: goto BFF0B16; case 7: goto BFF0B1A; case 8: goto BFF0B1C; case 9: goto BFF0B20; case 10: goto BFF0B36; case 11: goto BFF0B3A; case 12: goto BFF0B3E; default: npc=C.pc; goto out; }
BFF0AC4: if((s32)(cyc-lim)>=0){ i=0; goto ex; }
{ cyc+=4;
CMPL(31u,0x60u,(AD[7]&0xFFFFu),0xFFFFu,0x8000u); }
BFF0AC8: if((s32)(cyc-lim)>=0){ i=1; goto ex; }
{ cyc+=4;
if(CC(sr,7)){ cyc+=2;  goto BFF0ADE; } }
i=2; goto ex;
BFF0ADE: if((s32)(cyc-lim)>=0){ i=3; goto ex; }
{ cyc+=4;
cyc+=4;
cyc+=4; u32 r2=RAM16(0xBF4Cu);
CMPL(31u,0x8000u,r2,0xFFFFu,0x8000u); }
BFF0AE4: if((s32)(cyc-lim)>=0){ i=4; goto ex; }
{ cyc+=4;
if(CC(sr,7)){ cyc+=2;  goto BFF0AF8; } }
i=5; goto ex;
BFF0AF8: if((s32)(cyc-lim)>=0){ i=6; goto ex; }
{ cyc+=4;
cyc+=4;
cyc+=4; u32 r2=RAM8(0xFF8Fu);
NZL(31u,r2,0xFFu,0x80u); }
BFF0AFC: if((s32)(cyc-lim)>=0){ i=7; goto ex; }
{ cyc+=4;
if(CC(sr,10)){ cyc+=2;  goto BFF0B16; } }
i=8; goto ex;
BFF0B16: if((s32)(cyc-lim)>=0){ i=9; goto ex; }
{ cyc+=4;
cyc+=4;
cyc+=4; u32 r2=RAM8(0xFF8Cu);
NZL(31u,r2,0xFFu,0x80u); }
BFF0B1A: if((s32)(cyc-lim)>=0){ i=10; goto ex; }
{ cyc+=4;
if(CC(sr,6)){ cyc+=2;  i=11; goto ex; } }
BFF0B1C: if((s32)(cyc-lim)>=0){ i=12; goto ex; }
{ cyc+=4;
cyc+=4;
cyc+=4; u32 r2=RAM8(0xFF8Au);
NZL(31u,r2,0xFFu,0x80u); }
BFF0B20: if((s32)(cyc-lim)>=0){ i=13; goto ex; }
{ cyc+=4;
if(CC(sr,7)){ cyc+=2;  goto BFF0B36; } }
i=14; goto ex;
BFF0B36: if((s32)(cyc-lim)>=0){ i=15; goto ex; }
{ cyc+=4;
cyc+=4;
cyc+=4; u32 r2=RAM8(0xFF8Eu);
u32 mv=r2;
AD[0]=(AD[0]&~0xFFu)|((mv)&0xFFu);
NZL(16u,mv,0xFFu,0x80u); }
BFF0B3A: if((s32)(cyc-lim)>=0){ i=16; goto ex; }
{ cyc+=4;
cyc+=4;
cyc+=4; u32 r2=RAM16(0xBF92u);
u32 mv=r2;
AD[1]=(AD[1]&~0xFFFFu)|((mv)&0xFFFFu);
NZL(16u,mv,0xFFFFu,0x8000u); }
BFF0B3E: if((s32)(cyc-lim)>=0){ i=17; goto ex; }
{ cyc+=4;
cyc+=4;
cyc+=4; u32 r2=RAM16(0xBF48u);
u32 mv=r2;
AD[2]=(AD[2]&~0xFFFFu)|((mv)&0xFFFFu);
NZL(31u,mv,0xFFFFu,0x8000u); }
i=18; goto ex;
ex: npc=hi|xpc[i];
out: C.sr=(u16)sr; C.cycles=cyc; C.pc=npc; return rc;
}
u32 aotf128(u32 entry){
 static const u32 xpc[] AOT_TAB={0xFF0BAAu,0xFF1280u,0xFF1282u,0xFF1286u,0xFF1288u,0xFF128Cu,0xFF1292u,0xFF1296u,0xFF129Au,0xFF129Cu,0xFF129Eu,0xFF12A0u,0xFF12A2u,0xFF12A4u,0xFF12A6u,0xFF12A8u,0xFF12AAu,0xFF12ACu,0xFF12B0u,0xFF12B2u,0xFF12B4u,0xFF12B6u,0xFF12B8u,0xFF12BAu,0xFF12BEu,0xFF12C0u,0xFF12C4u,0xFF12C8u,0xFF12CCu,0xFF12CEu,0xFF12D4u,0xFF12D6u,0xFF12DAu,0xFF12DCu,0xFF12DEu,0xFF12E0u,0xFF12E2u,0xFF12E6u,0xFF12E8u,0xFF12EEu,0xFF12F0u,0xFF12F4u,0xFF12FCu,0xFF12F6u,0xFF12FAu,0xFF1312u,0xFF1316u,0xFF131Au,0xFF131Cu,0xFF1320u,0xFF1324u};
 u32 sr=C.sr, cyc=C.cycles, npc, i, rc=AOT_CONT; const u32 lim=aot_lim, hi=C.pc&0xFF000000u; 
 switch(entry){ case 0: goto BFF0BAA; case 1: goto BFF1280; case 2: goto BFF1282; case 3: goto BFF1286; case 4: goto BFF1288; case 5: goto BFF128C; case 6: goto BFF1292; case 7: goto BFF1296; case 8: goto BFF129A; case 9: goto BFF129C; case 10: goto BFF129E; case 11: goto BFF12A0; case 12: goto BFF12A2; case 13: goto BFF12A4; case 14: goto BFF12A6; case 15: goto BFF12A8; case 16: goto BFF12AA; case 17: goto BFF12AC; case 18: goto BFF12B0; case 19: goto BFF12B2; case 20: goto BFF12B4; case 21: goto BFF12B6; case 22: goto BFF12B8; case 23: goto BFF12BA; case 24: goto BFF12BE; case 25: goto BFF12C0; case 26: goto BFF12C4; case 27: goto BFF12C8; case 28: goto BFF12CC; case 29: goto BFF12CE; case 30: goto BFF12D4; case 31: goto BFF12D6; case 32: goto BFF12DA; case 33: goto BFF12DC; case 34: goto BFF12DE; case 35: goto BFF12E2; case 36: goto BFF12E6; case 37: goto BFF12E8; case 38: goto BFF12EE; case 39: goto BFF12F0; case 40: goto BFF12F4; case 41: goto BFF12F6; case 42: goto BFF12FA; case 43: goto BFF1312; case 44: goto BFF1316; case 45: goto BFF131A; case 46: goto BFF131C; case 47: goto BFF1320; case 48: goto BFF1324; default: npc=C.pc; goto out; }
BFF0BAA: if((s32)(cyc-lim)>=0){ i=0; goto ex; }
{ cyc+=4;
cyc+=2;

goto BFF1280; }
BFF1280: if((s32)(cyc-lim)>=0){ i=1; goto ex; }
{ cyc+=4;
AD[7]=0x0u;
NZL(16u,0x0u,0xFFFFFFFFu,0x80000000u); }
BFF1282: if((s32)(cyc-lim)>=0){ i=2; goto ex; }
{ cyc+=4;
cyc+=4;
AA[4]=0xFFFFAA2Eu; }
BFF1286: if((s32)(cyc-lim)>=0){ i=3; goto ex; }
{ cyc+=4;
u32 ea0=AA[4];
cyc+=4; u32 r2=R16(ea0);
NZL(20u,r2,0xFFFFu,0x8000u); }
BFF1288: if((s32)(cyc-lim)>=0){ i=4; goto ex; }
{ cyc+=4;
if(CC(sr,7)){ cyc+=2;  goto BFF1316; } }
BFF128C: if((s32)(cyc-lim)>=0){ i=5; goto ex; }
{ cyc+=4;
u32 ea0=AA[4]+0x2u; cyc+=4;
{ u32 a=ea0;
AD[0]=(u32)(s32)(s16)R16(a); a+=2; cyc+=4;
AD[1]=(u32)(s32)(s16)R16(a); a+=2; cyc+=4;
AD[2]=(u32)(s32)(s16)R16(a); a+=2; cyc+=4;
AD[3]=(u32)(s32)(s16)R16(a); a+=2; cyc+=4;
} }
BFF1292: if((s32)(cyc-lim)>=0){ i=6; goto ex; }
{ cyc+=4;
cyc+=4;
cyc+=4; u32 r2=RAM16(0xBF4Cu);
u32 mv=r2;
AD[6]=(AD[6]&~0xFFFFu)|((mv)&0xFFFFu);
NZL(16u,mv,0xFFFFu,0x8000u); }
BFF1296: if((s32)(cyc-lim)>=0){ i=7; goto ex; }
{ cyc+=4;
CMPL(21u,0x8000u,(AD[6]&0xFFFFu),0xFFFFu,0x8000u); }
BFF129A: if((s32)(cyc-lim)>=0){ i=8; goto ex; }
{ cyc+=4;
if(CC(sr,7)){ cyc+=2;  goto BFF12C0; } }
BFF129C: if((s32)(cyc-lim)>=0){ i=9; goto ex; }
{ cyc+=4;
if(CC(sr,5)){ cyc+=2;  goto BFF12A0; } }
BFF129E: if((s32)(cyc-lim)>=0){ i=10; goto ex; }
{ cyc+=4;
u32 r=SUBL(0u,(AD[6]&0xFFFFu),0,0xFFFFu,0x8000u,0,1,0);
AD[6]=(AD[6]&~0xFFFFu)|((r)&0xFFFFu); }
BFF12A0: if((s32)(cyc-lim)>=0){ i=11; goto ex; }
{ cyc+=4;
{ u32 r=SUBL(0u,(AD[0]&0xFFFFu),AD[2],0xFFFFu,0x8000u,0,1,0); AD[2]=(AD[2]&~0xFFFFu)|r; } }
BFF12A2: if((s32)(cyc-lim)>=0){ i=12; goto ex; }
{ cyc+=4;
{ u32 r=SUBL(0u,(AD[1]&0xFFFFu),AD[3],0xFFFFu,0x8000u,0,1,0); AD[3]=(AD[3]&~0xFFFFu)|r; } }
BFF12A4: if((s32)(cyc-lim)>=0){ i=13; goto ex; }
{ cyc+=4;
{ u32 r=(u32)((s32)(s16)AD[2]*(s32)(s16)((AD[6]&0xFFFFu)));
AD[2]=r; NZL(0u,r,0xFFFFFFFFu,0x80000000u); cyc+=50; } }
BFF12A6: if((s32)(cyc-lim)>=0){ i=14; goto ex; }
{ cyc+=4;
{ u32 r=ADDL(0u,(AD[2]&0xFFFFFFFFu),AD[2],0xFFFFFFFFu,0x80000000u,32,0,1,0); AD[2]=(AD[2]&~0xFFFFFFFFu)|r; } }
BFF12A8: if((s32)(cyc-lim)>=0){ i=15; goto ex; }
{ cyc+=4;
{ u32 v=AD[2]; v=(v>>16)|(v<<16); AD[2]=v; NZL(0u,v,0xFFFFFFFFu,0x80000000u); } }
BFF12AA: if((s32)(cyc-lim)>=0){ i=16; goto ex; }
{ cyc+=4;
{ u32 r=ADDL(0u,(AD[0]&0xFFFFu),AD[2],0xFFFFu,0x8000u,16,0,1,0); AD[2]=(AD[2]&~0xFFFFu)|r; } }
BFF12AC: if((s32)(cyc-lim)>=0){ i=17; goto ex; }
{ cyc+=4;
CMPL(1u,0x200u,(AD[2]&0xFFFFu),0xFFFFu,0x8000u); }
BFF12B0: if((s32)(cyc-lim)>=0){ i=18; goto ex; }
{ cyc+=4;
if(CC(sr,4)){ cyc+=2;  goto BFF1316; } }
BFF12B2: if((s32)(cyc-lim)>=0){ i=19; goto ex; }
{ cyc+=4;
{ u32 r=(u32)((s32)(s16)AD[3]*(s32)(s16)((AD[6]&0xFFFFu)));
AD[3]=r; NZL(0u,r,0xFFFFFFFFu,0x80000000u); cyc+=50; } }
BFF12B4: if((s32)(cyc-lim)>=0){ i=20; goto ex; }
{ cyc+=4;
{ u32 r=ADDL(0u,(AD[3]&0xFFFFFFFFu),AD[3],0xFFFFFFFFu,0x80000000u,32,0,1,0); AD[3]=(AD[3]&~0xFFFFFFFFu)|r; } }
BFF12B6: if((s32)(cyc-lim)>=0){ i=21; goto ex; }
{ cyc+=4;
{ u32 v=AD[3]; v=(v>>16)|(v<<16); AD[3]=v; NZL(0u,v,0xFFFFFFFFu,0x80000000u); } }
BFF12B8: if((s32)(cyc-lim)>=0){ i=22; goto ex; }
{ cyc+=4;
{ u32 r=ADDL(16u,(AD[1]&0xFFFFu),AD[3],0xFFFFu,0x8000u,16,0,1,0); AD[3]=(AD[3]&~0xFFFFu)|r; } }
BFF12BA: if((s32)(cyc-lim)>=0){ i=23; goto ex; }
{ cyc+=4;
CMPL(17u,0x200u,(AD[3]&0xFFFFu),0xFFFFu,0x8000u); }
BFF12BE: if((s32)(cyc-lim)>=0){ i=24; goto ex; }
{ cyc+=4;
if(CC(sr,4)){ cyc+=2;  goto BFF1316; } }
BFF12C0: if((s32)(cyc-lim)>=0){ i=25; goto ex; }
{ cyc+=4;
u32 ea0=AA[4]+0xCu; cyc+=4;
cyc+=4; u32 r2=R16(ea0);
u32 mv=r2;
AD[4]=(AD[4]&~0xFFFFu)|((mv)&0xFFFFu);
NZL(16u,mv,0xFFFFu,0x8000u); }
BFF12C4: if((s32)(cyc-lim)>=0){ i=26; goto ex; }
{ cyc+=4;
u32 ea0=AA[4]+0xEu; cyc+=4;
cyc+=4; u32 r2=R16(ea0);
u32 mv=r2;
AD[5]=(AD[5]&~0xFFFFu)|((mv)&0xFFFFu);
NZL(16u,mv,0xFFFFu,0x8000u); }
BFF12C8: if((s32)(cyc-lim)>=0){ i=27; goto ex; }
{ cyc+=4;
u32 ea0=AA[4]+0xAu; cyc+=4;
cyc+=4; u32 r2=R16(ea0);
NZL(20u,r2,0xFFFFu,0x8000u); }
BFF12CC: if((s32)(cyc-lim)>=0){ i=28; goto ex; }
{ cyc+=4;
if(CC(sr,6)){ cyc+=2;  goto BFF12E8; } }
BFF12CE: if((s32)(cyc-lim)>=0){ i=29; goto ex; }
{ cyc+=4;
cyc+=4;
cyc+=4; u32 r2=RAM16(0xFFC2u);
CMPL(31u,0x5u,r2,0xFFFFu,0x8000u); }
BFF12D4: if((s32)(cyc-lim)>=0){ i=30; goto ex; }
{ cyc+=4;
if(CC(sr,6)){ cyc+=2;  goto BFF12E2; } }
BFF12D6: if((s32)(cyc-lim)>=0){ i=31; goto ex; }
{ cyc+=4;
cyc+=4;
cyc+=4; u32 r2=RAM16(0xBF48u);
u32 mv=r2;
AD[0]=(AD[0]&~0xFFFFu)|((mv)&0xFFFFu);
NZL(0u,mv,0xFFFFu,0x8000u); }
BFF12DA: if((s32)(cyc-lim)>=0){ i=32; goto ex; }
{ cyc+=4;
{ u32 r=ADDL(16u,(AD[0]&0xFFFFu),AD[0],0xFFFFu,0x8000u,16,0,1,0); AD[0]=(AD[0]&~0xFFFFu)|r; } }
BFF12DC: if((s32)(cyc-lim)>=0){ i=33; goto ex; }
{ cyc+=4;
CMPL(31u,(AD[0]&0xFFFFu),AD[7],0xFFFFu,0x8000u); }
BFF12DE: if((s32)(cyc-lim)>=0){ i=34; goto ex; }
{ cyc+=4;
if(CC(sr,6)){ cyc+=2;  goto BFF12E2; } }
i=35; goto ex;
BFF12E2: if((s32)(cyc-lim)>=0){ i=36; goto ex; }
{ cyc+=4;
cyc+=4;
AA[7]-=4; W32(AA[7],(hi|0xFF12E6u));
cyc+=8;
npc=0xFFFFFD40u; goto out; }
BFF12E6: if((s32)(cyc-lim)>=0){ i=37; goto ex; }
{ cyc+=4;
cyc+=2;

goto BFF1316; }
BFF12E8: if((s32)(cyc-lim)>=0){ i=38; goto ex; }
{ cyc+=4;
cyc+=4;
cyc+=4; u32 r2=RAM16(0xFFC2u);
CMPL(31u,0x9u,r2,0xFFFFu,0x8000u); }
BFF12EE: if((s32)(cyc-lim)>=0){ i=39; goto ex; }
{ cyc+=4;
if(CC(sr,6)){ cyc+=2;  goto BFF1312; } }
BFF12F0: if((s32)(cyc-lim)>=0){ i=40; goto ex; }
{ cyc+=4;
cyc+=4;
cyc+=4; u32 r2=RAM16(0xBFF6u);
CMPL(31u,r2,AD[7],0xFFFFu,0x8000u); }
BFF12F4: if((s32)(cyc-lim)>=0){ i=41; goto ex; }
{ cyc+=4;
if(CC(sr,7)){ cyc+=2;  i=42; goto ex; } }
BFF12F6: if((s32)(cyc-lim)>=0){ i=43; goto ex; }
{ cyc+=4;
cyc+=4;
cyc+=4; u32 r2=RAM16(0xBFF8u);
CMPL(31u,r2,AD[7],0xFFFFu,0x8000u); }
BFF12FA: if((s32)(cyc-lim)>=0){ i=44; goto ex; }
{ cyc+=4;
if(CC(sr,6)){ cyc+=2;  goto BFF1312; } }
i=42; goto ex;
BFF1312: if((s32)(cyc-lim)>=0){ i=45; goto ex; }
{ cyc+=4;
cyc+=4;
AA[7]-=4; W32(AA[7],(hi|0xFF1316u));
cyc+=8;
npc=0xFFFFFD4Cu; goto out; }
BFF1316: if((s32)(cyc-lim)>=0){ i=46; goto ex; }
{ cyc+=4;
u32 ea0=AA[4]+0x12u; cyc+=4;
AA[4]=ea0; }
BFF131A: if((s32)(cyc-lim)>=0){ i=47; goto ex; }
{ cyc+=4;
u32 r=ADDL(16u,1u,(AD[7]&0xFFFFu),0xFFFFu,0x8000u,16,0,1,0);
AD[7]=(AD[7]&~0xFFFFu)|((r)&0xFFFFu); }
BFF131C: if((s32)(cyc-lim)>=0){ i=48; goto ex; }
{ cyc+=4;
CMPL(31u,0x14u,(AD[7]&0xFFFFu),0xFFFFu,0x8000u); }
BFF1320: if((s32)(cyc-lim)>=0){ i=49; goto ex; }
{ cyc+=4;
if(CC(sr,6)){ cyc+=2;  goto BFF1286; } }
BFF1324: if((s32)(cyc-lim)>=0){ i=50; goto ex; }
{ cyc+=4;
{ u32 t=R32(AA[7]); AA[7]+=4; cyc+=8;
npc=t; goto out;
} }
ex: npc=hi|xpc[i];
out: C.sr=(u16)sr; C.cycles=cyc; C.pc=npc; return rc;
}
u32 aotf129(u32 entry){
 static const u32 xpc[] AOT_TAB={0xFF1130u};
 u32 sr=C.sr, cyc=C.cycles, npc, i, rc=AOT_CONT; const u32 lim=aot_lim, hi=C.pc&0xFF000000u; 
 switch(entry){ case 0: goto BFF1130; default: npc=C.pc; goto out; }
BFF1130: if((s32)(cyc-lim)>=0){ i=0; goto ex; }
{ cyc+=4;
cyc+=4;
AA[7]-=4; W32(AA[7],(hi|0xFF1134u));
cyc+=8;
npc=0xFFFFFD46u; goto out; }
ex: npc=hi|xpc[i];
out: C.sr=(u16)sr; C.cycles=cyc; C.pc=npc; return rc;
}
u32 aotf130(u32 entry){
 static const u32 xpc[] AOT_TAB={0xFF1134u,0xFF1138u,0xFF113Au,0xFF1BB6u};
 u32 sr=C.sr, cyc=C.cycles, npc, i, rc=AOT_CONT; const u32 lim=aot_lim, hi=C.pc&0xFF000000u; 
 switch(entry){ case 0: goto BFF1134; case 1: goto BFF1138; case 2: goto BFF113A; default: npc=C.pc; goto out; }
BFF1134: if((s32)(cyc-lim)>=0){ i=0; goto ex; }
{ cyc+=4;
cyc+=4;
cyc+=4; u32 r2=RAM16(0xFFACu);
u32 mv=r2;
AD[2]=(AD[2]&~0xFFFFu)|((mv)&0xFFFFu);
NZL(16u,mv,0xFFFFu,0x8000u); }
BFF1138: if((s32)(cyc-lim)>=0){ i=1; goto ex; }
{ cyc+=4;
AD[3]=0x40u;
NZL(31u,0x40u,0xFFFFFFFFu,0x80000000u); }
BFF113A: if((s32)(cyc-lim)>=0){ i=2; goto ex; }
{ cyc+=4;
AA[7]-=4; W32(AA[7],(hi|0xFF113Eu));
cyc+=8;
i=3; goto ex; }
ex: npc=hi|xpc[i];
out: C.sr=(u16)sr; C.cycles=cyc; C.pc=npc; return rc;
}
u32 aotf131(u32 entry){
 static const u32 xpc[] AOT_TAB={0xFF113Eu};
 u32 sr=C.sr, cyc=C.cycles, npc, i, rc=AOT_CONT; const u32 lim=aot_lim, hi=C.pc&0xFF000000u; 
 switch(entry){ case 0: goto BFF113E; default: npc=C.pc; goto out; }
BFF113E: if((s32)(cyc-lim)>=0){ i=0; goto ex; }
{ cyc+=4;
cyc+=4;
npc=0xFFFFFC6Eu; goto out; }
ex: npc=hi|xpc[i];
out: C.sr=(u16)sr; C.cycles=cyc; C.pc=npc; return rc;
}
u32 aotf132(u32 entry){
 static const u32 xpc[] AOT_TAB={0xFF114Cu,0xFF114Eu,0xFF1152u};
 u32 sr=C.sr, cyc=C.cycles, npc, i, rc=AOT_CONT; const u32 lim=aot_lim, hi=C.pc&0xFF000000u; 
 switch(entry){ case 0: goto BFF114C; case 1: goto BFF114E; default: npc=C.pc; goto out; }
BFF114C: if((s32)(cyc-lim)>=0){ i=0; goto ex; }
{ cyc+=4;
u32 mv=(AD[7]&0xFFFFFFFFu);
u32 ea1=AA[6]; AA[6]+=4u;
cyc+=8; W32(ea1,mv);
NZL(31u,mv,0xFFFFFFFFu,0x80000000u); }
BFF114E: if((s32)(cyc-lim)>=0){ i=1; goto ex; }
{ cyc+=4;
if(!0){ u32 w=(AD[6]-1u)&0xFFFFu; AD[6]=(AD[6]&0xFFFF0000u)|w; if(w!=0xFFFFu){ goto BFF114C; } } }
i=2; goto ex;
ex: npc=hi|xpc[i];
out: C.sr=(u16)sr; C.cycles=cyc; C.pc=npc; return rc;
}
u32 aotf133(u32 entry){
 static const u32 xpc[] AOT_TAB={0xFF116Au,0xFF116Cu,0xFF116Eu,0xFF1170u,0xFF1172u,0xFF1176u};
 u32 sr=C.sr, cyc=C.cycles, npc, i, rc=AOT_CONT; const u32 lim=aot_lim, hi=C.pc&0xFF000000u; 
 switch(entry){ case 0: goto BFF116A; case 1: goto BFF116C; case 2: goto BFF116E; case 3: goto BFF1170; case 4: goto BFF1172; default: npc=C.pc; goto out; }
BFF116A: if((s32)(cyc-lim)>=0){ i=0; goto ex; }
{ cyc+=4;
{ u32 r=ADDL(31u,(AD[2]&0xFFu),AD[2],0xFFu,0x80u,8,0,1,0); AD[2]=(AD[2]&~0xFFu)|r; } }
BFF116C: if((s32)(cyc-lim)>=0){ i=1; goto ex; }
{ cyc+=4;
if(CC(sr,4)){ cyc+=2;  goto BFF1170; } }
BFF116E: if((s32)(cyc-lim)>=0){ i=2; goto ex; }
{ cyc+=4;
u32 ea0=AA[0]; AA[0]+=1u;
cyc+=4; u32 r2=R8(ea0);
u32 mv=r2;
u32 ea1=AA[2];
cyc+=4; W8(ea1,mv);
NZL(31u,mv,0xFFu,0x80u); }
BFF1170: if((s32)(cyc-lim)>=0){ i=3; goto ex; }
{ cyc+=4;
AA[2]+=1u; }
BFF1172: if((s32)(cyc-lim)>=0){ i=4; goto ex; }
{ cyc+=4;
if(!0){ u32 w=(AD[3]-1u)&0xFFFFu; AD[3]=(AD[3]&0xFFFF0000u)|w; if(w!=0xFFFFu){ goto BFF116A; } } }
i=5; goto ex;
ex: npc=hi|xpc[i];
out: C.sr=(u16)sr; C.cycles=cyc; C.pc=npc; return rc;
}
u32 aotf134(u32 entry){
 static const u32 xpc[] AOT_TAB={0xFF1278u,0xFF127Au,0xFF127Eu};
 u32 sr=C.sr, cyc=C.cycles, npc, i, rc=AOT_CONT; const u32 lim=aot_lim, hi=C.pc&0xFF000000u; 
 switch(entry){ case 0: goto BFF1278; case 1: goto BFF127A; default: npc=C.pc; goto out; }
BFF1278: if((s32)(cyc-lim)>=0){ i=0; goto ex; }
{ cyc+=4;
u32 mv=(AD[7]&0xFFFFFFFFu);
u32 ea1=AA[6]; AA[6]+=4u;
cyc+=8; W32(ea1,mv);
NZL(31u,mv,0xFFFFFFFFu,0x80000000u); }
BFF127A: if((s32)(cyc-lim)>=0){ i=1; goto ex; }
{ cyc+=4;
if(!0){ u32 w=(AD[6]-1u)&0xFFFFu; AD[6]=(AD[6]&0xFFFF0000u)|w; if(w!=0xFFFFu){ goto BFF1278; } } }
i=2; goto ex;
ex: npc=hi|xpc[i];
out: C.sr=(u16)sr; C.cycles=cyc; C.pc=npc; return rc;
}
u32 aotf135(u32 entry){
 static const u32 xpc[] AOT_TAB={0xFF134Cu,0xFF1350u,0xFF1354u,0xFF1358u,0xFF135Au};
 u32 sr=C.sr, cyc=C.cycles, npc, i, rc=AOT_CONT; const u32 lim=aot_lim, hi=C.pc&0xFF000000u; 
 switch(entry){ case 0: goto BFF134C; case 1: goto BFF1350; case 2: goto BFF1354; case 3: goto BFF1358; case 4: goto BFF135A; default: npc=C.pc; goto out; }
BFF134C: if((s32)(cyc-lim)>=0){ i=0; goto ex; }
{ cyc+=4;
cyc+=4;
cyc+=4; u32 r2=RAM16(0xBF94u);
u32 mv=r2;
AD[0]=(AD[0]&~0xFFFFu)|((mv)&0xFFFFu);
NZL(16u,mv,0xFFFFu,0x8000u); }
BFF1350: if((s32)(cyc-lim)>=0){ i=1; goto ex; }
{ cyc+=4;
cyc+=4;
{ u32 r=(AD[0]&0xFFFFu)*((0x1Au)&0xFFFFu);
AD[0]=r; NZL(31u,r,0xFFFFFFFFu,0x80000000u); cyc+=50; } }
BFF1354: if((s32)(cyc-lim)>=0){ i=2; goto ex; }
{ cyc+=4;
cyc+=4;
AA[5]=0xFFFFAC00u; }
BFF1358: if((s32)(cyc-lim)>=0){ i=3; goto ex; }
{ cyc+=4;
AA[5]+=(AD[0]&0xFFFFFFFFu); }
BFF135A: if((s32)(cyc-lim)>=0){ i=4; goto ex; }
{ cyc+=4;
{ u32 t=R32(AA[7]); AA[7]+=4; cyc+=8;
npc=t; goto out;
} }
ex: npc=hi|xpc[i];
out: C.sr=(u16)sr; C.cycles=cyc; C.pc=npc; return rc;
}
u32 aotf136(u32 entry){
 static const u32 xpc[] AOT_TAB={0xFF1750u,0xFF1754u,0xFF1758u,0xFF175Au,0xFF1772u,0xFF1776u,0xFF1778u,0xFF177Au,0xFF177Cu,0xFF177Eu,0xFF1780u,0xFF1788u,0xFF1782u,0xFF1786u,0xFF178Eu,0xFF1790u,0xFF1792u,0xFF1798u,0xFF179Au,0xFF179Cu,0xFF17A2u,0xFF17A6u,0xFF17A8u,0xFF17ACu,0xFF17AEu,0xFF17B2u,0xFF17B6u,0xFF17BAu,0xFF17BEu,0xFF17C0u,0xFF17C2u,0xFF17C4u,0xFF134Cu};
 u32 sr=C.sr, cyc=C.cycles, npc, i, rc=AOT_CONT; const u32 lim=aot_lim, hi=C.pc&0xFF000000u; 
 switch(entry){ case 0: goto BFF1750; case 1: goto BFF1754; case 2: goto BFF1758; case 3: goto BFF1772; case 4: goto BFF1776; case 5: goto BFF1778; case 6: goto BFF177A; case 7: goto BFF177C; case 8: goto BFF177E; case 9: goto BFF1780; case 10: goto BFF1782; case 11: goto BFF1786; case 12: goto BFF178E; case 13: goto BFF1790; case 14: goto BFF1798; case 15: goto BFF179A; case 16: goto BFF17A2; case 17: goto BFF17A6; case 18: goto BFF17A8; case 19: goto BFF17AC; case 20: goto BFF17AE; case 21: goto BFF17B2; case 22: goto BFF17B6; case 23: goto BFF17BA; case 24: goto BFF17BE; case 25: goto BFF17C0; case 26: goto BFF17C2; case 27: goto BFF17C4; default: npc=C.pc; goto out; }
BFF1750: if((s32)(cyc-lim)>=0){ i=0; goto ex; }
{ cyc+=4;
u32 ea0=AA[0]+0x4u; cyc+=4;
cyc+=4; u32 r2=R16(ea0);
u32 mv=r2;
AD[6]=(AD[6]&~0xFFFFu)|((mv)&0xFFFFu);
NZL(16u,mv,0xFFFFu,0x8000u); }
BFF1754: if((s32)(cyc-lim)>=0){ i=1; goto ex; }
{ cyc+=4;
CMPL(31u,0x8000u,(AD[6]&0xFFFFu),0xFFFFu,0x8000u); }
BFF1758: if((s32)(cyc-lim)>=0){ i=2; goto ex; }
{ cyc+=4;
if(CC(sr,6)){ cyc+=2;  goto BFF1772; } }
i=3; goto ex;
BFF1772: if((s32)(cyc-lim)>=0){ i=4; goto ex; }
{ cyc+=4;
u32 ea0=AA[0]+0x2u; cyc+=4;
cyc+=4; u32 r2=R16(ea0);
u32 mv=r2;
AD[0]=(AD[0]&~0xFFFFu)|((mv)&0xFFFFu);
NZL(4u,mv,0xFFFFu,0x8000u); }
BFF1776: if((s32)(cyc-lim)>=0){ i=5; goto ex; }
{ cyc+=4;
if(CC(sr,7)){ cyc+=2;  goto BFF17A2; } }
BFF1778: if((s32)(cyc-lim)>=0){ i=6; goto ex; }
{ cyc+=4;
u32 r=SUBL(21u,2u,(AD[0]&0xFFFFu),0xFFFFu,0x8000u,0,1,0);
AD[0]=(AD[0]&~0xFFFFu)|((r)&0xFFFFu); }
BFF177A: if((s32)(cyc-lim)>=0){ i=7; goto ex; }
{ cyc+=4;
if(CC(sr,2)){ cyc+=2;  goto BFF1798; } }
BFF177C: if((s32)(cyc-lim)>=0){ i=8; goto ex; }
{ cyc+=4;
if(CC(sr,7)){ cyc+=2;  goto BFF178E; } }
BFF177E: if((s32)(cyc-lim)>=0){ i=9; goto ex; }
{ cyc+=4;
CMPL(31u,(AD[6]&0xFFFFu),AD[5],0xFFFFu,0x8000u); }
BFF1780: if((s32)(cyc-lim)>=0){ i=10; goto ex; }
{ cyc+=4;
if(CC(sr,7)){ cyc+=2;  i=11; goto ex; } }
BFF1782: if((s32)(cyc-lim)>=0){ i=12; goto ex; }
{ cyc+=4;
u32 ea0=AA[0]+0x14u; cyc+=4;
cyc+=4; u32 r2=R16(ea0);
CMPL(31u,r2,AD[5],0xFFFFu,0x8000u); }
BFF1786: if((s32)(cyc-lim)>=0){ i=13; goto ex; }
{ cyc+=4;
if(CC(sr,6)){ cyc+=2;  goto BFF17A2; } }
i=11; goto ex;
BFF178E: if((s32)(cyc-lim)>=0){ i=14; goto ex; }
{ cyc+=4;
CMPL(31u,(AD[6]&0xFFFFu),AD[5],0xFFFFu,0x8000u); }
BFF1790: if((s32)(cyc-lim)>=0){ i=15; goto ex; }
{ cyc+=4;
if(CC(sr,6)){ cyc+=2;  goto BFF17A2; } }
i=16; goto ex;
BFF1798: if((s32)(cyc-lim)>=0){ i=17; goto ex; }
{ cyc+=4;
CMPL(31u,(AD[6]&0xFFFFu),AD[5],0xFFFFu,0x8000u); }
BFF179A: if((s32)(cyc-lim)>=0){ i=18; goto ex; }
{ cyc+=4;
if(CC(sr,6)){ cyc+=2;  goto BFF17A2; } }
i=19; goto ex;
BFF17A2: if((s32)(cyc-lim)>=0){ i=20; goto ex; }
{ cyc+=4;
u32 ea0=AA[0]+0x1Au; cyc+=4;
AA[0]=ea0; }
BFF17A6: if((s32)(cyc-lim)>=0){ i=21; goto ex; }
{ cyc+=4;
u32 r=ADDL(16u,1u,(AD[7]&0xFFFFu),0xFFFFu,0x8000u,16,0,1,0);
AD[7]=(AD[7]&~0xFFFFu)|((r)&0xFFFFu); }
BFF17A8: if((s32)(cyc-lim)>=0){ i=22; goto ex; }
{ cyc+=4;
CMPL(20u,0xB4u,(AD[7]&0xFFFFu),0xFFFFu,0x8000u); }
BFF17AC: if((s32)(cyc-lim)>=0){ i=23; goto ex; }
{ cyc+=4;
if(CC(sr,6)){ cyc+=2;  goto BFF1750; } }
BFF17AE: if((s32)(cyc-lim)>=0){ i=24; goto ex; }
{ cyc+=4;
u32 mv=(AD[1]&0xFFFFu);
cyc+=4;
cyc+=4; RAM16(0xBF98u)=(u16)(mv);;
NZL(16u,mv,0xFFFFu,0x8000u); }
BFF17B2: if((s32)(cyc-lim)>=0){ i=25; goto ex; }
{ cyc+=4;
u32 mv=(AD[2]&0xFFFFu);
cyc+=4;
cyc+=4; RAM16(0xBFA2u)=(u16)(mv);;
NZL(16u,mv,0xFFFFu,0x8000u); }
BFF17B6: if((s32)(cyc-lim)>=0){ i=26; goto ex; }
{ cyc+=4;
u32 mv=(AD[3]&0xFFFFu);
cyc+=4;
cyc+=4; RAM16(0xBFB2u)=(u16)(mv);;
NZL(16u,mv,0xFFFFu,0x8000u); }
BFF17BA: if((s32)(cyc-lim)>=0){ i=27; goto ex; }
{ cyc+=4;
u32 mv=(AD[4]&0xFFFFu);
cyc+=4;
cyc+=4; RAM16(0xBFB4u)=(u16)(mv);;
NZL(16u,mv,0xFFFFu,0x8000u); }
BFF17BE: if((s32)(cyc-lim)>=0){ i=28; goto ex; }
{ cyc+=4;
u32 ea0=AA[1];
cyc+=4; u32 r2=R16(ea0);
cyc+=4; W16(ea0,0);
sr=(sr&~(FN_|FV_|FC_))|FZ_; }
BFF17C0: if((s32)(cyc-lim)>=0){ i=29; goto ex; }
{ cyc+=4;
u32 ea0=AA[2];
cyc+=4; u32 r2=R16(ea0);
cyc+=4; W16(ea0,0);
sr=(sr&~(FN_|FV_|FC_))|FZ_; }
BFF17C2: if((s32)(cyc-lim)>=0){ i=30; goto ex; }
{ cyc+=4;
u32 ea0=AA[4];
cyc+=4; u32 r2=R16(ea0);
cyc+=4; W16(ea0,0);
sr=(sr&~(FN_|FV_|FC_))|FZ_; }
BFF17C4: if((s32)(cyc-lim)>=0){ i=31; goto ex; }
{ cyc+=4;
AA[7]-=4; W32(AA[7],(hi|0xFF17C8u));
cyc+=8;
i=32; goto ex; }
ex: npc=hi|xpc[i];
out: C.sr=(u16)sr; C.cycles=cyc; C.pc=npc; return rc;
}
u32 aotf137(u32 entry){
 static const u32 xpc[] AOT_TAB={0xFF17C8u,0xFF17CAu,0xFF17CEu,0xFF17D0u,0xFF17D8u,0xFF17DCu};
 u32 sr=C.sr, cyc=C.cycles, npc, i, rc=AOT_CONT; const u32 lim=aot_lim, hi=C.pc&0xFF000000u; 
 switch(entry){ case 0: goto BFF17C8; case 1: goto BFF17CA; case 2: goto BFF17CE; case 3: goto BFF17D8; case 4: goto BFF17DC; default: npc=C.pc; goto out; }
BFF17C8: if((s32)(cyc-lim)>=0){ i=0; goto ex; }
{ cyc+=4;
AD[0]=0x0u;
NZL(16u,0x0u,0xFFFFFFFFu,0x80000000u); }
BFF17CA: if((s32)(cyc-lim)>=0){ i=1; goto ex; }
{ cyc+=4;
u32 ea0=AA[5]+0xCu; cyc+=4;
cyc+=4; u32 r2=R16(ea0);
NZL(31u,r2,0xFFFFu,0x8000u); }
BFF17CE: if((s32)(cyc-lim)>=0){ i=2; goto ex; }
{ cyc+=4;
if(CC(sr,7)){ cyc+=2;  goto BFF17D8; } }
i=3; goto ex;
BFF17D8: if((s32)(cyc-lim)>=0){ i=4; goto ex; }
{ cyc+=4;
u32 mv=(AD[0]&0xFFFFu);
cyc+=4;
cyc+=4; RAM16(0xC02Cu)=(u16)(mv);;
NZL(31u,mv,0xFFFFu,0x8000u); }
BFF17DC: if((s32)(cyc-lim)>=0){ i=5; goto ex; }
{ cyc+=4;
{ u32 t=R32(AA[7]); AA[7]+=4; cyc+=8;
npc=t; goto out;
} }
ex: npc=hi|xpc[i];
out: C.sr=(u16)sr; C.cycles=cyc; C.pc=npc; return rc;
}
u32 aotf138(u32 entry){
 static const u32 xpc[] AOT_TAB={0xFF1896u,0xFF189Au,0xFF189Eu,0xFF18A0u,0xFF18A4u};
 u32 sr=C.sr, cyc=C.cycles, npc, i, rc=AOT_CONT; const u32 lim=aot_lim, hi=C.pc&0xFF000000u; 
 switch(entry){ case 0: goto BFF1896; case 1: goto BFF189A; case 2: goto BFF189E; case 3: goto BFF18A4; default: npc=C.pc; goto out; }
BFF1896: if((s32)(cyc-lim)>=0){ i=0; goto ex; }
{ cyc+=4;
{ u32 a=AA[7];
u32 native_n=m68k_movem_ram(a,0xFFFEu,2u); if(native_n){a-=4u*native_n;cyc+=8u*native_n;}else{
a-=4; W32(a,AA[6]); cyc+=8;
a-=4; W32(a,AA[5]); cyc+=8;
a-=4; W32(a,AA[4]); cyc+=8;
a-=4; W32(a,AA[3]); cyc+=8;
a-=4; W32(a,AA[2]); cyc+=8;
a-=4; W32(a,AA[1]); cyc+=8;
a-=4; W32(a,AA[0]); cyc+=8;
a-=4; W32(a,AD[7]); cyc+=8;
a-=4; W32(a,AD[6]); cyc+=8;
a-=4; W32(a,AD[5]); cyc+=8;
a-=4; W32(a,AD[4]); cyc+=8;
a-=4; W32(a,AD[3]); cyc+=8;
a-=4; W32(a,AD[2]); cyc+=8;
a-=4; W32(a,AD[1]); cyc+=8;
a-=4; W32(a,AD[0]); cyc+=8;
}
AA[7]=a; } }
BFF189A: if((s32)(cyc-lim)>=0){ i=1; goto ex; }
{ cyc+=4;
cyc+=4;
cyc+=4; u32 r2=RAM16(0xFF94u);
NZL(31u,r2,0xFFFFu,0x8000u); }
BFF189E: if((s32)(cyc-lim)>=0){ i=2; goto ex; }
{ cyc+=4;
if(CC(sr,7)){ cyc+=2;  goto BFF18A4; } }
i=3; goto ex;
BFF18A4: if((s32)(cyc-lim)>=0){ i=4; goto ex; }
{ cyc+=4;
cyc+=4;
AA[7]-=4; W32(AA[7],(hi|0xFF18A8u));
cyc+=8;
npc=0xFFFFFCF8u; goto out; }
ex: npc=hi|xpc[i];
out: C.sr=(u16)sr; C.cycles=cyc; C.pc=npc; return rc;
}
u32 aotf139(u32 entry){
 static const u32 xpc[] AOT_TAB={0xFF18A8u};
 u32 sr=C.sr, cyc=C.cycles, npc, i, rc=AOT_CONT; const u32 lim=aot_lim, hi=C.pc&0xFF000000u; 
 switch(entry){ case 0: goto BFF18A8; default: npc=C.pc; goto out; }
BFF18A8: if((s32)(cyc-lim)>=0){ i=0; goto ex; }
{ cyc+=4;
cyc+=4;
AA[7]-=4; W32(AA[7],(hi|0xFF18ACu));
cyc+=8;
npc=0xFFFFFCFEu; goto out; }
ex: npc=hi|xpc[i];
out: C.sr=(u16)sr; C.cycles=cyc; C.pc=npc; return rc;
}
u32 aotf140(u32 entry){
 static const u32 xpc[] AOT_TAB={0xFF18ACu,0xFF18B0u,0xFF18B4u,0xFF18BAu,0xFF18BCu,0xFF18C0u,0xFF18C6u,0xFF18C8u,0xFF18CCu,0xFF18CEu,0xFF18D4u,0xFF18DAu,0xFF18E0u,0xFF18E4u,0xFF18E8u};
 u32 sr=C.sr, cyc=C.cycles, npc, i, rc=AOT_CONT; const u32 lim=aot_lim, hi=C.pc&0xFF000000u; 
 switch(entry){ case 0: goto BFF18AC; case 1: goto BFF18B0; case 2: goto BFF18B4; case 3: goto BFF18BA; case 4: goto BFF18BC; case 5: goto BFF18C0; case 6: goto BFF18C6; case 7: goto BFF18C8; case 8: goto BFF18CC; case 9: goto BFF18CE; case 10: goto BFF18D4; case 11: goto BFF18DA; case 12: goto BFF18E0; case 13: goto BFF18E4; case 14: goto BFF18E8; default: npc=C.pc; goto out; }
BFF18AC: if((s32)(cyc-lim)>=0){ i=0; goto ex; }
{ cyc+=4;
cyc+=4;
AA[6]=0xFFFFF860u; }
BFF18B0: if((s32)(cyc-lim)>=0){ i=1; goto ex; }
{ cyc+=4;
cyc+=4;
u32 mv=0x18u;
AD[0]=(AD[0]&~0xFFFFu)|((mv)&0xFFFFu);
NZL(16u,mv,0xFFFFu,0x8000u); }
BFF18B4: if((s32)(cyc-lim)>=0){ i=2; goto ex; }
{ cyc+=4;
cyc+=4;
cyc+=4; u32 r2=RAM16(0xFFC2u);
CMPL(20u,0x3u,r2,0xFFFFu,0x8000u); }
BFF18BA: if((s32)(cyc-lim)>=0){ i=3; goto ex; }
{ cyc+=4;
if(CC(sr,7)){ cyc+=2;  goto BFF18C8; } }
BFF18BC: if((s32)(cyc-lim)>=0){ i=4; goto ex; }
{ cyc+=4;
cyc+=4;
u32 mv=0x78u;
AD[0]=(AD[0]&~0xFFFFu)|((mv)&0xFFFFu);
NZL(16u,mv,0xFFFFu,0x8000u); }
BFF18C0: if((s32)(cyc-lim)>=0){ i=5; goto ex; }
{ cyc+=4;
cyc+=4;
cyc+=4; u32 r2=RAM16(0xFFC2u);
CMPL(20u,0x9u,r2,0xFFFFu,0x8000u); }
BFF18C6: if((s32)(cyc-lim)>=0){ i=6; goto ex; }
{ cyc+=4;
if(CC(sr,6)){ cyc+=2;  goto BFF18DA; } }
BFF18C8: if((s32)(cyc-lim)>=0){ i=7; goto ex; }
{ cyc+=4;
cyc+=4;
cyc+=4; u32 r2=RAM16(0xC048u);
u32 r=SUBL(20u,1u,r2,0xFFFFu,0x8000u,0,1,0);
cyc+=4; RAM16(0xC048u)=(u16)(r);; }
BFF18CC: if((s32)(cyc-lim)>=0){ i=8; goto ex; }
{ cyc+=4;
if(CC(sr,6)){ cyc+=2;  goto BFF18DA; } }
BFF18CE: if((s32)(cyc-lim)>=0){ i=9; goto ex; }
{ cyc+=4;
cyc+=4;
u32 mv=0x6u;
cyc+=4;
cyc+=4; RAM16(0xC048u)=(u16)(mv);;
NZL(16u,mv,0xFFFFu,0x8000u); }
BFF18D4: if((s32)(cyc-lim)>=0){ i=10; goto ex; }
{ cyc+=4;
u32 ea0=AA[6]+((u32)(s32)(s16)AD[0]+0x0u); cyc+=6;
cyc+=4; u32 r2=R16(ea0);
u32 r=r2^0xEEu;
NZL(16u,r,0xFFFFu,0x8000u);
cyc+=4; W16(ea0,r); }
BFF18DA: if((s32)(cyc-lim)>=0){ i=11; goto ex; }
{ cyc+=4;
cyc+=8;
AA[6]=0xC00004u; }
BFF18E0: if((s32)(cyc-lim)>=0){ i=12; goto ex; }
{ cyc+=4;
cyc+=4;
cyc+=4; u32 r2=RAM16(0xFF96u);
u32 mv=r2;
AD[0]=(AD[0]&~0xFFFFu)|((mv)&0xFFFFu);
NZL(16u,mv,0xFFFFu,0x8000u); }
BFF18E4: if((s32)(cyc-lim)>=0){ i=13; goto ex; }
{ cyc+=4;
u32 r=(AD[0]&0xFFFFu)&0xCu;
NZL(31u,r,0xFFFFu,0x8000u);
AD[0]=(AD[0]&~0xFFFFu)|((r)&0xFFFFu); }
BFF18E8: if((s32)(cyc-lim)>=0){ i=14; goto ex; }
{ cyc+=4;
u32 ea0=(hi|0xFF18EAu)+((u32)(s32)(s16)AD[0]+0x18u); cyc+=6;
AA[7]-=4; W32(AA[7],(hi|0xFF18ECu));
cyc+=8;
npc=ea0; goto out; }
ex: npc=hi|xpc[i];
out: C.sr=(u16)sr; C.cycles=cyc; C.pc=npc; return rc;
}
u32 aotf141(u32 entry){
 static const u32 xpc[] AOT_TAB={0xFF18ECu};
 u32 sr=C.sr, cyc=C.cycles, npc, i, rc=AOT_CONT; const u32 lim=aot_lim, hi=C.pc&0xFF000000u; 
 switch(entry){ case 0: goto BFF18EC; default: npc=C.pc; goto out; }
BFF18EC: if((s32)(cyc-lim)>=0){ i=0; goto ex; }
{ cyc+=4;
cyc+=4;
AA[7]-=4; W32(AA[7],(hi|0xFF18F0u));
cyc+=8;
npc=0xFFFFFBBAu; goto out; }
ex: npc=hi|xpc[i];
out: C.sr=(u16)sr; C.cycles=cyc; C.pc=npc; return rc;
}
u32 aotf142(u32 entry){
 static const u32 xpc[] AOT_TAB={0xFF18F0u};
 u32 sr=C.sr, cyc=C.cycles, npc, i, rc=AOT_CONT; const u32 lim=aot_lim, hi=C.pc&0xFF000000u; 
 switch(entry){ case 0: goto BFF18F0; default: npc=C.pc; goto out; }
BFF18F0: if((s32)(cyc-lim)>=0){ i=0; goto ex; }
{ cyc+=4;
cyc+=4;
AA[7]-=4; W32(AA[7],(hi|0xFF18F4u));
cyc+=8;
npc=0xFFFFFCF2u; goto out; }
ex: npc=hi|xpc[i];
out: C.sr=(u16)sr; C.cycles=cyc; C.pc=npc; return rc;
}
u32 aotf143(u32 entry){
 static const u32 xpc[] AOT_TAB={0xFF18F4u};
 u32 sr=C.sr, cyc=C.cycles, npc, i, rc=AOT_CONT; const u32 lim=aot_lim, hi=C.pc&0xFF000000u; 
 switch(entry){ case 0: goto BFF18F4; default: npc=C.pc; goto out; }
BFF18F4: if((s32)(cyc-lim)>=0){ i=0; goto ex; }
{ cyc+=4;
cyc+=4;
AA[7]-=4; W32(AA[7],(hi|0xFF18F8u));
cyc+=8;
npc=0xFFFFFC7Au; goto out; }
ex: npc=hi|xpc[i];
out: C.sr=(u16)sr; C.cycles=cyc; C.pc=npc; return rc;
}
u32 aotf144(u32 entry){
 static const u32 xpc[] AOT_TAB={0xFF18F8u,0xFF18FCu,0xFF1900u};
 u32 sr=C.sr, cyc=C.cycles, npc, i, rc=AOT_CONT; const u32 lim=aot_lim, hi=C.pc&0xFF000000u; 
 switch(entry){ case 0: goto BFF18F8; case 1: goto BFF18FC; case 2: goto BFF1900; default: npc=C.pc; goto out; }
BFF18F8: if((s32)(cyc-lim)>=0){ i=0; goto ex; }
{ cyc+=4;
cyc+=4;
cyc+=4; u32 r2=RAM16(0xFF96u);
cyc+=4; RAM16(0xFF96u)=(u16)(0);;
sr=(sr&~(FN_|FV_|FC_))|FZ_; }
BFF18FC: if((s32)(cyc-lim)>=0){ i=1; goto ex; }
{ cyc+=4;
{ u32 a=AA[7];
u32 native_n=m68k_movem_ram(a,0x7FFFu,1u); if(native_n){a+=4u*native_n;cyc+=8u*native_n;}else{
AD[0]=R32(a); a+=4; cyc+=8;
AD[1]=R32(a); a+=4; cyc+=8;
AD[2]=R32(a); a+=4; cyc+=8;
AD[3]=R32(a); a+=4; cyc+=8;
AD[4]=R32(a); a+=4; cyc+=8;
AD[5]=R32(a); a+=4; cyc+=8;
AD[6]=R32(a); a+=4; cyc+=8;
AD[7]=R32(a); a+=4; cyc+=8;
AA[0]=R32(a); a+=4; cyc+=8;
AA[1]=R32(a); a+=4; cyc+=8;
AA[2]=R32(a); a+=4; cyc+=8;
AA[3]=R32(a); a+=4; cyc+=8;
AA[4]=R32(a); a+=4; cyc+=8;
AA[5]=R32(a); a+=4; cyc+=8;
AA[6]=R32(a); a+=4; cyc+=8;
}
AA[7]=a;
} }
BFF1900: if((s32)(cyc-lim)>=0){ i=2; goto ex; }
C.sr=(u16)sr;C.cycles=cyc;rc=md_native_rte();sr=C.sr;cyc=C.cycles;npc=C.pc;goto out;
ex: npc=hi|xpc[i];
out: C.sr=(u16)sr; C.cycles=cyc; C.pc=npc; return rc;
}
u32 aotf145(u32 entry){
 static const u32 xpc[] AOT_TAB={0xFF1902u,0xFF194Au,0xFF194Eu,0xFF1952u};
 u32 sr=C.sr, cyc=C.cycles, npc, i, rc=AOT_CONT; const u32 lim=aot_lim, hi=C.pc&0xFF000000u; 
 switch(entry){ case 0: goto BFF1902; case 1: goto BFF194A; case 2: goto BFF194E; case 3: goto BFF1952; default: npc=C.pc; goto out; }
BFF1902: if((s32)(cyc-lim)>=0){ i=0; goto ex; }
{ cyc+=4;
cyc+=2;

goto BFF1952; }
BFF194A: if((s32)(cyc-lim)>=0){ i=1; goto ex; }
{ cyc+=4;
cyc+=4;
cyc+=4; u32 r2=RAM8(0xFF8Bu);
u32 mv=r2;
AD[0]=(AD[0]&~0xFFu)|((mv)&0xFFu);
NZL(16u,mv,0xFFu,0x80u); }
BFF194E: if((s32)(cyc-lim)>=0){ i=2; goto ex; }
{ cyc+=4;
cyc+=4;
cyc+=4; u32 r2=RAM8(0xFF8Au);
u32 r=(AD[0]|r2)&0xFFu;
NZL(31u,r,0xFFu,0x80u);
cyc+=4; RAM8(0xFF8Au)=(u8)(r);; }
BFF1952: if((s32)(cyc-lim)>=0){ i=3; goto ex; }
{ cyc+=4;
{ u32 t=R32(AA[7]); AA[7]+=4; cyc+=8;
npc=t; goto out;
} }
ex: npc=hi|xpc[i];
out: C.sr=(u16)sr; C.cycles=cyc; C.pc=npc; return rc;
}
u32 aotf146(u32 entry){
 static const u32 xpc[] AOT_TAB={0xFF1906u,0xFF1912u};
 u32 sr=C.sr, cyc=C.cycles, npc, i, rc=AOT_CONT; const u32 lim=aot_lim, hi=C.pc&0xFF000000u; 
 switch(entry){ case 0: goto BFF1906; case 1: goto BFF1912; default: npc=C.pc; goto out; }
BFF1906: if((s32)(cyc-lim)>=0){ i=0; goto ex; }
{ cyc+=4;
cyc+=2;

goto BFF1912; }
BFF1912: if((s32)(cyc-lim)>=0){ i=1; goto ex; }
{ cyc+=4;
cyc+=4;
AA[7]-=4; W32(AA[7],(hi|0xFF1916u));
cyc+=8;
npc=0xFFFFFB96u; goto out; }
ex: npc=hi|xpc[i];
out: C.sr=(u16)sr; C.cycles=cyc; C.pc=npc; return rc;
}
u32 aotf147(u32 entry){
 static const u32 xpc[] AOT_TAB={0xFF1916u};
 u32 sr=C.sr, cyc=C.cycles, npc, i, rc=AOT_CONT; const u32 lim=aot_lim, hi=C.pc&0xFF000000u; 
 switch(entry){ case 0: goto BFF1916; default: npc=C.pc; goto out; }
BFF1916: if((s32)(cyc-lim)>=0){ i=0; goto ex; }
{ cyc+=4;
cyc+=4;
AA[7]-=4; W32(AA[7],(hi|0xFF191Au));
cyc+=8;
npc=0xFFFFFC08u; goto out; }
ex: npc=hi|xpc[i];
out: C.sr=(u16)sr; C.cycles=cyc; C.pc=npc; return rc;
}