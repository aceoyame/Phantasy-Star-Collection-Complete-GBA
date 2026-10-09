#!/usr/bin/env python3
"""cmpwav.py RUNTIME.wav REF.f32 [PNG]   compare the runtime render with the reference render (offline QA)."""
import sys, wave
import numpy as np
from scipy import signal

w = wave.open(sys.argv[1]); n = w.getnframes()
a = np.frombuffer(w.readframes(n), dtype=np.int16).reshape(-1, 2).astype(np.float64) / 256.0   # out8 units
r = np.fromfile(sys.argv[2], dtype=np.float32).reshape(-1, 2).astype(np.float64)               # S units (out8 = S/4)
FS = 13379.0
r = signal.resample(r, int(round(len(r) * FS / (7670453.6 / 144.0))), axis=0) / 4.0
m = min(len(a), len(r)); a = a[:m]; r = r[:m]
am = a.mean(axis=1); rm = r.mean(axis=1)
print('samples %d (%.1f s)   rms runtime %.2f  reference %.2f   peak %.1f / %.1f' % (m, m / FS, am.std(), rm.std(), np.abs(a).max(), np.abs(r).max()))
FR = 224
nf = m // FR
fa = np.sqrt((am[:nf * FR].reshape(nf, FR) ** 2).mean(axis=1)); fr = np.sqrt((rm[:nf * FR].reshape(nf, FR) ** 2).mean(axis=1))
act = fr > 0.5
if act.sum() > 10:
    print('frames with sound %d / %d   per-frame rms correlation %.3f   level ratio (median) %.3f  (10%%..90%%: %.3f..%.3f)' % (
        act.sum(), nf, np.corrcoef(fa[act], fr[act])[0, 1], np.median(fa[act] / fr[act]), *np.percentile(fa[act] / fr[act], [10, 90])))
# log-spectral distance on 1/3-octave-ish bands over 8192-sample chunks
bands = np.geomspace(100, 6500, 21)
def bandspec(x):
    f, t, S = signal.stft(x, FS, nperseg=1024, noverlap=512)
    P = np.abs(S) ** 2
    out = np.zeros((len(bands) - 1, P.shape[1]))
    for i in range(len(bands) - 1):
        sel = (f >= bands[i]) & (f < bands[i + 1]); out[i] = P[sel].sum(axis=0) + 1e-9
    return out
rm8 = np.floor(r.mean(axis=1) + 0.5)          # the reference as an 8 bit output would carry it (same quantisation floor as the runtime)
A = bandspec(am); Rr = bandspec(rm8)
mask = Rr.sum(axis=0) > np.percentile(Rr.sum(axis=0), 25)
if mask.sum() > 5:
    la = 10 * np.log10(A[:, mask]); lr = 10 * np.log10(Rr[:, mask])
    d = la - lr
    print('band levels (mean dB, runtime - reference) per 1/3-oct band:'); print(' '.join('%+.1f' % v for v in d.mean(axis=1)))
    print('log-spectral distance (rms over bands, mean over frames): %.2f dB   (frames %d)' % (np.sqrt((d ** 2).mean(axis=0)).mean(), mask.sum()))
if len(sys.argv) > 3:
    import matplotlib; matplotlib.use('Agg'); import matplotlib.pyplot as plt
    fig, ax = plt.subplots(2, 1, figsize=(14, 7), sharex=True)
    for x, t, k in ((rm, 'reference (ymfm, offline)', 0), (am, 'runtime (baked snapshots)', 1)):
        f, tt, S = signal.stft(x, FS, nperseg=512, noverlap=384)
        ax[k].pcolormesh(tt, f, 20 * np.log10(np.abs(S) + 1e-3), vmin=-20, vmax=45, shading='auto'); ax[k].set_title(t); ax[k].set_ylabel('Hz')
    plt.tight_layout(); plt.savefig(sys.argv[3], dpi=70)
