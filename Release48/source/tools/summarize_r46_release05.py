#!/usr/bin/env python3
"""Correlate completed Release 05 evidence; CRC32, no expensive hash sweep."""
import argparse, collections, json, math, re, zipfile, zlib
from pathlib import Path

R = Path(__file__).resolve().parents[2]
O = R / 'build_artifacts'
ap = argparse.ArgumentParser()
ap.add_argument('--baseline-zip', type=Path, required=True)
a = ap.parse_args()
def crc(data): return f'{zlib.crc32(data):08x}'
def read(name): return json.loads((O / name).read_text())
def write(name, value): (O / name).write_text(json.dumps(value, indent=2) + '\n')

changed = []
files = sorted(p for p in (R / 'source/gba').rglob('*') if p.is_file()
               and p.suffix not in ('.o', '.su', '.pyc')
               and '__pycache__' not in p.parts and not p.name.endswith('.o.tmp'))
with zipfile.ZipFile(a.baseline_zip) as z:
    before_names = {n.removeprefix('R46/source/') for n in z.namelist()
                    if n.startswith('R46/source/gba/') and not n.endswith('/')}
    assert {str(p.relative_to(R / 'source')) for p in files} == before_names
    for p in files:
        rel = str(p.relative_to(R / 'source'))
        before, after = z.read('R46/source/' + rel), p.read_bytes()
        if before != after:
            changed.append(dict(file=rel, before_bytes=len(before), after_bytes=len(after),
                                before_crc32=crc(before), after_crc32=crc(after)))
    expected = {'gba/audio_p134r/src/ps4_fm_multisample_data_generated.inc',
                'gba/audio_p134r/src/p134r_song_trim.h'}
    assert {x['file'] for x in changed} == expected
    protected = json.loads(z.read('R46/build_artifacts/R45_PRESERVATION.json'))['unchanged_audio_files']
    assert set(expected) <= set(protected)
    for rel in protected:
        if rel not in expected:
            assert (R / 'source' / rel).read_bytes() == z.read('R46/source/' + rel)
    donor_same = (R / 'donors/PSC_R15_shell_8MiB.gba').read_bytes() == z.read('R46/donors/PSC_R15_shell_8MiB.gba')
    poses_same = (R / 'source/gba/r31_field_poses.bin').read_bytes() == z.read('R46/source/gba/r31_field_poses.bin')
    profiles = json.loads((R / 'AUDIO_BANK_PROFILES.json').read_text())['profiles']
    rollback = {}
    for name, profile in profiles.items():
        for rec in profile['files'].values():
            data = (R / rec['file']).read_bytes()
            assert len(data) == rec['bytes'] and crc(data) == rec['crc32']
        rollback[name] = {'verified_files': len(profile['files']), 'preflight_pass': True}
    projected = 0
    for p in files:
        rel = str(p.relative_to(R / 'source'))
        target = 'source/' + rel
        rec = profiles['reference04']['files'].get(target)
        data = (R / rec['file']).read_bytes() if rec else p.read_bytes()
        assert data == z.read('R46/source/' + rel)
        projected += 1
assert donor_same and poses_same and len(files) == 259
preservation = dict(production_files=len(files), changed=changed,
                    other_production_files_byte_identical=len(files)-len(changed),
                    historical_protected_audio_entries=len(protected), intentional_changed_entries=2,
                    unchanged_entries=len(protected)-2, donor_byte_identical=donor_same,
                    pose_bank_byte_identical=poses_same, pose_bank_bytes=203168,
                    PD_gameplay_transport_SFX_unchanged=True,
                    reference04_rollback_projected_identical_files=projected)
write('R46_RELEASE05_PRODUCTION_GUARD.json', preservation)

# The diagnostic logs have no header. They record original absolute ROM PCs.
columns = ['song', 'frame', 'lane', 'pc', 'kind', 'event', 'duration', 'note',
           'keyed', 'held', 'instrument', 'wait']
