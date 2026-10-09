"""Record PSIV battle object/sprite state per frame from the Genesis core."""
import ctypes, struct
from mdbattle import *
L.md_snap_buf.restype=ctypes.POINTER(ctypes.c_uint8)
L.md_bo_buf.restype=ctypes.POINTER(ctypes.c_uint8)
L.md_planeb.restype=ctypes.POINTER(ctypes.c_uint8)
RUNOBJ_RTS=0x367B4
ROMB=open(ROM,'rb').read()
def snap():
    return bytes(ctypes.string_at(ctypes.cast(L.md_snap_buf(),ctypes.c_void_p),0x1000))
def built_objs():
    """Objects exactly as they were when Battle_BuildSprites drew them this frame."""
    n=L.md_bo_n(); b=bytes(ctypes.string_at(ctypes.cast(L.md_bo_buf(),ctypes.c_void_p),64*66))
    out=[]
    for k in range(n):
        slot=b[k*66]; o=b[k*66+2:k*66+66]
        t=struct.unpack('>H',o[0:2])[0]
        if not t: continue
        out.append(dict(slot=slot,type=t&0x7FFC,raw=o,flags=o[2],maps=struct.unpack('>I',o[0xC:0x10])[0]&0xFFFFFF,
            p1=o[0x13],p2=struct.unpack('>H',o[0x1E:0x20])[0],x=struct.unpack('>h',o[0x2C:0x2E])[0],y=struct.unpack('>h',o[0x2E:0x30])[0],routine=struct.unpack('>H',o[0x14:0x16])[0],
            parent=struct.unpack('>I',o[0x3C:0x40])[0]&0xFFFF, target=struct.unpack('>I',o[0x38:0x3C])[0]&0xFFFF))
    return out
def objs(sn):
    out=[]
    for i in range(64):
        o=sn[i*64:(i+1)*64]
        t=struct.unpack('>H',o[0:2])[0]
        if not t: continue
        if not (o[0]&0x80): continue   # not yet initialised -> was not drawn this frame
        out.append(dict(slot=i,type=t&0x7FFC,raw=o,flags=o[2],maps=struct.unpack('>I',o[0xC:0x10])[0]&0xFFFFFF,
            p1=o[0x13],p2=struct.unpack('>H',o[0x1E:0x20])[0],x=struct.unpack('>h',o[0x2C:0x2E])[0],y=struct.unpack('>h',o[0x2E:0x30])[0],routine=struct.unpack('>H',o[0x14:0x16])[0],
            parent=struct.unpack('>I',o[0x3C:0x40])[0]&0xFFFF, target=struct.unpack('>I',o[0x38:0x3C])[0]&0xFFFF))
    return out
def rdmem(m,a):
    a&=0xFFFFFF
    if a>=0xFF0000: return m.r8(a)
    return ROMB[a]
def pieces(m,o):
    """Reproduce Battle_FillSpriteAttributes for one object -> list of (x,y,size,tileword)."""
    if o['flags']&2: return []
    a=o['maps']; n=rdmem(m,a)+1; a+=1
    V=(o['p2']>>5)&0xFFFF; res=[]
    for k in range(n):
        yo=rdmem(m,a); yo=yo-256 if yo>127 else yo; size=rdmem(m,a+1); hi=rdmem(m,a+2); lo=rdmem(m,a+3); xo=rdmem(m,a+4); xa=rdmem(m,a+5); a+=6
        hb=((V>>8)+o['p1'])&0xFF
        if o['flags']&0x10: hb|=0x80
        hb=(hb+hi)&0xFF
        lb=(V&0xFF)+lo
        tw=((hb<<8)|(lb&0xFF))
        if lb>0xFF: tw=(tw+0x100)&0xFFFF
        if o['flags']&0x80:
            tw^=0x0800; xo=xa
        xo=xo-256 if xo>127 else xo
        X=o['x']+xo
        if not (0<= (X-0x41)&0xFFFF < 0x17F): continue
        res.append((X,o['y']+yo,size,tw))
    return res
def record(m,frames,keys=0,stop=None):
    L.md_set_snap(RUNOBJ_RTS); L.md_watch(0x500A,0x500B)
    rec=[]; wn=0
    for f in range(frames):
        if stop is not None and f>2 and stop(): break
        c0=L.md_bo_frames(); m.run(1,keys)
        sn=built_objs() if L.md_bo_frames()!=c0 else []
        n=L.md_watch_n(); lg=L.md_watch_log(); sfx=[lg[i*3+1]&0xFF for i in range(wn,n)]; wn=n
        rec.append(dict(objs=sn, cram=m.cram(), vram=m.vram(), img=m.shot(), sfx=[s for s in sfx if s], ram=bytes(m.ram()), regs=m.regs(), vsram=[L.md_vsram()[i] for i in range(40)], planeb=ctypes.string_at(ctypes.cast(L.md_planeb(),ctypes.c_void_p),224*320)))
    L.md_set_snap(0xFFFFFFFF)
    return rec
