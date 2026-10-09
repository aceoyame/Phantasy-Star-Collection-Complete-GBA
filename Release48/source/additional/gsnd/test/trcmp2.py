import sys
def reader(path):
    f=open(path)
    prev=None
    masked=False
    for line in f:
        r=line.split()
        if prev is not None and r[0]==prev[0]:   # LDIR/LDDR: one record per iteration in the reference, keep first
            continue
        prev=r
        if r[0]=='089b': masked=True
        k=(r[0],)+(tuple(r[4:]) if masked else tuple(r[2:]))
        if r[0]=='08b0': masked=False
        yield r,k
A=reader(sys.argv[1]); B=reader(sys.argv[2])
b0=next(B)
n=0
for a in A:
    n+=1
    if a[1]==b0[1] and a[0][1]==b0[0][1]: break
else: print('start not found'); sys.exit(1)
print('REF record',n,'matches AOT line 0')
def nxt(it):
    try: return next(it)
    except StopIteration: return None
off=None; j=0; ctx=[]; shown=0
b=b0
while True:
    if a is None or b is None: print('stream ended after',j,'records (ref None=%s aot None=%s)'%(a is None,b is None)); break
    if a[1]!=b[1] and a[0][0]=='0000' and a[0][1]=='0' and not (b[0][0]=='0000' and b[0][1]=='0'):
        # reference restarts the Z80 (reset) while the AOT trace has no record of the boot instructions: resync on the AOT record
        skipped=0
        while a is not None and not (a[1]==b[1] and a[0][1]==b[0][1]):
            a=nxt(A); skipped+=1
        print('resync at record %d, skipped %d reference records'%(j,skipped))
        off=None
        continue
    if a[1]!=b[1]:
        print('FIRST STATE DIFF at record %d:'%j)
        for c in ctx[-5:]: print('   ',c)
        print(' REF',' '.join(a[0])); print(' AOT',' '.join(b[0]))
        for _ in range(4):
            a=nxt(A); b=nxt(B)
            print(' REF+',' '.join(a[0]) if a else None,'  AOT+',' '.join(b[0]) if b else None)
        sys.exit(0)
    d=int(b[0][1])-int(a[0][1])
    if off is None: off=d
    elif d!=off:
        if shown<20: print('cycle delta changed at record %d pc %s: %d -> %d'%(j,b[0][0],off,d))
        shown+=1; off=d
    ctx.append(' '.join(a[0])); ctx=ctx[-8:]
    j+=1
    a=nxt(A); b=nxt(B)
print('identical state over',j,'records; cycle-delta changes',shown)
