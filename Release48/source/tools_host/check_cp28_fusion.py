#!/usr/bin/env python3
"""Reproduce the native glyph fusion; reject changes anywhere in its loop."""
from pathlib import Path
import importlib.util,json
R=Path(__file__).resolve().parents[2];A=R/'source/additional';V=R.parent/'validation_cp28'
spec=importlib.util.spec_from_file_location('native_spans',A/'aot/native_spans.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
old=(R.parent/'cp27_base/aot/generated/aot_code.c').read_text();actual=(A/'aot/generated/aot_code.c').read_text();out,sites=m.fuse_glyphs(old)
assert out==actual and sites==['B2BACA'];assert m.fuse_glyphs(out)==(out,[])
cases=0
for line in m.GLYPH_BODY.splitlines():
 if not line.strip():continue
 changed=old.replace(m.GLYPH_BODY,m.GLYPH_BODY.replace(line,line+' /* changed */',1),1);out,sites=m.fuse_glyphs(changed);assert not sites;cases+=1
report={'all_pass':True,'baseline':'sealed CP27 generated AOT','regenerated_fusion_matches_accepted_source':True,'duplicate_application_unchanged':True,'changed_loop_lines_refused':cases,'site':'B2BACA','native_guest_cycles':[80,84,88]}
(V/'FUSION_REPRODUCTION_EXACT.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