rom_source = (R / 'source/references/roms/ps4built.bin').read_bytes()
traces = []
for stem, frames, songs in [('ALL_NOTE_TRACE',1800,52), ('OPENING_NOTE_TRACE',13000,1),
                           ('ALL_LONG_NOTE_TRACE',13000,52)]:
    old = O / ('R46_RELEASE04_' + stem + '/notes.tsv')
    new = O / ('R46_RELEASE05_' + stem + '/notes.tsv')
    assert old.read_bytes() == new.read_bytes(), stem
    count = 0; direct = 0; seen = set(); unique = collections.defaultdict(set)
    for line in new.read_text().splitlines():
        row = dict(zip(columns, map(int, line.split('\t')))); count += 1
        assert 0 <= row['frame'] < frames; seen.add(row['song'])
        if 0x80 <= row['event'] < 0xe0:
            assert row['event'] in rom_source[max(0,row['pc']-2):row['pc']], row
            direct += 1
        if row['song'] == 0xac and row['kind'] in (1,2) and row['keyed'] and row['note'] >= 0:
            unique[row['lane']].add(row['note'])
    assert len(seen) == songs
    traces.append(dict(songs=songs, source_frames_each=frames, logged_events=count,
                       explicit_note_rest_bytes_match_original_ROM=direct,
                       reference04_reference05_identical=True,
                       opening_pitched_lane_unique_notes={str(k):len(v) for k,v in sorted(unique.items())},
                       evidence=str(new.relative_to(R))))
loops = json.loads((R / 'source/tools/ps4_music_loops_phase14.json').read_text())['tracks']
seconds = 13000 * 224 / 13379
outside = [x['music_id_hex'] for x in loops
           if x.get('source_loop_end_seconds_from_music_start',0) > seconds]
note_report = dict(traces=traces, longest_window_seconds=seconds,
                  original_loop_analysis_outside_window=outside,
                  opening_song='End of the Millennium ($AC)', original_FM4_instrument=7,
                  diagnosis='Authored repeated accompaniment, not a reproduced frozen note cursor. '
                            'Release 04 per-patch calibration overemphasized it versus the lead.',
                  original_repetition_preserved=True, note_sequencer_changed=False,
                  scope='Production host note events compared against preserved Release 04; explicit bytes '
                        'checked at their original ROM PCs. Not an independent full source-control-flow '
                        'oracle, all repeated notes forbidden, perceptual certification or full-game test.')
write('R46_RELEASE05_NOTE_TRACE_AUDIT.json', note_report)

# Validate every offset, length, format flag and original lookup key.
import sys
sys.path.insert(0, str(R / 'source/tools'))
from generate_dense_fm_multisamples import parse
_, _, blob, zones = parse(R / 'source/gba/audio_p134r/src/ps4_fm_multisample_data_generated.inc')
_, _, _, old_zones = parse(R / 'rollback/R46_RELEASE03_AUDIO/source/gba/audio_p134r/src/ps4_fm_multisample_data_generated.inc')
keys = [(z['bank'],z['inst'],z['note']) for z in zones]
assert keys == sorted(keys) == [(z['bank'],z['inst'],z['note']) for z in old_zones]
assert len(set(keys)) == 3977 and all(-128 <= x <= 127 for x in blob)
for zone in zones:
    flags = {o >> 30 for o in zone['offs']}; assert len(flags) == 1
    assert len(zone['offs']) == len(zone['gains']) == 6
    for off,gain in zip(zone['offs'],zone['gains']):
        points = 256 if off & 0x40000000 else 512
        assert (off & 0x3fffffff) + points <= len(blob) and 0 <= gain <= 255
waves = read('R46_RELEASE05_WAVES/REFERENCE_WAVE_REPORT.json')
assert len(blob) == waves['wave_blob_bytes']
waves['format_checks'] = dict(sorted_lookup_keys=3977, stage_spans=23862, bounds_flags_pass=True)

host = read('R46_RELEASE05_PSG_HOLD.json')
assert host['return_code'] == 0 and len(host['songs']) == 52
assert host['summary']['missing_FM_zones'] == host['summary']['mismatches'] == 0
native = read('R46_RELEASE05_MGBA_PSG_HOLD.json')
assert native['envelope_checks'] == 1280 and len(native['songs']) == 7
assert all(x['host_native_match'] for x in native['songs'])
timing = read('R46_RELEASE05_MGBA_MUSIC_DEADLINES.json')
assert len(timing['songs']) == 52
assert all(x['audio_delta']['underruns'] == x['audio_delta']['late_commits'] == 0 for x in timing['songs'])
sfx = read('R46_RELEASE05_MGBA_SFX_OWNERSHIP.json')
assert sfx['all_match'] and sfx['cases'] == 402
assert read('R46_RELEASE05_SFX_OWNERSHIP.json')['cases'] == 402
title = read('R46_RELEASE05_TITLE_AUDIO.json')
baseline_title = read('R46_RELEASE04_REPEAT_BASELINE_TITLE_AUDIO.json')
last = title['rows'][-1]
assert last['underruns'] == 0 and last['music_sequence'] == last['productions']
rom = (R / 'binaries/RUN_THIS_RELEASE48_PSC.gba').read_bytes()
module = (R / 'source/build-psc-module/ps4_psc_module.raw.gba').read_bytes()
assert title['ROM_crc32'] == crc(rom)
state = read('R46_RELEASE05_MGBA_DEBUG.rom_crc.json')
assert state['rom_crc32'] == crc(rom)
maptext = (R / 'source/build-psc-module/ps4_psc_module.map').read_text()
def symbol(name):
    m = re.search(r'^\s*([0-9a-fA-F]+)\s+[^\n]*\b'+name+r'\s*=',maptext,re.M)
    assert m, name; return int(m[1],16)
