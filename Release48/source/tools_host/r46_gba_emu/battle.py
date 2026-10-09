from play import *
import sys
def round_defend(g, members=5):
    g.press('A',3,30)  # COMD
    for m in range(members):
        for _ in range(4): g.press('RIGHT',3,8)
        g.press('A',3,25)
def capture(g, frames, every, prefix):
    ps=[]
    for i in range(frames//every):
        g.run(every); p='shots/%s_%03d.png'%(prefix,i); g.shot(p); ps.append(p)
    return ps
if __name__=='__main__':
    st=sys.argv[1]; rounds=int(sys.argv[2]); prefix=sys.argv[3]
    g=session(st,[])
    for r in range(rounds):
        round_defend(g)
        ps=capture(g, 600, 10, '%s_r%d'%(prefix,r))
        montage(ps,'shots/%s_r%d_m.png'%(prefix,r),cols=10)
        g.save('st_%s_r%d.bin'%(prefix,r))
def round_attack(g, members=5):
    g.press('A',3,30)
    for m in range(members):
        g.press('A',3,15); g.press('A',3,25)
