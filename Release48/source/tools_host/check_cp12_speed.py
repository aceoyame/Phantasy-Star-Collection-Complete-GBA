#!/usr/bin/env python3
"""Same guest/display-frame workloads for CP11 and CP12, all eight adventures."""
from pathlib import Path
import concurrent.futures,json,os,re,subprocess
R=Path(__file__).resolve().parents[2]
names=['Anne','Rudger','Kinds','Shilka','Amia','Huey','Eusis','Nei']
def run(job):
    index,tag=job
    elf=R/'source/references/cp11_speed/md.elf' if tag=='CP11' else R/'source/additional/build/md.elf'
    p=subprocess.run(['python3',str(R/'source/tools_host/cp10_save/speed.py'),str(R/f'binaries/RUN_THIS_R48_{tag}_TEST.gba'),str(index),'0x81'],env=dict(os.environ,MD_ELF=str(elf)),capture_output=True,text=True,check=True)
    a,b=[float(x) for x in re.findall(r': ([0-9.]+)',p.stdout)]
    return index,tag,{'title_rate':a,'song_81_rate':b}
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as ex:
    results=list(ex.map(run,[(i,t) for i in range(2,10) for t in ['CP11','CP12']]))
rows=[]
for i,n in enumerate(names,2):
    row={'game':n,'index':i}
    for idx,t,data in results:
        if idx==i:row[t]=data
    row['song_speed_gain_percent']=round((row['CP12']['song_81_rate']/row['CP11']['song_81_rate']-1)*100,1)
    rows.append(row)
out=R.parent/'validation_cp12/SPEED_ALL_EIGHT.json';out.parent.mkdir(exist_ok=True)
out.write_text(json.dumps({'units':'guest frames per GBA display frame','display_frames_per_window':600,'song_command':'0x81','rows':rows},indent=2)+'\n')
for r in rows:print(f"{r['game']:7s} {r['CP11']['song_81_rate']:.3f} -> {r['CP12']['song_81_rate']:.3f}  {r['song_speed_gain_percent']:+.1f}%")
