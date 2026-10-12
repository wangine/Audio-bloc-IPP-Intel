import pathlib,json
p=pathlib.Path(__file__).parent
rows=[json.loads(s) for s in (p/'telemetry.jsonl').read_text().splitlines() if s]
prev=None
for row in rows:
 t=row['telemetry']; c=t['inputs']['cinema']
 sig=(t['armed'],t['physicalArmed'],t['lifecycleStatus'],t['inputMuted'],t['liveFault'],t['sourceWaitCount'],c['streamEpoch'],c['faults'],tuple((d['state'],d['faults'],d['lastResetReason'],d['resyncCount']) for d in t['domains']))
 if sig!=prev:
  print(row['utc'],sig,'input',c['state'],'resume',t['inputResumeError'],'fifo',[(d['fifoFrames'],d['clockFillFrames'],d['pcmOverrangeSamples'],d['deadlineMisses'],d['xruns']) for d in t['domains']], 'peaks',max(t['crossoverPeaks']))
  prev=sig
print('samples',len(rows))
