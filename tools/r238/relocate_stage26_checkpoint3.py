"""Exact checkpoint-3 binary relocation; source only, no embedded ROM data."""
import hashlib,struct
BASE=0x08000000
N=32*1024*1024
R237="a84e9c0501c16a2b750a85037cfa16bcd859101fb4b201ff4edaa1da6b2742de"
R238="20f4fa76fb591b453a9c28b2b650a4dc1259904b083cbe0cc3129fb638a2c558"
STAGE26="3154fb729fdb016574d63dff8dea84e01f1b77e2ac346390530d4f90d932a1a0"
OUTPUT="9d9ca1b32cfdd844d648fe00caa9549bd702c193d4753d7de9dc5cadd2b365fa"
C0,C1,C2=0x0922BA00,0x0922DBB0,0x0924E000
M0,M1=0x09238000,0x09260000
TELE=0xE200
MAP_LEN,TELE_LEN=57696,6400
PTRS=(0x922c32c,0x922c5b8,0x922c5bc,0x922ca3c,0x922ccbc,0x922d348,0x922da68,0x922da74,0x922da78,0x922da80,0x922da84,0x922da8c,0x922da90,0x922da98,0x922da9c,0x922daa4,0x922daa8,0x922dab0,0x922dab4,0x922db1c,0x922db20,0x922db24,0x922db28,0x922db2c,0x922db30,0x922db34,0x922db38,0x922db3c,0x922db40)
HOOKS={0x08CA2F98:0x922c38c,0x08CA1FE4:0x922c398,0x08836420:0x922c448,0x08836550:0x922c46c,0x088365E0:0x922c794,0x0882FD68:0x922c490,0x0882FDB8:0x922c490,0x0884FB40:0x922c5c0,0x08816330:0x922c5c0,0x08842658:0x922c5e4,0x088434E4:0x922c604,0x0884372C:0x922c610,0x08862F1C:0x922d350,0x08866F84:0x922c7b4,0x0885D78C:0x922d3a0}
MAP_IMM,TELE_PTR=0x0922D70C,0x0922D344
def sha(x):return hashlib.sha256(x).hexdigest()
def need(ok,msg):
    if not ok:raise ValueError(msg)
def sl(a,b):return slice(a-BASE,b-BASE)
def w(r,a):return struct.unpack_from("<I",r,a-BASE)[0]
def put(r,a,v):struct.pack_into("<I",r,a-BASE,v)
def target(a,v):
    need((v>>25)&7==5 and v>>28!=15,"not ARM branch")
    d=v&0xffffff
    return a+8+4*(d-(0x1000000 if d&0x800000 else 0))
def encode(a,v,t):
    d=t-a-8
    need(d%4==0 and -(1<<25)<=d<(1<<25),"branch range")
    return (v&0xff000000)|((d//4)&0xffffff)
def imm(v):
    x=v&255;r=((v>>8)&15)*2
    return ((x>>r)|(x<<(32-r)))&0xffffffff if r else x
def encode_imm(v,x):
    for i in range(4096):
        if imm(i)==x:return (v&~0xfff)|i
    raise ValueError("immediate cannot be encoded")
def relocate(base,candidate,stage):
    for rom,h in ((base,R237),(candidate,R238),(stage,STAGE26)):
        need(len(rom)==N and sha(rom)==h,"ROM provenance mismatch")
    old=((C0,C1),(M0,M0+TELE+TELE_LEN))
    new=((C2,C2+C1-C0),(M1,M1+TELE+TELE_LEN))
    for a,b in new:
        for rom in (base,candidate,stage):
            need(not any(rom[sl(a,b)]),"relocation destination not blank")
    out=bytearray(stage);delta=C2-C0;conflicts=0
    for i,(a,b,c) in enumerate(zip(base,stage,candidate)):
        if a==c:continue
        addr=i+BASE
        if any(lo<=addr<hi for lo,hi in old):
            conflicts+=int(a!=b and b!=c);continue
        need(a==b,"unexpected shared change at "+hex(addr))
        out[i]=c
    need(conflicts==60810,"unexpected number of artwork conflicts")
    out[sl(C2,new[0][1])]=candidate[sl(C0,C1)]
    out[sl(M1,M1+MAP_LEN)]=candidate[sl(M0,M0+MAP_LEN)]
    out[sl(M1+TELE,M1+TELE+TELE_LEN)]=candidate[sl(M0+TELE,M0+TELE+TELE_LEN)]
    need(tuple(a for a in range(C0,C1,4) if C0<=w(candidate,a)<C1)==PTRS,"literal table")
    for a in PTRS:put(out,a+delta,w(candidate,a)+delta)
    need(tuple(a for a in range(C0,C1,4) if M0<=w(candidate,a)<M0+TELE+TELE_LEN)==(TELE_PTR,),"map literal table")
    need(w(candidate,TELE_PTR)==M0+TELE,"tele literal")
    put(out,TELE_PTR+delta,M1+TELE)
    need(w(candidate,MAP_IMM)==0xe283398e and w(candidate,MAP_IMM+4)==0xe2833409,"map instruction")
    ni=encode_imm(w(candidate,MAP_IMM),M1-0x09000000)
    need(imm(ni)+imm(w(candidate,MAP_IMM+4))==M1,"map address")
    put(out,MAP_IMM+delta,ni)
    for a in range(C0,C1,4):
        v=w(candidate,a)
        if (v>>25)&7==5 and v>>28!=15:
            t=target(a,v)
            need(C0<=t<C1 and w(out,a+delta)==v and target(a+delta,v)==t+delta,"relative branch changed")
    for a,t in HOOKS.items():
        v=w(candidate,a)
        need(w(stage,a)==w(base,a) and target(a,v)==t,"hook conflict")
        put(out,a,encode(a,v,t+delta))
        need(target(a,w(out,a))==t+delta,"hook target")
    for a,b in old:need(out[sl(a,b)]==stage[sl(a,b)],"artwork changed")
    reverted=bytearray(out[sl(C2,new[0][1])])
    for a in (*PTRS,TELE_PTR,MAP_IMM):struct.pack_into("<I",reverted,a-C0,w(candidate,a))
    need(reverted==candidate[sl(C0,C1)],"code other than relocation words changed")
    preserved=sum(a!=b for a,b in zip(base,stage))
    retained=sum(a!=b and b==c for a,b,c in zip(base,stage,out))
    need(preserved==retained==158332,"stage bytes not preserved")
    need(len(out)==N and out[:0xc0]==stage[:0xc0] and sha(out)==OUTPUT,"unexpected final ROM")
    return bytes(out),sum(a!=b for a,b in zip(stage,out))