def render_pieces(vram,cram,plist,img=None):
    from PIL import Image
    if img is None: img=Image.new('RGBA',(320,224),(0,0,0,0))
    px=img.load()
    for (X,Y,size,tw) in plist:
        w=((size>>2)&3)+1; h=(size&3)+1; pal=(tw>>13)&3; hf=tw&0x800; vf=tw&0x1000; t0=tw&0x7FF
        for cy in range(h*8):
            for cx in range(w*8):
                lx=w*8-1-cx if hf else cx; ly=h*8-1-cy if vf else cy
                t=t0+(lx>>3)*h+(ly>>3); b=vram[(t*32+(ly&7)*4+((lx&7)>>1))&0xFFFF]; ci=(b&15) if lx&1 else b>>4
                if not ci: continue
                sx=X-128+cx; sy=Y-128+cy
                if 0<=sx<320 and 0<=sy<224:
                    c=cram[pal*16+ci]; px[sx,sy]=(((c>>1)&7)*36,((c>>5)&7)*36,((c>>9)&7)*36,255)
    return img

def setup_enemy_battle(eid, settle=380, bg_event=18, form=None):
    m=MD(); m.load('/home/claude/w/md/st_field2.bin')
    import mdbattle
    if mdbattle._party is None: mdbattle._party=gba_party()
    slots,chars=mdbattle._party
    m.setram(0xF40A,slots); m.setram(0xF500,chars)
    m.w8(0xECFC,bg_event)
    assert run_until_break(m,0x5199A,30)
    L.md_set_reg(16,0x5A608)
    assert run_until_break(m,0x7F22,200)
    m.setram(0x41F4,form if form is not None else bytes([1,1,0,eid,0x14,0xFF,0xFF,0xFF]))
    m.run(settle)
    return m
def fighters(m):
    out=[]
    for i in range(9):
        b=0x4400+i*0x40
        out.append(dict(id=m.r16(b+0x12),x=struct.unpack('>h',bytes([m.r8(b+0xC),m.r8(b+0xD)]))[0],y=struct.unpack('>h',bytes([m.r8(b+0xE),m.r8(b+0xF)]))[0],routine=m.r16(b+2)))
    return out
