import pathlib,numpy as np
p=pathlib.Path(__file__).parent
a=np.genfromtxt(p/'decoder-observation-v15/decoder-trace.csv',delimiter=',',names=True,dtype=np.uint64)
for k in (1,2,3,4,5):
 x=a[a['kind']==k];d=np.diff(x['qpc']).astype(float)/10000
 print('kind',k,'count',len(x),'gapMs p50/p95/p99/max',np.round(np.quantile(d,[.5,.95,.99,1]),4) if len(d) else '')
 if k in (1,2,5):print('duration ms max',float((x['qpc']-x['a']).max())/10000)
c=a[a['kind']==1];gaps=c['b'][1:].astype(np.int64)-c['b'][:-1].astype(np.int64)-c['d'][:-1].astype(np.int64)
print('capture frame gaps',np.count_nonzero(gaps),'lostFrames',gaps[gaps>0].sum(),'flags',np.unique(c['e'],return_counts=True))
for i in np.where((gaps!=0)|(c['e'][1:]!=0))[0]:
 i+=1;print('capture event',tuple(c[i]),'frameGap',gaps[i-1],'arrivalGapMs',(int(c[i]['a'])-int(c[i-1]['a']))/10000,'priorProcessingMs',(int(c[i-1]['qpc'])-int(c[i-1]['a']))/10000)
 print('preceding restart/write/capture',a[(a['qpc']>c[i]['qpc']-1000000)&(a['qpc']<c[i]['qpc'])&np.isin(a['kind'],[1,4,5])][-15:])
print('restarts',a[a['kind']==4])
