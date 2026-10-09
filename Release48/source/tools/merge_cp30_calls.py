#!/usr/bin/env python3
"""Join accepted ROM translations without regenerating any instruction body.

Stack traffic, flags, cycle checks and native fusions remain exactly as supplied.
Only static control transfers and matching RTS continuations become C gotos.
Run after extend_cp30_aot.py; original function IDs remain valid in aot_fns.
"""
from pathlib import Path
import re,json

R=Path(__file__).resolve().parents[2]
A=R/'source/additional/aot/generated'
code=(A/'aot_code.c').read_text();tabs=(A/'aot_tabs.c').read_text()
assert 'CP30_MERGED_CALLS' not in code, 'Run expansion first; already merged'
pat=r'(?:AOT_HOT |__attribute__\(\(section\(".aotextra"\)\)\) )?u32 aotf(\d+)\(u32 entry\)\{.*?\nout: C.sr=\(u16\)sr; C.cycles=cyc; C.pc=npc; return rc;\n\}'
funcs={int(m[1]):m[0] for m in re.finditer(pat,code,re.S)}
tables={n:[tuple(int(x,0) for x in row) for row in re.findall(r'\{(0x[0-9a-f]+)u,(\d+),(\d+),(\d+)\}',b)]
        for n,b in re.findall(r'static const AotEnt AOT_TAB (\w+)\[\]=\{(.*?)\};',tabs,re.S)}
common=tables['rom_common']
chosen=sorted({fn for pc,fn,idx,rr in common if 0x2B786<=pc<0x2BD42})
# A shared function used by another cartridge/RAM table cannot acquire code
# from the seven-game text pipeline without a separate cartridge guard.
chosen=[fn for fn in chosen if all(n=='rom_common' or not any(row[1]==fn for row in rows)
                                  for n,rows in tables.items())]
owner=min(chosen)
entries=[];labels=set();xpcs=[];bodies=[];decls=[];entrymap={};returns=set()
for fn in chosen:
    text=funcs[fn]
    sw=re.search(r' switch\(entry\)\{(.*?)default: npc=C.pc; goto out; \}',text,re.S)[1]
    local=[(int(i),int(pc,16)) for i,pc in re.findall(r'case (\d+): goto B([0-9A-F]+);',sw)]
    for i,pc in local:
        assert pc not in labels, f'Duplicate label {pc:x}'
        entrymap[fn,i]=len(entries);entries.append(pc);labels.add(pc)
    returns.update(int(pc,16) for pc in re.findall(r'W32\(AA\[7\],\(hi\|0x([0-9A-F]+)u\)\)',text))

links=0;rts=0
for fn in chosen:
    text=funcs[fn]
    xs=[int(x,16) for x in re.search(r'xpc\[\] AOT_TAB=\{([^}]*)\}',text)[1].replace('u','').split(',')]
    offset=len(xpcs);xpcs+=xs
    decl=re.search(r'hi=C.pc&0xFF000000u; (.*?)\n switch',text,re.S)[1]
    body=text.split('default: npc=C.pc; goto out; }\n',1)[1].split('\nex: npc=hi|xpc[i];',1)[0]
    # Fold-local names and native span labels were scoped to their old function.
    for name in sorted(set(re.findall(r'\b(?:fx|lrd)\d+\b',decl)),key=len,reverse=True):
        decl=re.sub(r'\b'+name+r'\b',f'{name}_f{fn}',decl)
        body=re.sub(r'\b'+name+r'\b',f'{name}_f{fn}',body)
    decls.append(decl)
    # Protect budget exits, including ones inside existing native macros.
    body=re.sub(r'i=(\d+);\s*goto ex;',lambda m:f'i={offset+int(m[1])}; goto ex;',body)
    body=re.sub(r'(NATIVE_VRAM_COPY\(\d+,)(\d+)(\))',lambda m:m[1]+str(offset+int(m[2]))+m[3],body)
    # Convert only transfers after a complete instruction, never its entry check.
    lines=body.splitlines(keepends=True)
    for i,line in enumerate(lines):
        if re.match(r'B[0-9A-F]+: if\(\(s32\)\(cyc-lim\)',line):continue
        def rel(m):
            global links
            pc=xpcs[int(m[1])]
            if pc not in labels:return m[0]
            links+=1;return f'goto B{pc:X};'
        line=re.sub(r'i=(\d+);\s*goto ex;',rel,line)
        def absolute(m):
            global links
            pc=int(m[1],16)
            if (pc&0xffffff) not in labels:return m[0]
            links+=1
            return f'if(hi==0x{pc&0xff000000:X}u)goto B{pc&0xffffff:X}; '+m[0]
        lines[i]=re.sub(r'npc=0x([0-9A-F]+)u; goto out;',absolute,line)
    body=''.join(lines)
    # RTS still reads/pops the real guest stack. A modified return address,
    # another high byte or a caller outside this region uses the old dispatcher.
    cases=''.join(f'case 0x{pc:X}u: goto B{pc:X};' for pc in sorted(returns&labels))
    replacement=f'if((t&0xFF000000u)==hi){{switch(t&0xFFFFFFu){{{cases}default:break;}}}} npc=t; goto out;'
    rts+=body.count('npc=t; goto out;')
    body=body.replace('npc=t; goto out;',replacement)
    bodies.append(body)

