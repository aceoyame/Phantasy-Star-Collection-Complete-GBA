#!/usr/bin/env python3
"""Save a complete R46 source/ROM checkpoint with protected-baseline byte checks."""
from pathlib import Path
import argparse,json,re,zipfile
R=Path(__file__).resolve().parents[2]
ap=argparse.ArgumentParser();ap.add_argument('--label',required=True);ap.add_argument('--rom',type=Path,required=True);ap.add_argument('--baseline-zip',type=Path,required=True);ap.add_argument('--out',type=Path,required=True);ap.add_argument('--allow-changed-audio-file',action='append',default=[],help='Intentional exception within the historical protected-audio source list; all other entries remain byte-checked.');ap.add_argument('--allow-reclaimed-psc-padding',action='store_true',help='Permit only zeroing the two verified retail padding ranges; all stock payload/launcher/save bytes stay guarded.');a=ap.parse_args()
from psc_rom_space import check_reclaimed_padding,available_padding
source=R/'source';rom=a.rom.resolve().read_bytes();module=(source/'build-psc-module/ps4_psc_module.raw.gba').read_bytes();donor=(R/'donors/PSC_R15_shell_8MiB.gba').read_bytes()
assert len(rom)==0x2000000 and len(donor)==0x800000 and len(module)<=0x1800000
assert rom[:0x800000]==donor and rom[0x800000:0x800000+len(module)]==module
assert rom[0x800000+len(module):]==b'\xff'*(0x1800000-len(module))
with zipfile.ZipFile(a.baseline_zip) as z:
    base_root='R46' if 'R46/source/gba/title_runtime_v06.c' in z.namelist() else 'R45'
    evidence=json.loads(z.read(base_root+'/build_artifacts/R45_PRESERVATION.json'))
    protected=list(evidence['unchanged_audio_files']);allowed=set(a.allow_changed_audio_file)
    unknown=allowed-set(protected);assert not unknown,('unknown audio preservation exception',sorted(unknown))
    for f in protected:
        current=(source/f).read_bytes();baseline=z.read(base_root+'/source/'+f)
        if f in allowed:assert current!=baseline,('declared audio exception is unchanged',f)
        else:assert current==baseline,f
    prior_donor=z.read(base_root+'/donors/PSC_R15_shell_8MiB.gba');donor_same=donor==prior_donor
    if a.allow_reclaimed_psc_padding:check_reclaimed_padding(prior_donor,donor)
    else:assert donor_same
    poses=(source/'gba/r31_field_poses.bin').read_bytes();assert poses==z.read(base_root+'/source/gba/r31_field_poses.bin')
maptext=(source/'build-psc-module/ps4_psc_module.map').read_text()
def symbol(name):
    m=re.search(r'^\s*([0-9a-fA-F]+)\s+[^\n]*\b'+name+r'\s*=',maptext,re.M);assert m,name;return int(m[1],16)
iw,ew=symbol('__iwram_alloc_end'),symbol('__bss_end');assert iw<=0x3006D00 and ew<=0x202A000
manifest={'release':'R46','checkpoint':a.label,'binary':str(a.rom.resolve().relative_to(R)),'compiler':'Clang/LLD 18.1.3','complete_source':True,'restore':'Extract and use R46/. Earlier increments are not required.','audio_files_byte_identical':len(protected)-len(allowed),'audio_files_intentionally_changed':sorted(allowed),'audio_preservation_baseline_entries':len(protected),'donor_byte_identical':donor_same,'donor_changes_only_reclaimed_verified_padding':not donor_same and a.allow_reclaimed_psc_padding,'PSC_reserved_padding_bytes':available_padding(donor),'pose_bank_bytes':len(poses),'limits':{'module_bytes':len(module),'slot_headroom_bytes':0x1800000-len(module),'rom_bytes':len(rom),'iwram_end':hex(iw),'iwram_margin':0x3006D00-iw,'ewram_bss_end':hex(ew),'ewram_margin':0x202A000-ew},'finishable_certified':False,'hardware_playthrough_verified':False}
historical_binaries=[str(p.relative_to(R)) for p in sorted((R/'binaries').glob('*.gba')) if p.resolve()!=a.rom.resolve()]
manifest['historical_runtime_binaries_omitted']=historical_binaries
if a.label.startswith('release_'):
    tag=a.label.removeprefix('release_')
    notes=f'RELEASE_NOTES_R46_RELEASE_{tag}.md';validation=f'build_artifacts/R46_RELEASE{tag}_VALIDATION.json'
    assert (R/notes).is_file() and (R/validation).is_file()
    manifest.update(distribution_type='release',release_version=a.label,known_issues=notes,validation=validation)
    (R/'RELEASE_MANIFEST.json').write_text(json.dumps(manifest,indent=2)+'\n')
for name in ('HANDOFF_MANIFEST.json','CHECKPOINT_MANIFEST.json','build_artifacts/R46_PRESERVATION.json'):
    (R/name).write_text(json.dumps(manifest,indent=2)+'\n')
out=a.out.resolve();out.parent.mkdir(parents=True,exist_ok=True);tmp=out.with_suffix(out.suffix+'.tmp')
with zipfile.ZipFile(tmp,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=6,allowZip64=True) as z:
    for p in sorted(R.rglob('*')):
        if not p.is_file() or '__pycache__' in p.parts or p.suffix in ('.pyc','.o','.su','.so') or p.name.endswith('.o.tmp'):continue
        relative=p.relative_to(R)
        # The complete source/donor/references and current linked module are
        # retained. Package the current runnable ROM under binaries/; older
        # runtime copies remain in earlier complete checkpoints.
        if str(relative) in historical_binaries:continue
        if str(relative).startswith('build_artifacts/') and not p.suffix:continue
        z.write(p,'R46/'+str(relative))
with zipfile.ZipFile(tmp) as z:
    assert z.testzip() is None
    assert z.read('R46/'+manifest['binary'])==rom
    assert json.loads(z.read('R46/HANDOFF_MANIFEST.json'))['checkpoint']==a.label
tmp.replace(out)
print(json.dumps({'path':str(out),'bytes':out.stat().st_size,'manifest':manifest},indent=2))
