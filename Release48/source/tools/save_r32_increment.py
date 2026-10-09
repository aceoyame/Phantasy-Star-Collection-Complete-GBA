#!/usr/bin/env python3
"""Save cumulative R32 changes over durable checkpoint 01 without copying unchanged banks."""
from pathlib import Path
import argparse,json,zipfile
R=Path(__file__).resolve().parents[1];tree=R.parent
ap=argparse.ArgumentParser();ap.add_argument('number');ap.add_argument('--out-dir',type=Path,required=True);a=ap.parse_args();a.out_dir.mkdir(parents=True,exist_ok=True)
files=['R32_CHECKPOINT_STATUS.md','START_HERE.md','HANDOFF_MANIFEST.json','source/gba/title_runtime_v06.c','source/gba/r32_travel_state.h','source/gba/r32_travel_menu.h','source/gba/r32_map_updates.h','source/gba/r32_world_bg.h','source/gba/r32_vehicle_move.h','source/gba/r31_field_actors.h','source/gba/r31_field_poses.h','source/gba/r31_field_poses.bin','source/gba/r31_field_poses_data.s','source/gba/psc_runtime_plan.c','source/generated/psc_runtime_plan_report.json','source/generated/r31_field_poses_report.json','source/build_r32.py','source/tools/check_r32_travel_maps.py','source/tools/check_r31_story.py','source/tools/check_r32_vehicles.py','source/tools/save_r32_checkpoint.py','source/tools/save_r32_increment.py','source/tools/audit_r32_subsystems.py','source/tools/generate_r31_field_poses.py','source/docs/R32_NOTES.md']
files += ['build_artifacts/'+p.name for p in sorted((tree/'build_artifacts').glob('R32*')) if p.is_file()]
files += ['build_artifacts/R31_TO_R32_SOURCE.patch','build_artifacts/r31_story_probe.c','build_artifacts/r32_travel_maps_probe.c','build_artifacts/r32_vehicle_probe.c']
recovery=f'''# Recover R32 checkpoint {a.number}\n\nStart with saved PSIV_R32_CHECKPOINT_01_2026-09-30.zip (libfile_1470d4c0365c8191b65f8d032d37268f). Extract this ZIP over its R32 directory. Checkpoint 02 is not required: this package is cumulative against 01. It replaces all listed runtime/build/probe files. Checkpoint 01's playable binary is R31.\n\nWith Python 3 and LLVM 18 clang/clang++, ld.lld and llvm-objcopy on PATH, run source/build_r32.py from source/. Build output: binaries/RUN_THIS_R32_PSC_TRAVEL_AND_MAPS.gba. Read R32_CHECKPOINT_STATUS.md for validation, limitations and remaining work. The original ROM, donor and unchanged immutable/audio banks come from checkpoint 01. The account file storage limit prevented saving another full source archive; this package contains the complete change set and build/validation evidence.\n'''
out=a.out_dir/f'PSIV_R32_CHECKPOINT_{a.number}_INCREMENT_2026-09-30.zip'
with zipfile.ZipFile(out,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
 z.writestr('RECOVERY.md',recovery)
 for rel in dict.fromkeys(files):z.write(tree/rel,'R32/'+rel)
(a.out_dir/f'checkpoint{a.number}_increment_upload.json').write_text(json.dumps({'uploads':[{'local_path':str(out),'purpose':'create_library_file','library_artifact_type':'other'}]}))
print(out,out.stat().st_size)
