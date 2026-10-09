#!/usr/bin/env python3
"""Native CAMP cursor A/B: identical full pages, input routing and redraw cost.

Supply an extracted Release 06 ROM/map/matching debug state with --baseline-dir.
No game progress is mutated by cursor tests. Frame waits are excluded from the
redraw CPU measurement; this is not a first-open or physical-player benchmark.
"""
from pathlib import Path
import argparse,sys,json,statistics,zlib
p=argparse.ArgumentParser(add_help=False);p.add_argument('--baseline-dir',type=Path,required=True)
p.add_argument('--baseline-release',choices=('release06','release07','release08'),default='release06')
opt,rest=p.parse_known_args();sys.argv=[sys.argv[0]]+rest
h=Path(__file__).with_name('check_r46_special_recovery_mgba.py')
exec(compile(h.read_text().split('\ndef special_fixture(')[0].replace("default='CP23'","default='RELEASE07'"),str(h),'exec'),globals())
current_rom,current_state,current_symbols=rom,state,symbols.copy()
baseline=opt.baseline_dir;frames={};cases=[];which_current=a.label.lower()
baseline_crc={'release06':'cb75252c','release07':'979c0c64','release08':'e6c74f61'}[opt.baseline_release]
def parse_map(path):
    out={}
    for line in path.read_text().splitlines():
        q=line.split()
        if len(q)>=5:
            try:out[q[-1]]=int(q[0],16)
            except ValueError:pass
    return out

def edge(key):
    # Release the previous direction while remaining in the input wait.
    ret=g.reg(14);L.mg_break(0xffffffff);g.run(2,0)
    L.mg_break(ret);g.run(20,key);assert L.mg_hit(),(key,'no input edge')
    assert g.reg(0)&(2 if key=='DOWN' else 1 if key=='UP' else 0x60 if key=='A' else 0x90),(key,g.reg(0))
    L.mg_break(0xffffffff)

def snapshot():
    fb=g.mem(0x0202a000,38400)
    assert g.mem(0x06000000,38400)==fb,'display page was not published'
    return fb,g.mem(0x05000000,1024),g.mem(0x07000000,1024)

for which in ('baseline',which_current):
    for pn in (1,5):
        if which=='baseline':
            symbols=parse_map(baseline/(opt.baseline_release+'.map'));g=GBA(baseline/(opt.baseline_release+'.gba'));g.load(baseline/(opt.baseline_release+'-debug.bin'))
            assert f'{zlib.crc32((baseline/(opt.baseline_release+".gba")).read_bytes()):08x}'==baseline_crc,'wrong baseline'
        else:
            symbols=current_symbols;g=GBA(current_rom);g.load(current_state)
        call('ps4_debug_level60_party')
        if pn==1:g.put(md+0xf40a,bytes([0,255,255,255,255]))
        for name in ('field_custom_resume_event','field_cutscene_resume_index','field_transition_resume_index'):g.wr8(symbols[name],255)
        g.wr16(symbols['ps4_debug_event_pending'],0);g.wr8(symbols['field_custom_resume_phase'],0)
        g.wr8(symbols['ps4_debug_autoplace_pending'],1)
        tload=g.cycles();fload=g.frame();begin('field_runtime_loop',(0x13,0x110,0x120,2))
        assert run_until_break(g,symbols['field_runtime_active'],200),'field entry'
        load_cycles=g.cycles()-tload;load_frames=g.frame()-fload
        begin('ps4_field_menu',(0,0))
        assert run_until_break(g,symbols['ps4_ui_wait_edge'],60),'CAMP entry'
        saved=g.mem(md+0xf100,0xa00);initial=snapshot()
        if which=='baseline':frames[pn]=[initial]
        else:assert initial==frames[pn][0],('initial page differs',pn)
        scratch=Path('/tmp')/f'psiv07_camp_{which}_{pn}.bin';g.save(scratch)
        times=[];sel=0
        for n,key in enumerate(['DOWN']*7+['UP']*7):
            edge(key);t0=g.cycles();assert run_until_break(g,symbols['waitvb'],30),'redraw wait'
            times.append(g.cycles()-t0)
            assert run_until_break(g,symbols['ps4_ui_wait_edge'],30),'next CAMP input'
            sel=(sel+(1 if key=='DOWN' else -1))%7;shot_=snapshot()
            assert g.mem(md+0xf100,0xa00)==saved,'cursor changed saved state'
            assert shot_[1:]==initial[1:],'cursor changed palette or OAM'
            if which=='baseline':frames[pn].append(shot_)
            else:assert shot_==frames[pn][n+1],(pn,n,key,sel,'full-page/palette/OAM mismatch')
        # A chooses the selected SKILL route (2), without a resource change.
        for _ in range(2):
            edge('DOWN');assert run_until_break(g,symbols['ps4_ui_wait_edge'],30)
        edge('A');L.mg_break(0xffffffff);g.run(2,'A')
        assert run_until_break(g,symbols['ps4_field_ability_list'],60),'confirmed route'
        assert g.reg(0)==1,'SKILL selection routed to TECH'
        assert g.mem(md+0xf100,0xa00)==saved,'confirmation before submenu changed resources'
        # B closes the actual CAMP entry and clears ownership on release.
        g.load(scratch);edge('B');L.mg_break(0xffffffff);g.run(2,'B')
        assert run_until_break(g,0x08800000,60),'CAMP cancel'
        assert g.rd8(symbols['ps4_menu_overlay_active'])==g.rd8(symbols['ps4_menu_base_cached'])==0
        assert g.mem(md+0xf100,0xa00)==saved,'cancel changed saved state'
        scratch.unlink()
        row={'ROM':which,'party_count':pn,'cursor_moves':14,'redraw_cycles_before_VBlank':times,
             'Piata_native_loader_cycles':load_cycles,'Piata_native_loader_display_frames':load_frames,
             'median_redraw_cycles':statistics.median(times),'confirm_SKILL_cancel_wrap_saved_palettes_OAM_checked':True}
        cases.append(row);print('PASS CAMP',which,pn,'median redraw',row['median_redraw_cycles'],flush=True)
for pn in (1,5):
    old=next(c for c in cases if c['ROM']=='baseline' and c['party_count']==pn)['median_redraw_cycles']
    new=next(c for c in cases if c['ROM']==which_current and c['party_count']==pn)['median_redraw_cycles']
    assert new<old if opt.baseline_release=='release06' else new<=old*1.05,(pn,old,new)
report={'core':'upstream mGBA 0.10.5','rom':rom_id,'baseline_crc32':baseline_crc,'cases':cases,
        'full_framebuffers_palettes_and_OAM_identical':True,'source_choices_including_solo_MUMBL_preserved':True,
        'first_open_normal_playthrough_or_physical_latency_verified':False}
(O/(prefix+'_MGBA_CAMP_REDRAW.json')).write_text(json.dumps(report,indent=2)+'\n')
