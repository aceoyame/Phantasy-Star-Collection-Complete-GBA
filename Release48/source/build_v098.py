#!/usr/bin/env python3
"""PSIV static-recompiler v0.9.8: v18 AOT audio + source-backed gameplay AOT PCM Direct Sound build."""
from __future__ import annotations
# CP22: one PSC-style source-sequenced method for the whole soundtrack (PS4_HYBRID_PCM_MUSIC=1 restores PCM masters)
import os as _os_cp22
UNIVERSAL_SEQUENCED_MUSIC = _os_cp22.environ.get('PS4_HYBRID_PCM_MUSIC','0')!='1'

import argparse, hashlib, shutil, subprocess, sys
from pathlib import Path
ROOT=Path(__file__).resolve().parent; GBA=ROOT/'gba'; BUILD=ROOT/'build-v098'
EXPECTED='511f35cc11f88316f8b8940e28ab298bd75a4da193672a80172884d6eb913b6a'
def run(cmd):
    print('+',' '.join(map(str,cmd))); subprocess.run([str(x) for x in cmd],cwd=ROOT,check=True)
def which(n):
    p=shutil.which(n); return Path(p) if p else None
def main():
    global BUILD
    ap=argparse.ArgumentParser(); ap.add_argument('--rom',required=True,type=Path); ap.add_argument('--logo-from',required=True,type=Path)
    # CP22 unified forks: the PSC fork links THIS intact game (same code + same full asset banks) at
    # 0x08800000 inside a 24 MiB slot; only packaging differs from the standalone ROM.
    ap.add_argument('--psc-module',action='store_true',help='link at 0x08800000 (24 MiB slot) with PSIV saves at SRAM+0x2000')
    ap.add_argument('--psc',type=Path,help='original 8 MiB Phantasy Star Collection ROM: also pack the four-game image')
    ap.add_argument('--psc-out',type=Path,default=Path('RUN_THIS_PSC4_CP22.gba'))
    a=ap.parse_args()
    if a.psc_module: BUILD=ROOT/'build-psc-module'
    rom=a.rom.resolve(); logo=a.logo_from.resolve(); data=rom.read_bytes()
    if len(data)!=0x300000: raise SystemExit(f'bad PSIV source size {len(data):#x}')
    if _os_cp22.environ.get('PS4_NO_HASH')!='1':
        d=hashlib.sha256(data).hexdigest()
        if d!=EXPECTED: raise SystemExit(f'bad PSIV source {d}')
    BUILD.mkdir(exist_ok=True)
    if _os_cp22.environ.get('PS4_SKIP_ASSET_REGEN')!='1':
        run([sys.executable,'tools/generate_simple_event_descriptors.py','--rom',rom])
        run([sys.executable,'tools/generate_cutscene_scripts.py','--rom',rom])
        run([sys.executable,'tools/generate_enemy_animation_frames.py','--rom',rom])
        run([sys.executable,'tools/generate_enemy_attack_frames.py','--rom',rom])
        run([sys.executable,'tools/generate_party_battle_frames.py','--rom',rom])
        run([sys.executable,'tools/generate_party_attack_objects.py','--rom',rom])
        run([sys.executable,'tools/generate_technique_objects.py','--rom',rom])
        run([sys.executable,'tools/generate_source_ability_effects.py','--rom',rom])
        run([sys.executable,'tools/generate_source_sfx_chip_headers.py','--rom',rom])
        run([sys.executable,'tools/generate_source_enemy_effects.py','--rom',rom])
        run([sys.executable,'tools/generate_source_combo_effects.py','--rom',rom])
        run([sys.executable,'tools/generate_r46_weapon_effects.py'])
        run([sys.executable,'tools/import_r46_recorded_clips.py'])
        run([sys.executable,'tools/generate_ps4_window_assets.py','--rom',rom])
        run([sys.executable,'tools/generate_precompiled_visuals.py','--rom',rom])
        run([sys.executable,'tools/generate_title_intro_rows.py','--rom',rom])
        run([sys.executable,'tools/generate_packed_music.py'])
        run([sys.executable,'tools/generate_psc_field_predecode.py','--rom',rom])
        run([sys.executable,'tools/generate_psc_map_descriptors.py','--rom',rom])
        run([sys.executable,'tools/generate_psc_runtime_plans.py','--rom',rom])
        run([sys.executable,'tools/generate_psc_field_fastassets.py','--rom',rom])
    run([sys.executable,'tools/make_reversed_rom.py',rom,'-o','gba/ps4_reversed.bin'])
    run([sys.executable,'tools/extract_gba_logo.py',logo,'-o','gba/nintendo_logo.bin'])
    csrc=['gba/title_runtime_v06.c','gba/md_bus_runtime.c','gba/md_io_runtime.c','gba/freestanding_support.c','gba/ps4_audio_stream.c','gba/psc_field_predecode.c','gba/psc_map_desc.c','gba/psc_runtime_plan.c','gba/psc_panel_bank.c','gba/psc_field_fastassets.c','gba/source_ability_effects.c','gba/source_combo_effects.c','gba/source_enemy_effects.c','gba/r46_weapon_effects.c']
    ssrc=['gba/startup_v098.s','gba/ps4_audio_irq.s','gba/ps4_rom.s','gba/precomp_assets.s','gba/title_intro_rows_data.s','gba/pcm_tracks.s','gba/sfx_bank.s','gba/psc_field_predecode_data.s','gba/psc_panel_bank_data.s','gba/psc_field_fastassets_data.s','gba/source_ability_effects_data.s','gba/source_combo_effects_data.s','gba/source_enemy_effects_data.s','gba/r31_field_poses_data.s','gba/r46_weapon_effects_data.s','gba/r46_zio_story_data.s']
    csrc.append('gba/r46_recorded_clips.c')
    ssrc.append('gba/r46_recorded_clips_data.s')
    cpp=['gba/nemesis_recompiled_runtime.cpp','gba/ps4_audio_fallback.cpp','gba/audio_p134r/src/chip_audio.cpp','gba/audio_p134r/src/ps4_chip_audio_data.cpp','gba/audio_p134r/src/ps4_fm_multisample_data.cpp']
    if UNIVERSAL_SEQUENCED_MUSIC:
        # CP22: every music ID is P134R-sequenced; the 17.5 MB PCM track bank leaves the link.
        ssrc=[x for x in ssrc if x!='gba/pcm_tracks.s']
    clang=which('clang'); lld=which('ld.lld'); objcopy=which('llvm-objcopy')
    gcc=which('arm-none-eabi-gcc'); gxx=which('arm-none-eabi-g++'); gobjcopy=which('arm-none-eabi-objcopy')
    use_clang=bool(clang and lld and objcopy)
    if not use_clang and not (gcc and gxx and gobjcopy):
        raise SystemExit('need either clang + ld.lld + llvm-objcopy or devkitARM arm-none-eabi-gcc/g++/objcopy')
    cc=clang if use_clang else gcc; cxx=clang if use_clang else gxx; objcopy=objcopy if use_clang else gobjcopy
    common=(['--target=arm-none-eabi'] if use_clang else [])+['-mcpu=arm7tdmi','-marm','-ffreestanding','-fno-builtin','-fno-stack-protector','-fstack-usage','-Os','-ffunction-sections','-fdata-sections','-Igba','-Igba/audio_p134r/include',
        '-DMD2GBA_PS4_MAXMOD_MODE_A=1','-DMD2GBA_PS4_MAXMOD_CHANNEL_MAJOR=1','-DMD2GBA_PS4_V19_FULL_MIX=1','-DMD2GBA_PS4_M4A_NATIVE_ONLY=1']
    if not UNIVERSAL_SEQUENCED_MUSIC: common=common+['-DPS4_MUSIC_UNIVERSAL_SEQUENCED=0']
    # R16b A/B: PS4_NPCTYPE1_STANDARD_NPC=0 restores R15's 32x32 NPCType1 composition.
    if _os_cp22.environ.get('PS4_NPCTYPE1_STANDARD_NPC')=='0': common=common+['-DPS4_NPCTYPE1_STANDARD_NPC=0']
    # R16c A/B: PS4_BATTLE_SOURCE_DAMAGE_WINDOW=0 restores the R15 red 6x6 damage digits.
    if _os_cp22.environ.get('PS4_BATTLE_SOURCE_DAMAGE_WINDOW')=='0': common=common+['-DPS4_BATTLE_SOURCE_DAMAGE_WINDOW=0']
    # R16 A/B: PS4_DIALOGUE_F7_SEGMENTS=0 restores R15's $F7-swallowing dialogue runner.
    if _os_cp22.environ.get('PS4_DIALOGUE_F7_SEGMENTS')=='0': common=common+['-DPS4_DIALOGUE_F7_SEGMENTS=0']
    # R15 A/B: PS4_BATTLE_ENEMY_SPAN=0 restores the R14 per-pixel enemy compositor.
    if _os_cp22.environ.get('PS4_BATTLE_ENEMY_SPAN')=='0': common=common+['-DPS4_BATTLE_ENEMY_SPAN=0']
    # R14B A/B: PS4_AUDIO_IRQ_TRANSPORT=0 rebuilds the exact R13 polling audio transport.
    if _os_cp22.environ.get('PS4_AUDIO_IRQ_TRANSPORT')=='0': common=common+['-DPS4_AUDIO_IRQ_TRANSPORT=0']
    objs=[]
    for src in csrc:
        out=BUILD/(Path(src).stem+'.o'); extra=['-DPS4_SRAM_PHYS_BASE=0x2000'] if a.psc_module and src.endswith('md_bus_runtime.c') else []
        run([cc,*common,*extra,'-std=c11','-c',src,'-o',out]); objs.append(out)
    for src in ssrc:
        out=BUILD/(Path(src).stem+'.o'); run([cc,*common,'-x','assembler-with-cpp','-c',src,'-o',out]); objs.append(out)
    # CP22 battle-speed switches for P134R (default off = proven ARM/ROM layout; A/B on hardware):
    #   PS4_P134R_MIX_IWRAM=1 : mix_psc_mono in IWRAM (~3x cheaper fetches; IWRAM ASSERT guards the stack)
    #   PS4_P134R_THUMB=1     : P134R + wrapper compiled as Thumb (16-bit fetches from the 16-bit ROM bus)
    p134r_extra=[]
    # R14C: default ON (P134R per-sample kernels run from IWRAM); PS4_P134R_MIX_IWRAM=0 restores R13 ROM placement.
    if _os_cp22.environ.get('PS4_P134R_MIX_IWRAM','1')=='1': p134r_extra+=['-DPS4_FALLBACK_MIX_IWRAM=1']
    # R16c A/B: PS4_P134R_FM_SMOOTH=0 restores stage-jump timbre and per-frame constant FM level.
    if _os_cp22.environ.get('PS4_P134R_FM_SMOOTH')=='0': p134r_extra+=['-DPS4_P134R_FM_SMOOTH=0']
    # R16 A/B: PS4_P134R_FM_PITCH_MOD=0 renders FM at the bare note (R15).
    if _os_cp22.environ.get('PS4_P134R_FM_PITCH_MOD')=='0': p134r_extra+=['-DPS4_P134R_FM_PITCH_MOD=0']
    # R15 A/B: PS4_P134R_SONG_TRIM=0 restores R14 (untrimmed, limiter-saturated) music levels.
    if _os_cp22.environ.get('PS4_P134R_SONG_TRIM')=='0': p134r_extra+=['-DPS4_P134R_SONG_TRIM=0']
    if _os_cp22.environ.get('PS4_P134R_THUMB')=='1': p134r_extra+=['-mthumb']+([] if use_clang else ['-mthumb-interwork'])
    for src in cpp:
        out=BUILD/(Path(src).stem+'.o'); extra=p134r_extra if ('audio_p134r' in src or 'ps4_audio_fallback' in src) else []
        run([cxx,*common,*[x for x in extra if not (x=='-mthumb' and '-marm' not in common)],'-std=c++17','-fno-exceptions','-fno-rtti','-fno-unwind-tables','-c',src,'-o',out]); objs.append(out)
    tag='ps4_psc_module' if a.psc_module else 'ps4_v098'; linker='gba/link_psc.ld' if a.psc_module else 'gba/link.ld'
    elf=BUILD/(tag+'.elf'); mp=BUILD/(tag+'.map')
    if use_clang: run([lld,'-T',linker,'--gc-sections',f'-Map={mp}','-o',elf,*objs])
    else: run([gxx,'-nostdlib','-mcpu=arm7tdmi','-marm',f'-Wl,-T,{linker}','-Wl,--gc-sections',f'-Wl,-Map={mp}','-o',elf,*objs,'-lgcc'])
    raw=BUILD/(tag+'.raw.gba'); run([objcopy,'-O','binary',elf,raw])
    if a.psc_module:
        n=raw.stat().st_size
        if n>0x1800000: raise SystemExit(f'PSC PSIV module overflow: {n} > 25165824')
        print('PASS',raw,n,'headroom='+str(0x1800000-n))
        if a.psc:
            # Full banks stay inside the module, so no PSC-padding overlays are needed.
            run([sys.executable,'tools/build_psc_four_game.py','--psc',a.psc.resolve(),'--psiv',raw,'--out',a.psc_out.resolve(),'--no-overlays'])
            # One version: the standalone ROM is extracted from the PSC4 image (module byte-identical).
            run([sys.executable,'tools/extract_psiv_standalone.py',a.psc_out.resolve(),'-o',a.psc_out.resolve().with_name('RUN_THIS_PSIV_STANDALONE_EXTRACTED.gba')])
        return
    final=GBA/'ps4_static_recompiler_v098.gba'; run([sys.executable,'tools/finalize_gba_v098.py',raw,final])
    out=final.read_bytes(); h=hashlib.sha256(out).hexdigest()
    if out[0xA0:0xAC]!=b'PS4RC098    ' or out[0xAC:0xB0]!=b'P498': raise SystemExit('bad header id')
    if ((-sum(out[0xA0:0xBD])-0x19)&255)!=out[0xBD]: raise SystemExit('checksum fail')
    print('PASS',final,len(out),'sha256='+h)
if __name__=='__main__': main()