iw,ew = symbol('__iwram_alloc_end'),symbol('__bss_end')
build = dict(compiler='Clang/LLD 18.1.3', module_bytes=len(module),
             slot_headroom_bytes=0x1800000-len(module), slot_headroom_KiB=(0x1800000-len(module))/1024,
             iwram_alloc_end=hex(iw), iwram_margin_bytes=0x3006d00-iw,
             ewram_bss_end=hex(ew), ewram_margin_bytes=0x202a000-ew,
             new_mutable_globals=False, music_lane_bytes=148, render_FM_state_bytes=36)
report = dict(release='R46 release 05', date='2026-10-03', baseline='R46 release 04',
              ROM=dict(path='binaries/RUN_THIS_RELEASE48_PSC.gba',rom_bytes=len(rom),rom_crc32=crc(rom)),
              build=build, preservation=preservation, rollback_profiles=rollback, waves=waves,
              note_trace_audit=note_report,
              headroom=dict(songs=52,source_frames_each=3600,PCM_samples=41932800,
                            measurement='production pre-limiter with song trims disabled',
                            Q8_min=76,Q8_median=108,Q8_max=256,table_bytes=104,
                            max_final_1800_frame_limiter_top_fraction=max(x['limiter_top_fraction'] for x in host['songs']),
                            evidence='build_artifacts/R46_RELEASE05_PRELIMIT_HEADROOM.json'),
              host=dict(songs=52,source_frames=93600,PCM_samples=20966400,
                        sanitizers=host['sanitizers'],**host['summary'],
                        evidence='build_artifacts/R46_RELEASE05_PSG_HOLD.json'),
              native=dict(music_songs=7,music_source_frames=12600,music_PCM_samples=native['music_samples'],
                          PSG_envelope_checks=1280,host_PCM_CRCs_match=True,entire_MD_RAM_guarded_bytes=65536,
                          evidence='build_artifacts/R46_RELEASE05_MGBA_PSG_HOLD.json',scope=native['scope']),
              SFX=dict(host_cases=402,native_cases=402,exact_PCM_samples_each=sfx['exact_PCM_samples'],
                       evidence=['build_artifacts/R46_RELEASE05_SFX_OWNERSHIP.json',
                                 'build_artifacts/R46_RELEASE05_MGBA_SFX_OWNERSHIP.json'],
                       live_overlap_not_rerun=True),
              music_deadlines=dict(songs=52,display_frames=15600,underruns=0,late_commits=0,
                                   max_produce_CPU_cycles=max(x['max_produce_CPU_cycles'] for x in timing['songs']),
                                   evidence='build_artifacts/R46_RELEASE05_MGBA_MUSIC_DEADLINES.json',scope=timing['scope']),
              natural_title=dict(display_frames=title['frames'],final=last,
                                 baseline_late_commits=baseline_title['rows'][-1]['late_commits'],
                                 baseline_underruns=baseline_title['rows'][-1]['underruns'],
                                 PCM_RMS=title['PCM_RMS'],stuck_note_reproduced=False,
                                 late_commits_fixed=False,evidence='build_artifacts/R46_RELEASE05_TITLE_AUDIO.json',
                                 scope=title['scope']),
              historical_field_recovery='Release 04 34-case field sweep (2 transition late commits), '
                                        'six live SFX cases and starvation recovery are retained, not rerun.',
              finishable_certified=False,subjective_audio_verified=False,hardware_verified=False,
              PD_tests_rerun=False,
              handoff='RELEASE_NOTES_R46_RELEASE_05.md')
write('R46_RELEASE05_VALIDATION.json', report)
print(json.dumps(dict(ROM=report['ROM'],build=build,changed=changed,
                      note_traces=traces,max_produce_cycles=report['music_deadlines']['max_produce_CPU_cycles'],
                      natural_title=report['natural_title']),indent=2))
