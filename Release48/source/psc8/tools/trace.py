import sys, struct, subprocess
from bench import GBA, CASES, L
def syms(elf): return {l.split()[2]:int(l.split()[0],16) for l in subprocess.run(['llvm-nm',elf],capture_output=True,text=True).stdout.splitlines() if len(l.split())==3}
def trace(rom, elf, fn, pcoff, n, name='GAIDEN'):
    s = syms(elf); g = GBA(rom, entry=0x086ca500); g.put(0x0203FF00, struct.pack('<IIII', *CASES[name], 0))
    out = []
    for i in range(n):
        L.mg_break(s[fn]); 
        for k in range(50):
            g.run(1)
            if L.mg_hit(): break
        cpu = s['cpu']; out.append((g.reg(1), g.rd16(cpu+pcoff[0]), g.rd32(cpu+pcoff[1])))
    L.mg_break(0xffffffff)
    return out
