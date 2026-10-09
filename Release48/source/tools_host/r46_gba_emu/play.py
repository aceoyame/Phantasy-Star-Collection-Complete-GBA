import sys, os
from gba import *
from mont import montage
ROM=os.environ.get('ROM','/home/claude/w/R45_orig.gba')
def session(state, cmds, out='shots/play_m.png', save=None, cols=4):
    g=GBA(ROM, entry=0x08800000)
    if state: g.load(state)
    ps=[]
    for i,c in enumerate(cmds):
        parts=c.split()
        if parts[0]=='wait': g.run(int(parts[1]))
        elif parts[0]=='hold': g.run(int(parts[2]), parts[1])
        elif parts[0]=='shot':
            p='shots/_p%02d.png'%len(ps); g.shot(p); ps.append(p)
        else:
            k=parts[0]; n=int(parts[1]) if len(parts)>1 else 1; w=int(parts[2]) if len(parts)>2 else 20
            for _ in range(n): g.press(k,3,w)
    if ps: montage(ps,out,cols)
    if save: g.save(save)
    return g
if __name__=='__main__':
    state=sys.argv[1] if sys.argv[1]!='-' else None
    save=sys.argv[2] if sys.argv[2]!='-' else None
    session(state, sys.argv[3].split(';'), save=save)
