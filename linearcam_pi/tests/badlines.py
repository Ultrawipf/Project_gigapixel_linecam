# Counts bad lines in a raw PIO dump: broken markers or an MSB lane that is not smooth.
# usage: badlines.py dump words lag [skip]
import sys, numpy as np
f, words, lag = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
skip = int(sys.argv[4]) if len(sys.argv) > 4 else 5
d = np.fromfile(f, dtype=np.uint32)
d = d[:d.size // words * words].reshape(-1, words)[skip:]
mk = np.arange(31, -1, -1, dtype=np.uint32)
markers_bad = int(np.count_nonzero(~np.all(d[:, :32] == mk, axis=1)))
data = d[:, 32:]
msb = []
for b in (3, 1):
    x = ((data >> (8*b)) & 0xff).astype(np.float32)
    msb.append(np.sqrt(np.mean((x[:, :-lag] - x[:, lag:])**2, axis=1)))
msb = np.maximum(msb[0], msb[1])
med = np.median(msb)
bad = np.nonzero(msb > max(3*med, med + 2))[0]
print(f"lines {d.shape[0]}, markers bad {markers_bad}, MSB rms median {med:.2f} max {msb.max():.2f}, bad lines {len(bad)}" + (f" e.g. {list(bad[:8] + skip)}" if len(bad) else ""))
