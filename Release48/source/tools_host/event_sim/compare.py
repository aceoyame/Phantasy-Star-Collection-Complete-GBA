import json,collections,sys
d=json.load(open('src_cases.json'))['cases']; E=json.load(open('expect_flags.json'))
bad=collections.defaultdict(list)
for c,e,l in zip(d,E,open('port.txt')):
    parts=l.split(); pid,m=int(parts[0]),int(parts[1])
    ti=next(i for i,p in enumerate(parts) if p.startswith('T')); tstr=parts[ti][1:]
    ptypes={int(a):int(b) for a,b in (x.split('=') for x in tstr.split(',') if x)}
    rest=''.join(p for p in parts[ti+1:] if '.' in p)
    pf=set(tuple(map(int,x.split('.'))) for x in rest.split(',') if x)
    ef=set((int(t),x) for t,v in e.items() for x in v)
    sm=sum(1<<x for x in c['src_cleared']); st={int(k):v for k,v in c['src_types'].items() if v!=0x38}
    issues=[]
    if sm!=m: issues.append(f"clears src={c['src_cleared']} port={[i for i in range(32) if m>>i&1]}")
    if st!=ptypes: issues.append(f"types src={st} port={ptypes}")
    if pf!=ef: issues.append(f"flags extra={sorted(pf-ef)} missing={sorted(ef-pf)}")
    if issues: bad[(hex(c['id']),c['label'])].append((c['flags'],issues))
print('managers',len(set(c['id'] for c in d)),'cases',len(d),'| managers with mismatches',len(bad))
for k,v in bad.items(): print(k,f'{len(v)} cases; e.g.',v[0])
sys.exit(1 if bad else 0)
