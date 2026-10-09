import subprocess,glob,sys
lines=subprocess.run(['llvm-objdump','-t','/home/claude/work/handoff/CLAUDE_R48_CP07_HANDOFF/Release48/source/psc8/build/z80a.s.o'],capture_output=True,text=True).stdout.splitlines()
syms=[]
for l in lines:
    p=l.split()
    if len(p)>=5 and p[2]=='.hot.z80a': syms.append((int(p[0],16),p[-1]))
syms.sort(); sz={}
end=int(sys.argv[1],16) if len(sys.argv)>1 else 0x4900
for i,(a,n) in enumerate(syms): sz[n]=(syms[i+1][0] if i+1<len(syms) else end)-a
used={}
for f in glob.glob('/tmp/claude-0/sp/ops*_*.txt'):
    for l in open(f):
        _,k,c=l.split(); g,o=k.split('_'); n={'op':'op_','ed':'ed_','xy':'xy_'}.get(g,g+'_')+o; used[n]=used.get(n,0)+int(c)
cand=sorted(((v,k) for k,v in sz.items() if k.startswith(('op_','ed_','xy_')) and k not in used),reverse=True)
print('unused',len(cand),sum(v for v,_ in cand)); print([k for v,k in cand])
rare=sorted(((used[k],k,sz[k]) for k in sz if k in used and k.startswith(('op_','ed_','xy_'))))[:40]
print('rarest used (count,name,bytes):',rare)
