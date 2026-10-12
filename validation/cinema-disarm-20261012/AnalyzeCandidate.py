import json,pathlib
p=pathlib.Path(__file__).parent
rs=[json.loads(s) for s in (p/'candidate-live.jsonl').read_text().splitlines()]
base=rs[0]['time'];previous=None
for row in rs:
 t=row['telemetry'];c=t['inputs']['cinema'];ds=t['domains']
 signature=(t['armed'],t['physicalArmed'],t['liveFault'],t['sourceWaitCount'],c['streamEpoch'],c['faults'],tuple((d['state'],d['faults'],d['lastResetReason']) for d in ds))
 if signature!=previous:
  print(round(row['time']-base,3),signature,'source',c['firstFrame'],c['state'],'FIFO',[(d['fifoFrames'],round(d['clockFillFrames'],1),d['ppm'],d['groupReplans']) for d in ds])
  previous=signature
print('duration',rs[-1]['time']-base,'rows',len(rs),'fatalRows',sum(bool(r['telemetry']['liveFault']) for r in rs))
