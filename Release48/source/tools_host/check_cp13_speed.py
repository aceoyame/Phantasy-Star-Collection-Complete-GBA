#!/usr/bin/env python3
"""Same 600-display-frame windows as CP12, all eight games."""
from pathlib import Path
import concurrent.futures,json,re,subprocess
R=Path(__file__).resolve().parents[2]
names=['Anne','Rudger','Kinds','Shilka','Amia','Huey','Eusis','Nei']
before=json.loads((R.parent/'validation_cp12/SPEED_ALL_EIGHT.json').read_text())['rows']
def run(index):
 p=subprocess.run(['python3',str(R/'source/tools_host/cp10_save/speed.py'),str(R/'binaries/RUN_THIS_R48_CP13_TEST.gba'),str(index),'0x81'],capture_output=True,text=True,check=True)
 a,b=[float(x) for x in re.findall(r': ([0-9.]+)',p.stdout)];base=before[index-2]
 return {'game':names[index-2],'index':index,'CP11':base['CP11'],'CP12':base['CP12'],'CP13':{'title_rate':a,'song_81_rate':b},'gain_vs_CP12_percent':round((b/base['CP12']['song_81_rate']-1)*100,1)}
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as ex:rows=list(ex.map(run,range(2,10)))
(R.parent/'validation_cp13/SPEED_ALL_EIGHT.json').write_text(json.dumps({'units':'guest frames per GBA display frame','display_frames_per_window':600,'song_command':'0x81','rows':rows},indent=2)+'\n')
for r in rows:print(f"{r['game']:7s} CP12 {r['CP12']['song_81_rate']:.3f} -> CP13 {r['CP13']['song_81_rate']:.3f}  {r['gain_vs_CP12_percent']:+.1f}%",flush=True)
