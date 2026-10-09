#!/usr/bin/env python3
from pathlib import Path
import re,subprocess,tempfile
R=Path(__file__).resolve().parents[2];A=R/'source/additional'
with tempfile.TemporaryDirectory(prefix='cp13-native-') as tmp:
    t=Path(tmp);parts=['#include "aotrt.h"\n']
    for name,path in [('native_fn',A/'aot/generated/aot_code.c'),('reference_fn',R/'source/references/cp12_speed/aot/generated/aot_code.c')]:
        fn=re.search(r'(?:AOT_HOT )?u32 aotf12\(.*?(?=(?:AOT_HOT )?u32 aotf13\()',path.read_text(),re.S)[0]
        parts.append(fn.replace('aotf12(',name+'('))
    p=t/'functions.c';p.write_text('\n'.join(parts));exe=t/'check'
    subprocess.run(['gcc','-O2','-w','-DMD_HOST','-I'+str(A),'-I'+str(A/'aot'),str(R/'source/tools_host/check_cp13_native.c'),str(p),*[str(A/f) for f in ['md.c','mdrender.c','m68k.c','m68k_ref.c']],'-o',str(exe)],check=True)
    result=subprocess.run([str(exe)],capture_output=True,text=True,check=True).stdout
    (R.parent/'validation_cp13/NATIVE_EXACT.txt').write_text(result);print(result.strip())
