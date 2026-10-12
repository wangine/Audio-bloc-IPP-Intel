import pathlib,numpy as np,json
p=pathlib.Path(__file__).parent
a=np.genfromtxt(p/'decoder-observation/decoder-trace.csv',delimiter=',',names=True,dtype=np.uint64)
print('events',len(a))
for kind in (1,2,3,4,5):
 x=a[a['kind']==kind]
 print('kind',kind,'rows',len(x))
 if len(x)>1:
  print('  gaps ms p50/p95/p99/max',np.round(np.quantile(np.diff(x['qpc']).astype(float)/10000,[.5,.95,.99,1]),4))
 if kind in (1,2,5) and len(x):
  duration=(x['qpc']-x['a']).astype(float)/10000
  print('  processing duration ms p50/p95/p99/max',np.round(np.quantile(duration,[.5,.95,.99,1]),4))
 if kind==1:
  gaps=x['b'][1:].astype(np.int64)-x['b'][:-1].astype(np.int64)-x['d'][:-1].astype(np.int64)
  print('  noncontiguous capture packets',np.sum(gaps!=0),'lostFrames',gaps[gaps>0].sum(),'flags',np.unique(x['e'],return_counts=True))
  for i in np.where(gaps!=0)[0][:15]:print('  capture gap',int(x[i+1]['qpc']),int(gaps[i]),int(x[i+1]['e']),round((int(x[i+1]['a'])-int(x[i]['a']))/10000,4))
 if kind==2:
  print('  queue range',x['b'].min(),x['b'].max(),'intervals',np.unique(x['c'],return_counts=True))
 if kind==4:print(x)
b=np.genfromtxt(p/'cinema-trace-timing.csv',delimiter=',',names=True,dtype=np.uint64)
b=b[b['kind']==0];d=np.diff(b['source_qpc']).astype(np.int64)/10000
print('Publication gap ms p50/p95/p99/max',np.round(np.quantile(d,[.5,.95,.99,1]),4),'over20ms',np.sum(d>20))
for i in np.argsort(d)[-8:]:
 print('gap',int(b[i]['source_qpc']),round(d[i],4),'epoch',int(b[i]['epoch']),int(b[i+1]['epoch']))
 low=b[i]['source_qpc'];high=b[i+1]['source_qpc']
 for kind in (1,2,3,4,5):
  x=a[(a['kind']==kind)&(a['qpc']>=low)&(a['qpc']<=high)]
  print('  events',kind,'count',len(x),'firstlast',list(x[[0,-1]]) if len(x) else [])
