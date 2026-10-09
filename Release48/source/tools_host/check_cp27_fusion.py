#!/usr/bin/env python3
"""Reproduce native text fusions; reject edits anywhere in each source loop."""
from pathlib import Path
import importlib.util,json
R=Path(__file__).resolve().parents[2];A=R/'source/additional';V=R.parent/'validation_cp27'
spec=importlib.util.spec_from_file_location('native_spans',A/'aot/native_spans.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
original=(R.parent/'cp26_base/aot/generated/aot_code.c').read_text();expected=(A/'aot/generated/aot_code.c').read_text()
result,sites=m.fuse_text_scans(original);assert result==expected and set(sites)==set(m.SCAN_BODIES)
assert m.fuse_text_scans(result)==(result,[])
cases=0
for label,body in m.SCAN_BODIES.items():
 for i,line in enumerate(body.splitlines()):
  if not line.strip():continue
  changed=original.replace(body,body.replace(line,line+' /* changed */',1),1)
  out,applied=m.fuse_text_scans(changed);assert label not in applied;cases+=1
report={'all_pass':True,'baseline':'sealed CP26 generated AOT','regenerated_fusion_matches_accepted_source':True,'duplicate_application_unchanged':True,'changed_loop_lines_refused':cases,'sites':sites,'exits':'class match/terminator and kerning match/terminator preserve original generated destinations'}
(V/'FUSION_REPRODUCTION_EXACT.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
