import pathlib
import sys
import numpy as np

path = pathlib.Path(sys.argv[1])
rows = np.genfromtxt(path, delimiter=',', names=True, dtype=np.uint64)
start = int(rows['qpc'].min())
for kind in (1, 2, 3, 4, 5):
    group = rows[rows['kind'] == kind]
    gaps = np.diff(group['qpc'].astype(np.int64)) / 10_000
    print('kind', kind, 'rows', len(group), 'gapMs p50/p95/p99/max',
          np.round(np.quantile(gaps, [.5, .95, .99, 1]), 4) if len(gaps) else [])
    if kind in (1, 2, 5) and len(group):
        print(' processing maxMs', (group['qpc'] - group['a']).max() / 10_000)
    if kind == 2 and len(gaps):
        for i in np.argsort(gaps)[-6:]:
            print(' pace delay at seconds', round((int(group[i+1]['qpc'])-start)/1e7, 4),
                  'gapMs', round(float(gaps[i]), 4), 'queuedAfter', int(group[i+1]['b']),
                  'generation', int(group[i+1]['d']))
capture = rows[rows['kind'] == 1]
frame_gaps = (capture['b'][1:].astype(np.int64) - capture['b'][:-1].astype(np.int64)
              - capture['d'][:-1].astype(np.int64))
print('capture packets', len(capture), 'frame-index gaps', np.count_nonzero(frame_gaps),
      'flags', np.unique(capture['e'], return_counts=True))
for i in np.flatnonzero(capture['e']):
    print(' flagged packet at seconds', round((int(capture[i]['a'])-start)/1e7, 4),
          'frame', int(capture[i]['b']), 'flags', int(capture[i]['e']),
          'holdMs', round((int(capture[i]['qpc'])-int(capture[i]['a']))/1e4, 4),
          'ownerArrivalGapMs', round((int(capture[i]['a'])-int(capture[i-1]['a']))/1e4, 4) if i else None)
print('restarts', rows[rows['kind'] == 4])
