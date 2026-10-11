"""Summarize bounded read-only captures; not a PCM byte-identity audit."""
import argparse, csv, json, pathlib

p=argparse.ArgumentParser()
p.add_argument('--source',required=True)
p.add_argument('--driver',required=True)
p.add_argument('--output',required=True)
p.add_argument('--label',required=True)
a=p.parse_args()
out=pathlib.Path(a.output)
if out.exists(): raise RuntimeError('Refusing to overwrite a prior report')
rows=[json.loads(x) for x in pathlib.Path(a.source).read_text(encoding='utf-8-sig').splitlines() if x.strip()]
good=[r for r in rows if 'snapshot' in r]
if len(good)<2: raise RuntimeError('Insufficient source samples')
driver=list(csv.DictReader(pathlib.Path(a.driver).read_text(encoding='utf-8-sig').splitlines()))
for row in driver:
    for k,v in row.items(): row[k]=int(v)
if len(driver)<2: raise RuntimeError('Insufficient driver samples')
driver_good=[r for r in driver if r['info_ok']]
if len(driver_good)<2: raise RuntimeError('Insufficient valid driver INFO samples')
changes=[]
previous=None
for row in good:
    m=row['snapshot']['sources'][0]
    if previous is None or any(m[k]!=previous[k] for k in ('generation','state','underruns')):
        changes.append(dict(tickMs=row['tickMs'],utc=row['utc'],**{k:m[k] for k in ('state','generation','rate','validBits','underruns','framesIn','fillFrames')}))
    previous=m
driver_changes=[]
previous=None
for row in driver_good:
    if previous is None or any(row[k]!=previous[k] for k in ('state','generation','rate','bits')):
        driver_changes.append(row)
    previous=row
first,last=good[0]['snapshot'],good[-1]['snapshot']
source_results=[]
for i in range(2):
    before,after=first['sources'][i],last['sources'][i]
    metrics=('framesIn','framesOut','packets','underruns','lostFrames','faults','overruns')
    source_results.append(dict(role=before['role'],initial={k:before[k] for k in metrics},
        final={k:after[k] for k in metrics},delta={k:after[k]-before[k] for k in metrics},
        generations=sorted({r['snapshot']['sources'][i]['generation'] for r in good}),
        signalObserved=any(r['snapshot']['sources'][i]['preRmsL']>0 or r['snapshot']['sources'][i]['preRmsR']>0 for r in good)))
result=dict(label=a.label,scope='READ_ONLY_REAL_PLAYBACK_COUNTERS_NOT_BYTE_IDENTITY',
    sourceSamples=len(good),sourceReadErrors=[r for r in rows if 'error' in r],
    sourceDurationMs=good[-1]['tickMs']-good[0]['tickMs'],startUtc=good[0]['utc'],endUtc=good[-1]['utc'],
    sessionIds=sorted({r['snapshot']['sessionId'] for r in good}),sources=source_results,
    publisherDropsDelta=last['publisherDrops']-first['publisherDrops'],clockLateBlocksDelta=last['clockLateBlocks']-first['clockLateBlocks'],
    sourceChanges=changes,driverSamples=len(driver),driverDurationMs=driver[-1]['tick_ms']-driver[0]['tick_ms'],
    driverReadErrors=[r for r in driver if not r['info_ok']],driverChanges=driver_changes,
    lifetimeLostInitial=driver_good[0]['lifetime_lost'],lifetimeLostFinal=driver_good[-1]['lifetime_lost'],
    lifetimeCancelledDelta=driver_good[-1]['lifetime_cancelled']-driver_good[0]['lifetime_cancelled'],
    writeFailureDelta=driver[-1]['write_failure']-driver[0]['write_failure'],
    limitations=['Snapshot sampling does not prove byte identity or every short transient',
        'Intentional STOP cancellation counters remain reported; this is not a lossless EOS qualification',
        'WASAPI Shared/Exclusive mode remains unverified by driver metadata',
        'Manual pause/play requires human confirmation and matching captured transitions'])
out.write_text(json.dumps(result,indent=2,ensure_ascii=False)+'\n',encoding='utf-8')
print(json.dumps({k:result[k] for k in ('label','sourceSamples','sourceDurationMs','sources','publisherDropsDelta','clockLateBlocksDelta','lifetimeCancelledDelta','writeFailureDelta')},indent=2))
