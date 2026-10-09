#!/usr/bin/env python3
from pathlib import Path
import sys,json
R=Path(__file__).resolve().parents[2];A=R/'source/additional';V=R.parent/'validation_cp29'
sys.path.insert(0,str(A/'aot'));from native_spans import fuse_rows,fuse_shifted,ROW_BODY,SHIFTED_BODY
base=(R.parent/'cp28_base/aot/generated/aot_code.c').read_text();final=(A/'aot/generated/aot_code.c').read_text()
s,rows=fuse_rows(base);s,shifted=fuse_shifted(s);assert s==final and rows==['B2BC8A'] and shifted==['B2BC4C']
assert fuse_rows(final)==(final,[]) and fuse_shifted(final)==(final,[])
mutations=0
for body,fn in [(ROW_BODY,fuse_rows),(SHIFTED_BODY,fuse_shifted)]:
 for i,line in enumerate(body.splitlines(keepends=True)):
  if not line.strip():continue
  lines=body.splitlines(keepends=True);lines[i]=line.rstrip('\n')+' /* changed */\n';mutated=base.replace(body,''.join(lines));assert fn(mutated)==(mutated,[]),(i,line);mutations+=1
out={'all_pass':True,'baseline':'sealed CP28','exact_reproduction':True,'idempotent':True,'changed_loop_lines_refused':mutations,'sites':rows+shifted}
(V/'FUSION_REPRODUCTION_EXACT.json').write_text(json.dumps(out,indent=2)+'\n');print(out)
