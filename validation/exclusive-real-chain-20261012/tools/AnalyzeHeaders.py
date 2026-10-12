import pathlib,struct,mmap,json,collections
root=pathlib.Path(__file__).resolve().parents[1]
def headers(path):
    with path.open('rb') as f:
        data=mmap.mmap(f.fileno(),0,access=mmap.ACCESS_READ)
        if (len(data)-256)%24712:raise RuntimeError('Bad record size')
        result=[]
        for p in range(256,len(data),24712):
            qpc=struct.unpack_from('<Q',data,p)[0];h=struct.unpack_from('<16s9Q10I',data,p+8)
            result.append(dict(qpc=qpc,epoch=h[1],sequence=h[2],first=h[3],control=h[4],gen=list(h[5:8]),sourceQpc=h[8],revision=h[9],frames=h[10],valid=h[11],discontinuity=h[12]))
        data.close();return result
results={}
for role in ('input','output'):
    rows=headers(root/('baseline2-stage2-'+role+'.bin'));changes=[];gaps=0
    for a,b in zip(rows,rows[1:]):
        if b['sequence']!=a['sequence']+1:gaps+=1
        reasons=[f'generation_{i}:{a["gen"][i]}->{b["gen"][i]}' for i in range(3) if a['gen'][i]!=b['gen'][i]]
        if a['revision']!=b['revision']:reasons.append('music_revision')
        if reasons:changes.append(dict(sequence=b['sequence'],first=b['first'],qpc=b['qpc'],reasons=reasons,oldGen=a['gen'],newGen=b['gen'],oldValidity=a['valid'],newValidity=b['valid']))
    summary=dict(records=len(rows),gaps=gaps,firstSequence=rows[0]['sequence'],lastSequence=rows[-1]['sequence'],validityCounts=dict(collections.Counter(r['valid'] for r in rows)),generationCounts={str(i):dict(collections.Counter(r['gen'][i] for r in rows)) for i in range(3)},resetConditionTransitions=len(changes),changes=changes)
    results[role]=summary
(root/'baseline-header-analysis.json').write_text(json.dumps(results,indent=2))
print(json.dumps({role:{k:v for k,v in summary.items() if k!='changes'} for role,summary in results.items()},indent=2))
print('First reset conditions:',json.dumps(results['input']['changes'][:10]))
