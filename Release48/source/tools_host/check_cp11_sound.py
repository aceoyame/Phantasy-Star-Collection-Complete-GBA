#!/usr/bin/env python3
"""Compare actual WAV bytes/events to CP10; no hashes and no network needed."""
from pathlib import Path
import concurrent.futures,json,os,subprocess,tempfile
R=Path(__file__).resolve().parents[2];A=R/'source/additional';G=A/'gsnd'
REF=R/'source/references/cp10_speed'
jobs='Anne:81 Anne:88 Rudger:83 Kinds:86 Shilka:8c Amia:90 Huey:a0 Eusis:b0 Nei:c0 Nei:84 Shilka:b3 Rudger:9a'.split()
with tempfile.TemporaryDirectory(prefix='cp11-sound-') as tmp:
    t=Path(tmp);env=dict(os.environ,SND_ROM=str(R/'binaries/RUN_THIS_R48_CP11_TEST.gba'))
    def cc(cmd):subprocess.run(cmd,check=True,capture_output=True)
    cc(['gcc','-O2','-w','-Dsnd_data0=snd_data0_unused','-I'+str(A),'-c',str(G/'gsnd_bank.c'),'-o',str(t/'bank0.o')])
    cc(['gcc','-O2','-c',str(R/'source/tools_host/cp10_snd_bitexact/hostbank.c'),'-o',str(t/'hb.o')])
    cc(['ld','-r',str(t/'bank0.o'),str(t/'hb.o'),'-o',str(t/'bank.o')])
    for tag,src in [('before',REF),('after',A)]:
        files=[G/'test/sndtest.c',src/'md.c',A/'mdrender.c',src/'m68k.c',A/'m68k_ref.c',src/'mdsnd.c',A/'mdsndmix.c']
        cc(['gcc','-O2','-w','-DMD_HOST','-DSND_DEBUG','-I'+str(A),'-I'+str(G),*map(str,files),str(t/'bank.o'),'-o',str(t/tag)])
    def run(job):
        game,id=job.split(':');box='1c0a' if int(id,16)<=0x8f else '1c0b';logs=[];paths=[]
        for tag in ['before','after']:
            path=t/(job.replace(':','_')+'_'+tag+'.wav');paths.append(path)
            p=subprocess.run([str(t/tag),str(R/f'additional_roms/{game}_Adventure_-_English.bin'),'700',str(path),'-inj',f'120:{box}:{id}'],env=env,capture_output=True,text=True,check=True)
            logs.append(p.stdout.splitlines()[-1])
        same=paths[0].read_bytes()==paths[1].read_bytes()
        return {'case':job,'samples_identical':same,'events_identical':logs[0]==logs[1],'driver_summary':logs[1]}
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as ex:rows=list(ex.map(run,jobs))
    result={'frames_per_case':700,'cases':rows,'all_pass':all(r['samples_identical'] and r['events_identical'] for r in rows)}
    out=R.parent/'validation_cp11/SOUND_BIT_EXACT.json';out.parent.mkdir(exist_ok=True);out.write_text(json.dumps(result,indent=2)+'\n')
    print('12/12 sample/event comparisons pass' if result['all_pass'] else 'SOUND COMPARISON FAILED')
    assert result['all_pass']
