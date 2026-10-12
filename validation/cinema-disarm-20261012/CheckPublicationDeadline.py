"""Measured continuous publication must fit the unchanged 4096-frame sink target."""
import sys
import numpy as np

rows = np.genfromtxt(sys.argv[1], delimiter=',', names=True, dtype=np.uint64)
rows = rows[rows['kind']==0]
assert len(rows)>2
same = rows['epoch'][1:]==rows['epoch'][:-1]
gaps = np.diff(rows['source_qpc'].astype(np.int64))[same]/10000
limit_ms = 4096*1000/192000
print('publication gap ms p50/p95/p99/max', np.quantile(gaps,[.5,.95,.99,1]))
print('unchanged sink target ms',limit_ms,'exceeded gaps',np.count_nonzero(gaps>limit_ms))
if gaps.max()>limit_ms:
    print('FAIL continuous source delivery missed unchanged sink reserve')
    sys.exit(1)
print('PASS measured publication stayed within unchanged sink reserve')
