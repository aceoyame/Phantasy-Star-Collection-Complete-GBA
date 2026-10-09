#!/usr/bin/env python3
"""Source-semantics oracle for PSIV MapDataMan_* object clears (subset 68K interpreter over ps4.asm text)."""
import re, json, itertools, sys
import os
D=os.environ.get('PS4DISASM','ps4disasm')+'/'
A=open(D+'ps4.asm',encoding='latin-1').read().split('\n')
C=open(D+'ps4.constants.asm',encoding='latin-1').read()
const={}
for m in re.finditer(r'^(\w+)\s*=\s*ramaddr\(\$([0-9A-Fa-f]+)\)',C,re.M): const[m.group(1)]=int(m.group(2),16)
for m in re.finditer(r'^(\w+)\s*=\s*(\$[0-9A-Fa-f]+|\d+)\b',C,re.M):
    v=m.group(2); const[m.group(1)]=int(v[1:],16) if v.startswith('$') else int(v)
# manager table
tab={}
start=next(i for i,l in enumerate(A) if 'bra.w\tMapDataMan_' in l)
for l in A[start:start+400]:
    m=re.match(r'\tbra\.w\t(MapDataMan_\w+)\t; \$?([0-9A-F]+)',l)
    if m: tab[int(m.group(2),16)]=m.group(1)
    elif tab and not l.strip().startswith('bra') and l.strip() and not l.startswith(';'): break
labels={}
for i,l in enumerate(A):
    m=re.match(r'^(\w+):',l)
    if m: labels[m.group(1)]=i
def flagval(tok):
    tok=tok.lstrip('#')
    if tok in const: return const[tok]
    if tok.startswith('$'): return int(tok[1:],16)
    return int(tok)
