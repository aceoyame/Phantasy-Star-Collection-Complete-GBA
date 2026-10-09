#!/usr/bin/env python3
from pathlib import Path
import subprocess,json,sys
R=Path(__file__).resolve().parents[1];O=R.parent/'build_artifacts'
names=['r31_story','r32_vehicles','r32_travel_maps','r33_story','r33_alys','r34_scenes','r35_scenes','r36_spaceship','r37_scenes','r38_scenes','r38_panel140','r39_scenes','r39_panels','r40_scenes','r40_panels','r41_scenes','r41_poses','r41_panels','r42_scenes','r42_panels','r42_leroof_art','r43_scenes','r43_panels','r44_poses','r44_scenes'];results=[]
for n in names:
 with (O/f'R44_REGRESSION_{n}.log').open('w') as f:p=subprocess.run([sys.executable,str(R/'tools'/f'check_{n}.py')],stdout=f,stderr=subprocess.STDOUT)
 print(n,p.returncode,flush=True);results.append({'probe':n,'exit':p.returncode})
 if p.returncode:break
(O/'R44_REGRESSIONS.json').write_text(json.dumps(results,indent=2)+'\n');sys.exit(int(any(x['exit'] for x in results)))
