"""Offline: retain every state transition and per-DAC extrema from live telemetry."""
import json
import pathlib
import sys

path = pathlib.Path(sys.argv[1])
until = float(sys.argv[2]) if len(sys.argv) > 2 else float('inf')
first = last = None
transitions = []
domains = {}
samples = 0
previous = None
with path.open() as stream:
    for line in stream:
        row = json.loads(line)
        if row['time'] > until:
            continue
        t = row['telemetry']
        keys = ('armed', 'physicalArmed', 'sourceWaitCount', 'sourceResumeCount', 'liveFault')
        state = {k: t[k] for k in keys}
        if first is None:
            first = dict(time=row['time'], **state)
        last = dict(time=row['time'], **state)
        if state != previous:
            transitions.append(last)
            previous = state
        for d in t['domains']:
            result = domains.setdefault(d['id'], {})
            for key in ('faults', 'sinkFaults', 'pcmFaults', 'xruns', 'resyncCount',
                        'groupReplans', 'deadlineMisses', 'maximumCallbackWallExcludingWaitUs',
                        'eventWaitUs', 'pcmNonfiniteSamples', 'pcmOverrangeSamples', 'pcmClippedBlocks'):
                result['max_' + key] = max(result.get('max_' + key, 0), float(d.get(key, 0)))
            for key in ('fifoFrames',):
                result['min_' + key] = min(result.get('min_' + key, float('inf')), float(d.get(key, 0)))
        samples += 1
print(json.dumps(dict(samples=samples, first=first, last=last,
                     until_unix=until if until != float('inf') else None,
                     source_wait_delta=int(last['sourceWaitCount'])-int(first['sourceWaitCount']),
                     source_resume_delta=int(last['sourceResumeCount'])-int(first['sourceResumeCount']),
                     transitions=transitions, domains=domains), indent=2))
