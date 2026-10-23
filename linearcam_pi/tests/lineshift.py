# Detects lines shifted by whole samples in a raw PIO dump of a static scene.
# Every line of a static scene should match the median line. A line that matches better when shifted
# by one or more 16 bit samples started its readout one clock off (TG seen at a different clock edge).
# usage: lineshift.py dump words [skip] [maxshift]
import sys, numpy as np
f, words = sys.argv[1], int(sys.argv[2])
skip = int(sys.argv[3]) if len(sys.argv) > 3 else 8
maxs = int(sys.argv[4]) if len(sys.argv) > 4 else 4
d = np.fromfile(f, dtype=np.uint32)
d = d[:d.size // words * words].reshape(-1, words)[skip:, 32:]  # drop start-up lines and markers
# Two 16 bit samples per word, high half first
px = np.empty((d.shape[0], d.shape[1] * 2), dtype=np.float32)
px[:, 0::2] = d >> 16
px[:, 1::2] = d & 0xffff
ref = np.median(px, axis=0)
n = px.shape[1]
lo, hi = maxs, n - maxs
shifts = []
for line in px:
    err = [np.mean(np.abs(line[lo + s:hi + s] - ref[lo:hi])) for s in range(-maxs, maxs + 1)]
    shifts.append(int(np.argmin(err)) - maxs)
shifts = np.array(shifts)
vals, counts = np.unique(shifts, return_counts=True)
bad = np.nonzero(shifts != 0)[0]
print(f"lines {len(shifts)}, shifted {len(bad)} ({100*len(bad)/len(shifts):.2f}%), shift histogram " +
      " ".join(f"{v:+d}:{c}" for v, c in zip(vals, counts)) + (f", e.g. lines {list(bad[:8] + skip)}" if len(bad) else ""))
