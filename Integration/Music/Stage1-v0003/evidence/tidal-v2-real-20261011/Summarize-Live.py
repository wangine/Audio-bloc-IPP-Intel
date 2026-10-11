import hashlib
import json
from pathlib import Path

root = Path(__file__).resolve().parent

def read(name):
    return json.loads((root / name).read_text(encoding='utf-8-sig'))

def case(name):
    rows = [json.loads(line) for line in (root / name).read_text(encoding='utf-8-sig').splitlines() if line]
    first, last = rows[0], rows[-1]
    sources = []
    for i, role in enumerate(('MUSIC', 'AUX')):
        values = [r['sources'][i] for r in rows]
        deltas = {key: last['sources'][i][key] - first['sources'][i][key]
                  for key in ('packets', 'framesIn', 'framesOut', 'faults', 'underruns', 'overruns')}
        sources.append(dict(role=role, providerIds=sorted(set(v['providerId'] for v in values)),
            states=sorted(set(v['state'] for v in values)),
            formats=sorted(set(f"{v['validBits']}-valid-in-{v['containerBits']}/{v['rate']}" for v in values)),
            modes=sorted(set(v['mode'] for v in values)),
            generations=sorted(set(v['generation'] for v in values)),
            startUnderruns=values[0]['underruns'], endUnderruns=values[-1]['underruns'],
            deltas=deltas, lostFramesMax=max(v['lostFrames'] for v in values),
            nonzeroSignalSnapshots=sum(v['preRmsL'] > 0 or v['preRmsR'] > 0 for v in values),
            nonzeroSideSnapshots=sum(v['preSideRms'] > 0 for v in values),
            sampledPrePeakMax=max(max(v['samplePeakL'], v['samplePeakR']) for v in values),
            sampledPostPeakMax=max(max(v['outputPeakL'], v['outputPeakR']) for v in values)))
    pass_continuity = all(s['states'] == [2] and len(s['generations']) == 1
                          and all(s['deltas'][k] == 0 for k in ('faults', 'underruns', 'overruns'))
                          and s['lostFramesMax'] == 0 for s in sources)
    return dict(file=name, sha256=hashlib.sha256((root / name).read_bytes()).hexdigest(),
        durationMs=last['observedTickMs'] - first['observedTickMs'], snapshots=len(rows),
        bothSignalSnapshots=sum(all(s['preRmsL'] > 0 or s['preRmsR'] > 0 for s in r['sources']) for r in rows),
        sources=sources, publisherDropsStart=first['publisherDrops'], publisherDropsEnd=last['publisherDrops'],
        clockLateBlocksStart=first['clockLateBlocks'], clockLateBlocksEnd=last['clockLateBlocks'],
        boundedContinuityPass=pass_continuity)

summary = dict(date='2026-10-11', binaries=read('binary-identity.json'),
    selection=dict(beforeProvider=read('before-selection.json')['sources'][0]['providerId'],
        responseStatus=read('selection-response.json')['status'],
        note='MUSIC was already raw at the first current snapshot. The same raw endpoint was reopened through IPC. '
             '540 cumulative MUSIC underruns existed before this request; no reset or attribution was made.'),
    exclusive=dict(userConfirmed=True, sourcePacketMode=0,
        note='Exclusive is user-reported in TIDAL. The kernel emits mode=UNKNOWN metadata; no access failure is inferred.'),
    firstWindow=case('snapshots-180s.jsonl'), dualWindow=case('snapshots-dual-180s.jsonl'),
    auxFormat=dict(renderConfigured='24-valid-in-32/96000', captureConfigured='16/44100',
        captureActual='24-valid-in-32/44100', sharedMixReportedByProbe=44100,
        note='Read-only endpoint registry properties and actual ingress distinguish the configured render format '
             'from the capture format. No original AUX PCM 24/96 identity certification.'),
    auxStereo=dict(observation='Every AUX pre-SRC Side RMS snapshot in dual window was zero; L/R RMS equal.',
        antiPhaseProbe=read('stereo-probe-loopback.json'), leftOnlyControl=read('left-control-loopback.json'),
        conclusion='INCONCLUSIVE localization: both Windows loopback probes returned zero. '
                   'No independent witness that the known signal reached the AUX capture stream; '
                   'do not blame Spotify, Windows or the driver from this test alone.'),
    limitations=['Three-minute windows do not certify all host loads or explain historical 540 underruns.',
        'Telemetry observes source continuity/counters, not every original TIDAL PCM byte.',
        'Instantaneous pre/post SRC meter windows differ and cannot prove unity or stereo preservation sample-for-sample.',
        'Peak maxima are maxima of sampled telemetry snapshots, not full-session sample maxima.',
        'AUX rate mismatch and mono observation remain unresolved. No DSP, driver or Windows settings were modified.'])
(root / 'LIVE-RESULT.json').write_text(json.dumps(summary, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
print(json.dumps({k: dict(durationMs=summary[k]['durationMs'], snapshots=summary[k]['snapshots'],
                         passContinuity=summary[k]['boundedContinuityPass'],
                         bothSignal=summary[k]['bothSignalSnapshots'])
                  for k in ('firstWindow','dualWindow')}, indent=2))
