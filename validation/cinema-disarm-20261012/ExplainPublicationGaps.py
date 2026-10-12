"""Offline: compare actual publication timestamps against the upstream pacer."""
import sys
import numpy as np

t = np.genfromtxt(sys.argv[1], delimiter=',', names=True, dtype=np.uint64)
r = np.genfromtxt(sys.argv[2], delimiter=',', names=True, dtype=np.uint64)
t = t[t['kind'] == 0]
pace = r[r['kind'] == 2]
limit = 4096 * 10_000_000 / 192000
gaps = np.diff(t['source_qpc'].astype(np.int64))
indices = np.flatnonzero((gaps > limit) & (t['epoch'][1:] == t['epoch'][:-1]))
print('same-epoch sequence discontinuities', np.count_nonzero(
    (np.diff(t['sequence'].astype(np.int64)) != 1) & (t['epoch'][1:] == t['epoch'][:-1])))
for i in indices:
    begin, end = int(t[i]['source_qpc']), int(t[i+1]['source_qpc'])
    p = pace[(pace['qpc'] >= begin) & (pace['qpc'] <= end)]
    print('publication gap', begin, end, 'ms', (end-begin)/10000,
          'sequence', int(t[i]['sequence']), int(t[i+1]['sequence']),
          'pacer submissions inside gap', len(p), 'frames', len(p)*128,
          'pacer max gap ms', np.diff(p['qpc'].astype(np.int64)).max()/10000 if len(p)>1 else None,
          'minimum queuedAfter', int(p['b'].min()) if len(p) else None)