SLOT_BASE=0xC300
class Unsupported(Exception): pass
def run(label,flags):
    """flags: dict type->set.  returns (cleared slots set, touched flags list)"""
    pc=labels[label]+1; d0=None; z=None; a0=None; cleared=set(); typed={}; steps=0; touched=[]; writes=[]; dreg={}; flags={k:set(v) for k,v in flags.items()}
    while steps<400:
        steps+=1; l=A[pc]; s=l.split(';')[0].strip(); s=re.sub(r'\ba4\b','a0',s)   # single object pointer register (a0 or a4)
        if not s or s.endswith(':') or re.match(r'^\w+:$',s) or s in('+','-','++','--') or re.match(r'^[+-]+$',s): pc+=1; continue
        if re.match(r'^\w+:',l) and not l.startswith('\t'): pc+=1; continue
        m=re.match(r'(move\.w|moveq|move\.b)\s+(#\w+|#\$[0-9A-F]+),\s*d0$',s)
        if m: d0=flagval(m.group(2)); pc+=1; continue
        m=re.match(r'(jsr|bsr\.w|bsr\.s|bsr)\s+\(?(\w+)\)?(\.l)?$',s)
        if m:
            f=m.group(2)
            t={'EventFlags_Test':0,'ChestFlags_Test':1,'TempEveFlags_Test':2}.get(f)
            w={'EventFlags_Set':(0,1),'EventFlags_Clear':(0,0),'ChestFlags_Set':(1,1),'ChestFlags_Clear':(1,0),'TempEveFlags_Set':(2,1),'TempEveFlags_Clear':(2,0)}.get(f)
            if t is not None:
                touched.append((t,d0)); z = (d0 not in flags.get(t,set())); pc+=1; continue
            if w is not None:
                writes.append((w[0],d0,w[1]))
                if w[1]: flags.setdefault(w[0],set()).add(d0)
                else: flags.setdefault(w[0],set()).discard(d0)
                pc+=1; continue
            pc+=1; continue   # other subroutines: side effects outside object/flag model
        m=re.match(r'moveq\s+#\$?([0-9A-F]+),\s*d([1-7])$',s)
        if m: dreg[int(m.group(2))]=int(m.group(1),16) if '$' in l.split(',')[0] else int(m.group(1)); pc+=1; continue
        m=re.match(r'dbf\s+d([0-7]),\s*(\S+)$',s)
        if m:
            r=int(m.group(1))
            if r==0: d0=(d0 if d0 is not None else 0)-1; cont=(d0!=-1)
            else: dreg[r]=dreg.get(r,0)-1; cont=(dreg[r]!=-1)
            if cont:
                tgt=m.group(2)
                if re.match(r'^-+$',tgt):
                    k=len(tgt); j=pc
                    while k: j-=1; k-= 1 if A[j].strip()=='-' else 0
                    pc=j+1; continue
                if tgt in labels: pc=labels[tgt]+1; continue
                raise Unsupported('dbf '+tgt)
            pc+=1; continue
        m=re.match(r'move\.w\s+(d0|#\$?[0-9A-Fa-f]+|#\w+),\s*(\$?[0-9A-F]*|curr_x_pos|curr_y_pos)\(a0\)$',s)
        if m:
            if a0 is None: raise Unsupported('a0 unknown')
            src,off=m.group(1),m.group(2)
            if off in('curr_x_pos','curr_y_pos'): pc+=1; continue
            off=int(off[1:],16) if off else 0
            addr=(a0+off)&0xFFFF
            if SLOT_BASE<=addr<SLOT_BASE+0x40*32 and (addr-SLOT_BASE)%0x40==0:
                v=d0 if src=='d0' else flagval(src)
                slot=(addr-SLOT_BASE)//0x40
                if v==0: cleared.add(slot); typed.pop(slot,None)
                else: typed[slot]=v; cleared.discard(slot)
            pc+=1; continue
        if re.match(r'(cmp|tst|btst|cmpi|cmpa)',s): raise Unsupported('data-dependent: '+s)
        if re.match(r'(move|lea|addi|subi|add|sub|andi|ori|and|or|eor|lsl|lsr|asl|asr|swap|ext|clr\.b|clr\.l|bset|bclr|st|sf|neg|not|movem|mulu|divu)',s) and not re.match(r'clr\.w\s+[\$0-9A-F]*\(a0\)',s) and not re.match(r'lea\s+(\$[0-9A-F]+\(a0\)|\((\$?[0-9A-F]+|\w+)\)\.w),\s*a0$',s) and not re.match(r'(move\.w|moveq|move\.b)\s+(#\w+|#\$[0-9A-F]+),\s*d0$',s):
            if re.search(r',\s*d0$',s) or re.search(r',\s*a0$',s): raise Unsupported('d0/a0 clobber: '+s)
            pc+=1; continue
        m=re.match(r'(beq|bne|bra)(\.[sw])?\s+(\S+)$',s)
        if m:
            op,tgt=m.group(1),m.group(3)
            take = op=='bra' or (op=='beq' and z) or (op=='bne' and not z)
            if not take: pc+=1; continue
            if re.match(r'^\++$',tgt):
                k=len(tgt); j=pc
                while k: j+=1; k-= 1 if A[j].strip()=='+' else 0
                pc=j+1; continue
            if re.match(r'^-+$',tgt):
                k=len(tgt); j=pc
                while k: j-=1; k-= 1 if A[j].strip()=='-' else 0
                pc=j+1; continue
            if tgt in labels: pc=labels[tgt]+1; continue
            raise Unsupported('branch '+tgt)
        m=re.match(r'lea\s+\$([0-9A-F]+)\(a0\),\s*a0$',s)
        if m:
            if a0 is None: raise Unsupported('a0 unknown')
            a0=(a0+int(m.group(1),16))&0xFFFFFFFF; pc+=1; continue
        m=re.match(r'lea\s+\((\$?[0-9A-F]+|\w+)\)\.w,\s*a0$',s)
        if m:
            v=m.group(1)
            if v=='Field_Obj_Secondary': a0=0xFFFFC300
            elif v.startswith('$'): a0=0xFFFF0000|(int(v[1:],16)&0xFFFF)
            elif v in const: a0=0xFFFF0000|(const[v]&0xFFFF)
            else: raise Unsupported('lea '+v)
            pc+=1; continue
        m=re.match(r'clr\.w\s+\((\$[0-9A-F]+|\w+)\)\.w$',s)
        if m:
            v=m.group(1); addr=(int(v[1:],16)&0xFFFF) if v.startswith('$') else (0xC4C0 if v=='Alys_Piata' else 0xC300 if v=='Field_Obj_Secondary' else None)
            if addr is None: raise Unsupported('clr '+v)
            if addr>=SLOT_BASE and (addr-SLOT_BASE)%0x40==0 and addr<SLOT_BASE+0x40*32: cleared.add((addr-SLOT_BASE)//0x40); pc+=1; continue
            pc+=1; continue
        m=re.match(r'clr\.w\s+(\$?[0-9A-F]*)\(a0\)$',s)
        if m:
            off=int(m.group(1)[1:],16) if m.group(1) else 0
            if a0 is None: raise Unsupported('a0 unknown')
            addr=(a0+off)&0xFFFF
            if addr<SLOT_BASE or addr>=SLOT_BASE+0x40*32: pc+=1; continue   # not an object slot (RAM side effect)
            if (addr-SLOT_BASE)%0x40: raise Unsupported('clr %04X'%addr)
            cleared.add((addr-SLOT_BASE)//0x40); pc+=1; continue
        if re.match(r'moveq\s+#0,\s*d7$',s): pc+=1; continue
        if s in('rts','jmp\t(RunEvent_NoEvent).l'): return (cleared,typed),touched,writes
        m=re.match(r'jmp\s+\(?(\w+)\)?(\.l)?$',s)
        if m and m.group(1) in ('EventFlags_Set','EventFlags_Clear','TempEveFlags_Set','TempEveFlags_Clear','ChestFlags_Set','ChestFlags_Clear'):
            w={'EventFlags_Set':(0,1),'EventFlags_Clear':(0,0),'ChestFlags_Set':(1,1),'ChestFlags_Clear':(1,0),'TempEveFlags_Set':(2,1),'TempEveFlags_Clear':(2,0)}[m.group(1)]
            writes.append((w[0],d0,w[1])); return (cleared,typed),touched,writes
        raise Unsupported(s)
    raise Unsupported('steps')
cases=[]; unsupported={}
for mid,lab in sorted(tab.items()):
    # discover flags by running with none, then enumerate touched combos (<=5 flags)
    try:
        _,t,_=run(lab,{})
    except (Unsupported,KeyError,ValueError,IndexError,TypeError) as e: unsupported[mid]=(lab,str(e)); continue
    fl=[]
    frontier=[{}]; seen=set()
    # iterative discovery of all flags along any path
    for _ in range(6):
        new=[]
        for fs in frontier:
            try: _,t,_=run(lab,fs)
            except (Unsupported,KeyError,ValueError,IndexError,TypeError) as e: unsupported[mid]=(lab,str(e)); break
            for x in t:
                if x not in fl: fl.append(x)
        combos=[]
        for bits in range(1<<len(fl)):
            fs={}
            for i,(tt,f) in enumerate(fl):
                if bits>>i&1: fs.setdefault(tt,set()).add(f)
            combos.append(fs)
        frontier=combos
        if len(fl)>6: break
    if mid in unsupported: continue
    for fs in frontier:
        (cl,ty),_,wr=run(lab,fs)
        cases.append({'id':mid,'label':lab,'flags':{str(k):sorted(v) for k,v in fs.items()},'src_cleared':sorted(cl),'src_types':{str(k):v for k,v in ty.items()},'src_writes':wr})
json.dump({'cases':cases,'unsupported':{hex(k):v for k,v in unsupported.items()}},open('src_cases.json','w'),indent=0)
import collections
print(collections.Counter(v[1].split(':')[0][:28] for v in unsupported.values()).most_common(8))
print(len(tab),'managers;',len(set(c['id'] for c in cases)),'modelled;',len(cases),'flag cases;',len(unsupported),'use other side effects (not modelled)')
