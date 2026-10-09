#!/usr/bin/env python3
"""Refresh update continuations only, preserving the immutable panel bank."""
from pathlib import Path
import json,re
R=Path(__file__).resolve().parents[1]
s=(R/'gba/title_runtime_v06.c').read_text();a=s.index('static void field_apply_map_update_id');b=s.index('static int ps4_load_field_map',a)
ids={int(x,0) for x in re.findall(r'case\s+(0x[0-9A-Fa-f]+|\d+)u:',s[a:b])};ids.discard(0)
p=R/'generated/psc_runtime_plan_report.json';q=json.loads(p.read_text());flat=[]
for plan in q['plans']:
 if not plan['valid']:continue
 plan['updates']=[x for x in plan['authored_updates'] if x in ids];plan['update_offset']=len(flat);plan['update_count']=len(plan['updates']);flat.extend(plan['updates'])
q['active_update_ids']=sorted(ids);q['active_update_entries']=len(flat);p.write_text(json.dumps(q,indent=2)+'\n')
p=R/'gba/psc_runtime_plan.c';s=p.read_text();s=re.sub(r'const uint8_t ps4_psc_update_ids\[\]=\{[^}]*\};','const uint8_t ps4_psc_update_ids[]={'+','.join('0x%02Xu'%x for x in flat)+'};',s)
fields=['manager_offset','update_offset','event_offset','trans1_offset','trans2_offset','manager_count','update_count','event_count','trans1_count','trans2_count','valid']
rows='\n'.join('{'+','.join(str(plan[k])+'u' for k in fields)+'}, /* %03X */'%i for i,plan in enumerate(q['plans']))
s=re.sub(r'(static const Ps4PscRuntimePlan plans\[0x1A1\]=\{).*?(\n\};)',lambda m:m[1]+'\n'+rows+m[2],s,flags=re.S);p.write_text(s)
print('PASS updated continuations:',len(flat),'/',q['authored_update_entries'],'entries; source order retained')
