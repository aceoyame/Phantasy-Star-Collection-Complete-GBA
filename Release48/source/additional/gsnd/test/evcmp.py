import sys,glob,os,re,collections
def load(p):
    ev=[]
    for line in open(p):
        m=re.match(r'F(\d+) T(\d+) (\S+) ([0-9a-f]+) ([0-9a-f]+)',line)
        if m: ev.append((int(m.group(1)),int(m.group(2)),m.group(3),m.group(4),m.group(5)))
        else:
            m=re.match(r'F(\d+) T(\d+) (PSG\S*) ([0-9a-f]+)',line)
            if m: ev.append((int(m.group(1)),int(m.group(2)),m.group(3),m.group(4),''))
    return ev
tot=0;bad=0;rows=[]
a_tag,b_tag=sys.argv[1],sys.argv[2]
for p in sorted(glob.glob('evcmp/%s_*.log'%a_tag)):
    k=os.path.basename(p)[len(a_tag)+1:]
    q='evcmp/%s_%s'%(b_tag,k)
    if not os.path.exists(q): continue
    A=load(p);B=load(q); tot+=1
    sa=[e[2:] for e in A]; sb=[e[2:] for e in B]
    if sa==sb:
        dt=[abs((a[0]*59659+a[1])-(b[0]*59659+b[1])) for a,b in zip(A,B)]
        rows.append((k,len(A),'same',max(dt) if dt else 0, sum(dt)/len(dt) if dt else 0))
    else:
        bad+=1
        # first divergence
        i=next((j for j,(x,y) in enumerate(zip(sa,sb)) if x!=y),min(len(sa),len(sb)))
        rows.append((k,len(A),'DIFF len %d vs %d, first diff at %d'%(len(sa),len(sb),i),0,0))
print('compared',tot,'identical sequences',tot-bad)
for r in rows:
    if r[2]!='same': print(r)
same=[r for r in rows if r[2]=='same']
if same:
    print('time shift (T-states) among identical: max of max %d, mean of mean %.1f'%(max(r[3] for r in same),sum(r[4] for r in same)/len(same)))
