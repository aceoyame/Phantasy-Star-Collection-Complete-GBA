#!/usr/bin/env python3
"""One-command private Stage21C -> R237 -> Stage26 -> R238 build chain."""
import argparse,subprocess,sys,tempfile,zipfile
from pathlib import Path
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[2]
STAGE21=HERE/"build_stage21c_r237_relocated.py"
STAGE26=HERE/"stage26_restore_overwritten_data.py"
R238=ROOT/"tools/r238/integrate_r238.py"
def main():
 p=argparse.ArgumentParser(description=__doc__)
 for k in ("r235","stage14","stage21c","r237_zip","r238_zip","out"):
  p.add_argument("--"+k.replace("_","-"),type=Path,required=True)
 p.add_argument("--report",type=Path)
 a=p.parse_args()
 if a.out.exists() or (a.report and a.report.exists()):raise ValueError("output/report exists")
 with tempfile.TemporaryDirectory(prefix="psc_r238_stage26_") as td:
  td=Path(td);base=td/"r237.gba";mid=td/"stage21c.gba";stage=td/"stage26.gba"
  with zipfile.ZipFile(a.r237_zip) as z:base.write_bytes(z.read("r237/PSC_R237_CANDIDATE.gba"))
  subprocess.run([sys.executable,str(STAGE21),"--r235",str(a.r235),
    "--stage14",str(a.stage14),"--stage21c",str(a.stage21c),
    "--r237-zip",str(a.r237_zip),"--out",str(mid),
    "--report",str(td/"stage21c.json")],check=True)
  subprocess.run([sys.executable,str(STAGE26),str(base),str(mid),str(stage)],check=True)
  cmd=[sys.executable,str(R238),"--r237",str(base),"--checkpoint",str(a.r238_zip),
    "--stage26",str(stage),"--out",str(a.out)]
  if a.report:cmd.extend(["--report",str(a.report)])
  subprocess.run(cmd,check=True)
if __name__=="__main__":main()
