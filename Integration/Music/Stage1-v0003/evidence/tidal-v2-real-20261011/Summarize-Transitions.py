import csv,json,pathlib
root=pathlib.Path(__file__).absolute().parent
source=[json.loads(x) for x in (root/'transition-source-240s.jsonl').read_text(encoding='utf-8-sig').splitlines() if x.strip()]
driver=list(csv.DictReader((root/'transition-driver-240s.csv').read_text(encoding='utf-8-sig').splitlines()))
music=[s['sources'][0] for s in source]
changes=[]
for a,b in zip(source,source[1:]):
    am,bm=a['sources'][0],b['sources'][0]
    if am['generation']!=bm['generation'] or am['underruns']!=bm['underruns']:
        changes.append({'tickMs':b['observedTickMs'],'generation':bm['generation'],'underruns':bm['underruns'],'delta':bm['underruns']-am['underruns']})
result={'scope':'r3 diagnostic window, user confirmed three TIDAL pause/play cycles; not full-session acceptance',
 'sourceSnapshots':len(source),'driverSnapshots':len(driver),
 'sourceDurationMs':source[-1]['observedTickMs']-source[0]['observedTickMs'],
 'musicGenerations':sorted(set(m['generation'] for m in music)),
 'musicUnderrunsStart':music[0]['underruns'],'musicUnderrunsEnd':music[-1]['underruns'],
 'musicUnderrunsDelta':music[-1]['underruns']-music[0]['underruns'],
 'musicFaultsDelta':music[-1]['faults']-music[0]['faults'],
 'musicLostFramesDelta':music[-1]['lostFrames']-music[0]['lostFrames'],
 'driverStates':sorted(set(int(d['state']) for d in driver if d['info_ok']=='1')),
 'driverNoOwnerSnapshots':sum(d['info_ok']=='1' and d['state']=='0' for d in driver),
 'driverMaxLostFrames':max(int(d['lost']) for d in driver if d['info_ok']=='1'),
 'driverLifetimeLostDelta':int(driver[-1]['lifetime_lost'])-int(driver[0]['lifetime_lost']),
 'driverLifetimeCancelledDelta':int(driver[-1]['lifetime_cancelled'])-int(driver[0]['lifetime_cancelled']),
 'driverWriteFailureDelta':int(driver[-1]['write_failure'])-int(driver[0]['write_failure']),
 'changes':changes,
 'interpretation':'Core r3 counted explicit upstream idle as scheduler underrun. Stop cancellation is separately reported, not certified lossless EOS. No original TIDAL PCM reference; no byte identity claim.'}
(root/'TRANSITION-RESULT.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
print(json.dumps({k:v for k,v in result.items() if k!='changes'},indent=2))
