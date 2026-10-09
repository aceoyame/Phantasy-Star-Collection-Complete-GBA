#!/usr/bin/env python3
"""Translate the pinned US ending's linear panel/dialogue program. Only this
small script table is generated; accepted asset/audio banks are untouched."""
from pathlib import Path
import re,json
R=Path(__file__).resolve().parents[1];O=R.parent/'build_artifacts';asm=(R/'references/disassembly/ps4disasm/ps4disasm/ps4.asm').read_text();lines=asm.splitlines();start=next(i for i,l in enumerate(lines) if l.strip()=='Cutscene_Ending:');end=next(i for i in range(start,len(lines)) if lines[i].strip()=='loc_79BD4:');start=next(i for i in range(start,end) if 'MusicID_Explosion' in lines[i]);ops=[];calls=[];d0=0;entry=0;delay=0;a0=a1='';last=None;loopwait=False
kinds=['INIT','FADE_IN','PANEL','POP','PUBLISH','WAIT','TEXT','RESUME','TONE','RESTORE','SAVE','COPY','SAVED_PD_PAL','ZERO_LINE3','DOWN','UP','SOUND','CLEAR']
sounds={'MusicID_Explosion':0xAD,'MusicID_ThePromisingFuture1':0xAF,'MusicID_ThePromisingFuture2':0xB2,'Sound_StopMusic':0xFB}
def emit(k,v=0,line=0):ops.append((k,v,line));return k
def number(n):return int(n[1:],16) if n.startswith('$') else int(n)
for i in range(start,end):
 l=lines[i].strip();m=re.match(r'move(?:q|\.w)\s+#(\$[0-9A-F]+|\d+), d0',l)
 # RunText3 pushes its F7 continuation. The native runner retains that exact
 # pointer in field_dialogue_saved_cursor; popdlg needs no second cursor stack.
 if m:d0=number(m[1])
 m=re.match(r'move\.b\s+#(\$[0-9A-F]+|\d+), \(\$FFFFED52\)',l)
 if m:delay=number(m[1])
 m=re.match(r'lea\s+\(([^)]+)\)\.(?:w|l), (a[01])',l)
 if m:
  if m[2]=='a0':a0=m[1]
  else:a1=m[1]
 if 'clr.b\t(Panel_Num)' in lines[i] or re.match(r'clr.b\s+\(Panel_Num\)',l):emit('CLEAR',line=i+1)
 if l=='trap\t#1' or re.match(r'trap\s+#1$',l):
  if a0=='Palette_Table_Buffer' and a1=='Palette_Table_Buffer_2':emit('SAVE',line=i+1)
  elif a0=='Palette_Table_Buffer_2' and a1=='Palette_Table_Buffer':emit('COPY',line=i+1)
  elif a0=='loc_2EEA04' and a1=='Palette_Table_Buffer_2':emit('SAVED_PD_PAL',line=i+1)
  else:raise AssertionError((i,l,a0,a1))
 if l=='loc_79882:':emit('ZERO_LINE3',line=i+1)
 m=re.match(r'move\.b\s+#([^,]+), \(Sound_Index\)',l)
 if m:emit('SOUND',sounds[m[1]],i+1)
 m=re.match(r'(?:jsr|bsr\.w)\s+\(?([^).]+)\)?(?:\.l)?$',l)
 if not m:continue
 call=m[1];calls.append((i+1,call));op=None
 if call=='InitVRAMAndCRAM':op=emit('INIT',line=i+1)
 elif call=='Pal_FadeIn':op=emit('FADE_IN',line=i+1)
 elif call=='GetDialogueByID':entry=d0
 elif call=='Panel_Create':op=emit('PANEL',d0,i+1)
 elif call=='Panel_Destroy':op=emit('POP',1,i+1)
 elif call=='VInt_PrepareLoop':op=emit('WAIT',d0+1,i+1)
 elif call=='RunText3':op=emit('TEXT',entry,i+1)
 elif call=='Event_GetAndRunDialogue4':op=emit('TEXT',d0,i+1)
 elif call=='Event_RunDialogue4':op=emit('RESUME',line=i+1)
 elif call=='Pal_IncreaseTone':op=emit('TONE',60,i+1);loopwait=True
 elif call=='loc_58666':op=emit('RESTORE',40,i+1);loopwait=True
 elif call=='VInt_Prepare':
  if loopwait:loopwait=False
  else:op=emit('WAIT',1,i+1)
 elif call=='DMAPlanes_VInt':
  # Panel_Create's native compositor publishes the source DMA tick itself.
  if last!='PANEL':op=emit('PUBLISH',line=i+1)
 elif call=='DMAPlane_A_VInt':pass # production RunText owns its publication
 elif call in ['Window_Create','Window_Destroy','Map_LoadChunks']:pass # production RunText/compositor owns these
 elif call=='loc_5B168':op=emit('DOWN',delay,i+1)
 elif call=='loc_5B1B8':op=emit('UP',delay,i+1)
 elif call=='DoDMAPlanesLoop':op=emit('WAIT',d0+1,i+1)
 else:raise AssertionError((i+1,call))
 if op:last=op
h='/* Generated from pinned US Cutscene_Ending through loc_79BD4. */\n'
h+='enum{'+','.join('R45_END_'+k for k in kinds)+'};\nstruct R45EndingOp{uint16_t value;uint8_t kind;};\nstatic const struct R45EndingOp r45_ending_ops[]={\n'
h+=''.join('{%du,R45_END_%s}, /* source line %d */\n'%(v,k,line) for k,v,line in ops)+'};\n';(R/'gba/r45_ending_script.h').write_text(h)
(O/'R45_ENDING_SCRIPT_SOURCE.json').write_text(json.dumps({'source':'pinned US ps4.asm Cutscene_Ending','source_line_range':[start+1,end+1],'operations':[{'kind':k,'value':v,'line':l} for k,v,l in ops],'calls':calls,'scope':__doc__},indent=2)+'\n');print('Generated',len(ops),'operations from',len(calls),'source calls; no asset regeneration')