merged='/* CP30_MERGED_CALLS: preserve accepted bodies and every guest boundary. */\n'
merged+=f'u32 aotf{owner}(u32 entry){{\n static const u32 xpc[] AOT_TAB={{'+','.join(f'0x{x:X}u' for x in xpcs)+'};\n'
merged+=' u32 sr=C.sr,cyc=C.cycles,npc,i,rc=AOT_CONT; const u32 lim=aot_lim,hi=C.pc&0xFF000000u; '+''.join(decls)+'\n'
merged+=' switch(entry){'+''.join(f'case {i}:goto B{pc:X};' for i,pc in enumerate(entries))+'default:npc=C.pc;goto out;}\n'
merged+='\n'.join(bodies)+'\nex: npc=hi|xpc[i];\nout: C.sr=(u16)sr; C.cycles=cyc; C.pc=npc; return rc;\n}'
# Function IDs retained as slots, pointing at the combined owner. No duplicate
# old machine code remains linked merely to preserve those IDs.
code=re.sub(pat,lambda m:merged if int(m[1])==owner else '' if int(m[1]) in chosen else m[0],code,flags=re.S)
for name,rows in tables.items():
    rows=[(pc,owner,entrymap[fn,idx],rr) if fn in chosen else (pc,fn,idx,rr) for pc,fn,idx,rr in rows]
    tabs=re.sub(r'(static const AotEnt AOT_TAB '+name+r'\[\]=)\{.*?\};',lambda m:m[1]+'{\n'+''.join('{0x%06xu,%d,%d,%d},\n'%row for row in rows)+'};',tabs,flags=re.S)
def remap(m):
    return m[1]+re.sub(r'aotf(\d+)',lambda f:'aotf'+str(owner if int(f[1]) in chosen else int(f[1])),m[2])+m[3]
tabs=re.sub(r'(const aot_fn AOT_TAB aot_fns\[\]=\{)([^}]*)(\})',remap,tabs)
assert sorted(re.findall(r'CP\d+_NATIVE_\w+',code))==sorted(re.findall(r'CP\d+_NATIVE_\w+',(A/'aot_code.c').read_text()))
(A/'aot_code.c').write_text(code);(A/'aot_tabs.c').write_text(tabs)
report={'functions':chosen,'owner':owner,'entries':len(entries),'direct_transfers':links,'RTS_sites':rts,'matching_return_sites':sorted(returns&labels),'guest_cycles_removed':0,'new_RAM_bytes':0}
(R.parent/'validation_cp30/CALL_MERGE.json').write_text(json.dumps(report,indent=2)+'\n')
print(report)
