import numpy as np,sys
fr=304*59.7275; rr=49716.0
a=np.fromfile(sys.argv[1] if len(sys.argv)>1 else 'fast.raw',np.int16).astype(float)
b=np.fromfile('ref.raw',np.int16).astype(float)
# resample ref onto fast timebase
t=np.arange(len(a))/fr; b=np.interp(t,np.arange(len(b))/rr,b)
W=4096; cs=[]; ea=[];eb=[]
for s in range(0,len(a)-W,W):
    x=a[s:s+W]*np.hanning(W); y=b[s:s+W]*np.hanning(W)
    X=np.abs(np.fft.rfft(x)); Y=np.abs(np.fft.rfft(y)); f=np.fft.rfftfreq(W,1/fr); m=(f>40)&(f<6000)
    if Y[m].sum()<1e-3 or X[m].sum()<1e-3: continue
    lx=X[m]; ly=Y[m]; cs.append(lx@ly/np.linalg.norm(lx)/np.linalg.norm(ly))
H=int(fr*0.02)
for s in range(0,len(a)-H,H): ea.append(np.sqrt((a[s:s+H]**2).mean())); eb.append(np.sqrt((b[s:s+H]**2).mean()))
print('logspec cosine median %.3f mean %.3f  env corr %.3f  rms ratio %.2f'%(np.median(cs),np.mean(cs),np.corrcoef(ea,eb)[0,1],np.mean(ea)/np.mean(eb)))
