#!/usr/bin/env python3
"""Compare actual WAV bytes/events to CP11; no hashes and no network needed."""
from pathlib import Path
import concurrent.futures,json,os,re,subprocess,tempfile
R=Path(__file__).resolve().parents[2];A=R/'source/additional';G=A/'gsnd'
REF=R/'source/references/cp11_speed'
jobs='Anne:81 Anne:88 Rudger:83 Kinds:86 Shilka:8c Amia:90 Huey:a0 Eusis:b0 Nei:c0 Nei:84 Shilka:b3 Rudger:9a'.split()
with tempfile.TemporaryDirectory(prefix='cp13-sound-') as tmp:
    t=Path(tmp);env=dict(os.environ,SND_ROM=str(R/'binaries/RUN_THIS_R48_CP13_TEST.gba'))
    def cc(cmd):subprocess.run(cmd,check=True,capture_output=True)
    cc(['gcc','-O2','-w','-Dsnd_data0=snd_data0_unused','-I'+str(A),'-c',str(G/'gsnd_bank.c'),'-o',str(t/'bank0.o')])
    cc(['gcc','-O2','-c',str(R/'source/tools_host/cp10_snd_bitexact/hostbank.c'),'-o',str(t/'hb.o')])
    cc(['ld','-r',str(t/'bank0.o'),str(t/'hb.o'),'-o',str(t/'bank.o')])
    for tag,src in [('before',REF),('after',A)]:
        files=[G/'test/sndtest.c',src/'md.c',A/'mdrender.c',src/'m68k.c',A/'m68k_ref.c',src/'mdsnd.c',src/'mdsndmix.c']
        cc(['gcc','-O2','-w','-DMD_HOST','-DSND_DEBUG','-I'+str(A),'-I'+str(G),*map(str,files),str(t/'bank.o'),'-o',str(t/tag)])
    def run(job):
        game,id=job.split(':');box='1c0a' if int(id,16)<=0x8f else '1c0b';logs=[];paths=[];events=[]
        for tag in ['before','after']:
            path=t/(job.replace(':','_')+'_'+tag+'.wav');paths.append(path);ev=t/(job.replace(':','_')+'_'+tag+'.ev');events.append(ev)
            p=subprocess.run([str(t/tag),str(R/f'additional_roms/{game}_Adventure_-_English.bin'),'700',str(path),'-inj',f'120:{box}:{id}','-ev',str(ev)],env=env,capture_output=True,text=True,check=True)
            logs.append(p.stdout.splitlines()[-1])
        same=paths[0].read_bytes()==paths[1].read_bytes()
        semantic=lambda s:re.sub(r'z80 slices \d+','z80 slices <implementation count>',s)
        return {'case':job,'samples_identical':same,'events_identical':events[0].read_bytes()==events[1].read_bytes(),'stats_identical':semantic(logs[0])==semantic(logs[1]),'before_summary':logs[0],'driver_summary':logs[1]}
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as ex:rows=list(ex.map(run,jobs))
    result={'frames_per_case':700,'cases':rows,'all_pass':all(r['samples_identical'] and r['events_identical'] and r['stats_identical'] for r in rows)}
    traces=[]
    for tag in ['before','after']:
        trace=t/(tag+'.trace');traces.append(trace)
        subprocess.run([str(t/tag),str(R/'additional_roms/Anne_Adventure_-_English.bin'),'250',str(t/(tag+'_trace.wav')),'-inj','120:1c0a:88','-trace',str(trace)],env=env,check=True,capture_output=True)
    result['instruction_trace_identical']=traces[0].read_bytes()==traces[1].read_bytes()
    if not result['instruction_trace_identical']:
        for line,(before,after) in enumerate(zip(traces[0].read_text().splitlines(),traces[1].read_text().splitlines()),1):
            if before!=after:result['first_trace_difference']={'line':line,'before':before,'after':after};break
    result['all_pass'] &= result['instruction_trace_identical']
    out=R.parent/'validation_cp13/SOUND_BIT_EXACT.json';out.parent.mkdir(exist_ok=True);out.write_text(json.dumps(result,indent=2)+'\n')
    print('12/12 sample/event comparisons pass' if result['all_pass'] else 'SOUND COMPARISON FAILED')
    assert result['all_pass']
