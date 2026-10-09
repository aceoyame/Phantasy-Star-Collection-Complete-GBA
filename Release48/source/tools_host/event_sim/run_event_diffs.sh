#!/bin/sh
# R16 source-semantics differentials (x86 gcc/g++, python3; no ARM toolchain):
#   1. MapDataMan_* object clears / type writes / flag writes  (src_mgr.py  vs field_apply_map_manager_load)
#   2. RunEvent_*   dispatch                                     (src_runev.py vs field_run_map_event_condition)
# The oracles interpret the ps4disasm routine text (68K subset); routines with palette/scroll/
# position side effects are reported as not modelled.  Expected: 0 mismatches.
set -e
SRC=$(cd "$(dirname "$0")/../.." && pwd); W=${TMPDIR:-/tmp}/ps4_evsim; mkdir -p "$W"
[ -f "$W/ps4disasm/ps4.asm" ] || (cd "$W" && unzip -qo "$SRC/references/disassembly/ps4disasm.zip")
DIS=$(dirname $(find "$W" -name ps4.asm | head -1)); export PS4DISASM="$DIS"
sh "$SRC/tools_host/battle_sim/run_diff_enemy.sh" >/dev/null     # builds host objects into $TMPDIR/ps4_bsim
B=${TMPDIR:-/tmp}/ps4_bsim; D="-DMD2GBA_PS4_MAXMOD_MODE_A=1 -DMD2GBA_PS4_MAXMOD_CHANNEL_MAJOR=1 -DMD2GBA_PS4_V19_FULL_MIX=1 -DMD2GBA_PS4_M4A_NATIVE_ONLY=1"
cd "$W"
python3 - "$SRC" <<'PY'
import sys
s=open(sys.argv[1]+'/gba/title_runtime_v06.c').read()
def body(s,sig,nb):
    i=s.index(sig); j=s.index('\n}\n',i)+3; return s[:i]+sig+nb+'\n}\n'+s[j:]
s=body(s,'static void dma_copy32(const void* src,void* dst,unsigned bytes){','\n    if(!bytes) return; { const uint32_t* a=(const uint32_t*)src; uint32_t* b=(uint32_t*)dst; for(unsigned i=0;i<bytes/4u;i++) b[i]=a[i]; }')
s=body(s,'static void dma_fill32(void* dst,unsigned bytes,uint32_t value){','\n    { uint32_t* b=(uint32_t*)dst; for(unsigned i=0;i<bytes/4u;i++) b[i]=value; }')
i=s.index('static void waitvb(void){unsigned audio_blank_at_entry'); j=s.index('\n',i)
s=s[:i]+'static void waitvb(void){ram16w(0xEF1Cu,(uint16_t)(ram16r(0xEF1Cu)+1u));++native_main_frame_count;}'+s[j:]
open('title_host.c','w').write(s)
h='static int field_dispatch_event_index(unsigned event_index,unsigned* camx,unsigned* camy){'
i=s.index(h)
open('title_host_ev.c','w').write(s[:i]+'static unsigned host_rec_event;\n'+h+'host_rec_event=event_index;(void)camx;(void)camy;return 1;}\nstatic int field_dispatch_event_index_real_unused(unsigned event_index,unsigned* camx,unsigned* camy){'+s[i+len(h):])
PY
python3 "$SRC/tools_host/event_sim/src_mgr.py"
python3 - <<'PY'
import json
d=json.load(open('src_cases.json'))
def fin(c):
    f={int(k):set(v) for k,v in c['flags'].items()}
    for t,x,v in c['src_writes']: (f.setdefault(t,set()).add if v else f.setdefault(t,set()).discard)(x)
    return f
open('cases.txt','w').write(''.join(f"{c['id']} {sum(len(v) for v in c['flags'].values())} "+' '.join(f'{t} {x}' for t,v in c['flags'].items() for x in v)+'\n' for c in d['cases']))
json.dump([{k:sorted(v) for k,v in fin(c).items()} for c in d['cases']],open('expect_flags.json','w'))
d=json.load(open('src_runev.json')) if False else None
PY
OBJS=$(cd "$B" && ls *.o | grep -v diff_enemy.o | sed "s|^|$B/|")
gcc -O1 -w -fno-pie -no-pie $D -I"$W" -I"$SRC/gba" -I"$SRC/gba/audio_p134r/include" -c "$SRC/tools_host/event_sim/mgr_diff.c" -o mgr_diff.o
g++ -no-pie -fno-pie -Wl,-Ttext-segment=0x40000000 -o mgr_diff mgr_diff.o $OBJS 2>/dev/null
./mgr_diff < cases.txt > port.txt
python3 "$SRC/tools_host/event_sim/compare.py"
python3 "$SRC/tools_host/event_sim/src_runev.py"
python3 - <<'PY'
import json
d=json.load(open('src_runev.json'))
open('ev_cases.txt','w').write(''.join(f"{c['id']} {sum(len(v) for v in c['flags'].values())} "+' '.join(f'{t} {x}' for t,v in c['flags'].items() for x in v)+'\n' for c in d['cases']))
PY
gcc -O1 -w -fno-pie -no-pie $D -I"$W" -I"$SRC/gba" -I"$SRC/gba/audio_p134r/include" -c "$SRC/tools_host/event_sim/ev_diff.c" -o ev_diff.o
g++ -no-pie -fno-pie -Wl,-Ttext-segment=0x40000000 -o ev_diff ev_diff.o $OBJS 2>/dev/null
./ev_diff < ev_cases.txt > ev_port.txt
python3 - <<'PY'
import json,sys
d=json.load(open('src_runev.json'))['cases']; P=[tuple(map(int,l.split())) for l in open('ev_port.txt')]
# RunEvent_PiataChazAlone ($7C) is lowered inline (dialogue $6D + flag $15 == Event $A0 body): equivalent.
bad=[(hex(c['id']),c['label'],c['flags'],hex(c['src_event']),hex(ev)) for c,(i,ev,r) in zip(d,P) if c['src_event']!=ev and not (c['id']==0x7C and c['src_event']==0xA0 and r==1)]
print('run events compared',len(set(c['id'] for c in d)),'cases',len(P),'mismatches',len(bad)); [print(b) for b in bad[:20]]
sys.exit(1 if bad else 0)
PY
