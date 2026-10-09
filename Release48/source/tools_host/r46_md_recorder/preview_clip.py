import pickle, sys
from PIL import Image
from clipsim import render_tick
def preview(fn, out, bg=None, roles=None):
    d=pickle.load(open(fn,'rb')); c=d['clip']
    fi=d['fighters']; fx={k+1:f['x'] for k,f in enumerate(fi[:9])}
    caster=c.get('caster'); target=c.get('target')
    def anc(a):
        k,i=a
        if k==0: return 0
        if k==1: return fx.get(i+1)
        if k==2: return fx.get(i+6)
        if k==3: return fx.get(caster) if roles is None else roles['caster']
        if k==4: return fx.get(target) if roles is None else roles['target']
    if bg is None: bg=Image.new('RGB',(240,160),(30,30,60))
    md=d['preview']; n=min(len(md), (c['ticks']+15)//16)
    W=Image.new('RGB',(320+240, 224*n))
    for i in range(n):
        W.paste(md[i],(0,224*i)); W.paste(render_tick(c,d['blocks'],i*16,bg,anc),(320,224*i))
    W.save(out)
if __name__=='__main__': preview(sys.argv[1],sys.argv[2])
