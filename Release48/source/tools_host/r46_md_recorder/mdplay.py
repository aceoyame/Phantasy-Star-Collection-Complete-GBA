import sys; sys.path.insert(0,'/home/claude/w/md')
from md import *; from mdmont import montage
def msession(state,cmds,out='shots/mdplay_m.png',save=None,cols=4):
    m=MD()
    if state: m.load(state)
    ps=[]
    for c in cmds:
        p=c.split()
        if p[0]=='wait': m.run(int(p[1]))
        elif p[0]=='shot':
            f='shots/_m%02d.png'%len(ps); m.shot(f); ps.append(f)
        else:
            n=int(p[1]) if len(p)>1 else 1; w=int(p[2]) if len(p)>2 else 20
            for _ in range(n): m.press(p[0],3,w)
    if ps: montage(ps,out,cols)
    if save: m.save(save)
    return m
if __name__=='__main__':
    msession(None if sys.argv[1]=='-' else sys.argv[1], sys.argv[3].split(';'), save=None if sys.argv[2]=='-' else sys.argv[2])