def record_enemy_ability(eid, ability, frames=360, m=None):
    if m is None: m=setup_enemy_battle(eid)
    ok=False
    for _att in range(4):          # boss intros (EE87) can swallow the first command round
        L.md_set_break(0xD024)         # armed during the menu presses too (enemy-first rounds)
        md_round_defend(m)
        hit=bool(L.md_break_hit()) or run_until_break(m,0xD024,900)
        while hit:
            a4=L.md_reg(12)
            if m.r16((a4&0xFFFF)+0x12)==eid: ok=True; break   # another formation member acting: let it finish
            L.md_set_break(0xFFFFFFFF); m.run(1)               # step off the breakpoint PC
            hit=run_until_break(m,0xD024,900)
        L.md_set_break(0xFFFFFFFF)
        if ok: break
    if not ok: return None,m
    a4=L.md_reg(12)
    if ability is not None: m.w16(a4+0x24,ability)
    fi=fighters(m)
    tgt=m.r16(0x4144)
    actor=m.r16(0x4142)   # stop when the next actor's turn begins (party items/techs, victory windows)
    rec=record(m,frames,stop=lambda: m.r16(0x4142)!=actor)
    return dict(eid=eid,ability=ability,actor=actor,fighters=fi,rec=rec,enemy_slot=((a4&0xFFFF)-0x4400)//0x40,caster_fighter=((a4&0xFFFF)-0x4400)//0x40+1,target_fighter=tgt,a4=a4),m

DO_ENTRY={'tech':0x0,'skill':0x0}
def _lst_addr(label):
    import re
    for line in open('/home/claude/w/R45/source/references/disassembly/ps4disasm/ps4disasm/ps4.lst',errors='ignore'):
        m_=re.match(r'\s*\d+/\s*([0-9A-F]+) :\s+'+re.escape(label)+r':\s*$',line)
        if m_: return int(m_.group(1),16)
    raise KeyError(label)
def setup_party_battle(form=bytes([4,0x0f,0,0x6d,0x08,0x6d,0x10,0x6d,0x18,0x6d,0x20,0xff]),settle=380,bg_event=18):
    m=MD(); m.load('/home/claude/w/md/st_field2.bin')
    import mdbattle
    if mdbattle._party is None: mdbattle._party=gba_party()
    slots,chars=mdbattle._party
    m.setram(0xF40A,slots); m.setram(0xF500,chars)
    m.w8(0xECFC,bg_event)
    assert run_until_break(m,0x5199A,30); L.md_set_reg(16,0x5A608)
    assert run_until_break(m,0x7F22,200)
    m.setram(0x41F4,form)
    m.run(settle)
    return m
def record_party_ability(kind,aid,caster,target,m,frames=600,cmds=None):
    """kind: 'tech'(2)/'skill'(3). caster: party slot 0-4. target: fighter number (1-5 party, 6-9 enemies)."""
    m.press('C',3,30)
    for k in range(4):
        for _ in range(4): m.press('RIGHT',3,8)
        m.press('C',3,25)
    typ={'tech':2,'skill':3}[kind]
    if cmds is None: cmds={caster:(typ,aid,target)}
    for s,(t,i,tg) in cmds.items(): m.setram(0x410A+s*4,bytes([t,i,0,tg]))
    for _ in range(4): m.press('RIGHT',3,8)
    L.md_set_snap(RUNOBJ_RTS)
    m.press('C',3,0)
    entry=_lst_addr('Character_DoTech' if kind=='tech' else 'Character_DoSkill')
    if not run_until_break(m,entry,900): return None
    fi=fighters(m)
    actor=m.r16(0x4142)
    rec=record(m,frames,stop=lambda: m.r16(0x4142)!=actor)
    return dict(kind=kind,a=aid,b=0,fighters=fi,rec=rec,caster_slot=caster,caster_fighter=caster+1,target_fighter=target)

def record_enemy_intro(eid, frames=600, form=None, wait=500):
    """EE87 bosses act before the first command round (EnemyInit sets $FFEE87; the
    attack routine then spawns its intro object).  Record that action untouched."""
    m=setup_enemy_battle(eid,settle=0,form=form)
    if not run_until_break(m,0xD024,wait): return None,m
    a4=L.md_reg(12); fi=fighters(m); tgt=m.r16(0x4144); ee87=m.r8(0xEE87)
    actor=m.r16(0x4142)
    rec=record(m,frames,stop=lambda: m.r16(0x4142)!=actor)
    return dict(eid=eid,ability=0,ee87=ee87,fighters=fi,rec=rec,enemy_slot=((a4&0xFFFF)-0x4400)//0x40,caster_fighter=((a4&0xFFFF)-0x4400)//0x40+1,target_fighter=tgt,a4=a4),m

COMBO_SETUP=0x28549C
def combo_setups():
    """ComboSetupData: 14 records [count(|$80 ordered), entries of count bytes..., $FF]."""
    a=COMBO_SETUP; out=[]
    for k in range(14):
        cnt=ROMB[a]; n=cnt&0x7F; a+=1; ents=[]
        while ROMB[a]!=0xFF: ents.append(tuple(ROMB[a:a+n])); a+=n
        a+=1; out.append((cnt,ents))
    return out
def record_party_combo(k,m,frames=900):
    """Combo k (1..14): first ComboSetupData entry, participants = party slots 0..n-1.
    Recorder-only: the turn-order list at $FFEFAC is rewritten at the first
    loc_27DE7C call so the participants act consecutively."""
    cnt,ents=combo_setups()[k-1]; part=ents[0]; n=len(part)
    m.press('C',3,30)
    for s in range(4):
        for _ in range(4): m.press('RIGHT',3,8)
        m.press('C',3,25)
    def sa_of(j): return (m.r16(0x4400+j*0x40+4)<<16|m.r16(0x4400+j*0x40+6))&0xFFFF
    # prefer the party member who really owns a skill (character-specific palette/pose objects)
    slots=[None]*n; used=set()
    for i_,b in enumerate(part):
        if b&0x80:
            for j in range(5):
                if j not in used and (b&0x7F) in bytes(m.r8(sa_of(j)+0x62+q) for q in range(8)): slots[i_]=j; used.add(j); break
    for i_ in range(n):
        if slots[i_] is None:
            j=next(j for j in range(5) if j not in used); slots[i_]=j; used.add(j)
    order=[]
    for i_,b in enumerate(part):
        j=slots[i_]; order.append(j)
        typ,aid=(3,b&0x7F) if b&0x80 else (2,b)
        m.setram(0x410A+j*4,bytes([typ,aid,0,6]))
        sa=sa_of(j)
        if typ==3:   # make sure the skill is in the list with uses left (Skill list $62.., uses $6A..)
            lst=[m.r8(sa+0x62+q) for q in range(8)]
            q=lst.index(aid) if aid in lst else 0
            m.w8(sa+0x62+q,aid); m.w8(sa+0x6A+2*q,9)
        else:
            m.w16(sa+0x12,999)
    for _ in range(4): m.press('RIGHT',3,8)
    L.md_set_break(0x27DE7C)
    m.press('C',3,0)
    if not (L.md_break_hit() or run_until_break(m,0x27DE7C,900)): return None
    pos=m.r16(0x4140)
    for i_,j in enumerate(order): m.w16(0xEFAC+(pos+i_)*4,j+1)
    L.md_set_break(0xFFFFFFFF)
    if not run_until_break(m,0xAB98,900): return None
    fi=fighters(m); actor=m.r16(0x4142)
    rec=record(m,frames,stop=lambda: m.r16(0x4142)!=actor)
    return dict(kind='combo',a=k,b=0,part=part,slots=order,fighters=fi,rec=rec,caster_slot=order[0],caster_fighter=order[0]+1,target_fighter=6)
