"""Aggregate only recorded Stage03 measurements; incomplete runs are errors."""
import csv
import json
import math
import sys
from collections import Counter
from pathlib import Path

root = Path(sys.argv[1])

def read(path):
    with path.open(encoding="utf-8-sig", newline="") as stream:
        return list(csv.DictReader(stream))

def write(path, data):
    with path.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(data[0]))
        writer.writeheader()
        writer.writerows(data)

numeric, parameters = [], []
for shard in sorted(root.glob("shard[0-9]*")):
    numeric.extend(read(shard / "numerical.csv"))
    parameters.extend(read(shard / "parameters.csv"))
assert len({(r['case'], r['block'], r['frequency_hz']) for r in numeric}) == 2128
assert len([r for r in numeric if r['oracle'] == 'native']) == 34048
assert len(parameters) == 17920
assert len({(r['program'], r['variant'], r['block'], r['output'], r['parameter']) for r in parameters}) == 17920
write(root / "numerical.csv", numeric)
write(root / "parameters.csv", parameters)

# Compare the final corpus to the published Stage02 measurements by case key,
# independent of worker/shard order. Keep every difference visible.
published = Path(__file__).resolve().parents[1] / 'validation/stage02/numerical-complete.csv'
if published.exists():
    baseline_numeric = read(published)
    numeric_keys = ('case', 'block', 'output', 'oracle', 'frequency_hz')
    fields = ('max_absolute_error', 'rms_error', 'peak', 'rms', 'reference_peak',
              'reference_rms', 'bit_mismatches', 'nonfinite', 'sine_amplitude',
              'reference_sine_amplitude', 'phase_rad', 'reference_phase_rad',
              'phase_difference_rad', 'first_sample', 'reference_first_sample',
              'impulse_peak_sample', 'reference_impulse_peak_sample',
              'peak_lag_samples', 'analytic_amplitude_error', 'analytic_phase_error_rad')
    baseline_index = {tuple(r[k] for k in numeric_keys): r for r in baseline_numeric}
    assert len(baseline_index) == len(baseline_numeric) == len(numeric) == 68096
    assert set(baseline_index) == {tuple(r[k] for k in numeric_keys) for r in numeric}
    changes = []
    for row in numeric:
        old = baseline_index[tuple(row[k] for k in numeric_keys)]
        changed = {k: {'before': old[k], 'after': row[k]} for k in fields if old[k] != row[k]}
        if changed:
            changes.append({k: row[k] for k in numeric_keys} | {'fields': changed})
    (root / 'stage02-published-numerical-comparison.json').write_text(
        json.dumps({'rows': len(numeric), 'compared_fields': fields,
                    'different_rows': len(changes), 'changes': changes}, indent=2), encoding='utf-8')

# The first measured binary printed initial taps after event 2 changed length.
# Correct fixture metadata only; retain the original raw CSV and an audit trail.
reactivation = read(root / 'reactivation.csv')
corrections = []
for row in reactivation:
    variant, event = int(row['mode']), int(row['event'])
    initial_taps = {0: 0, 1: 2049, 2: 4097, 3: 8193, 4: 8193}[variant]
    target = initial_taps
    if variant and event >= 2:
        target = 8193 if initial_taps != 8193 else 2049
    if int(row['taps']) != target:
        corrections.append({k: row[k] for k in ('program', 'mode', 'frames', 'event', 'output', 'oracle')} | {'printed_taps': row['taps'], 'target_taps': target})
        row['taps'] = str(target)
if corrections:
    import shutil
    shutil.copyfile(root / 'reactivation.csv', root / 'reactivation-original-labels.csv')
    write(root / 'reactivation.csv', reactivation)
    (root / 'reactivation-label-corrections.json').write_text(json.dumps(corrections, indent=2), encoding='utf-8')

def summarize(data):
    result = {'rows': len(data), 'statuses': dict(Counter(r['status'] for r in data))}
    for key in ('max_absolute_error', 'rms_error', 'analytic_amplitude_error', 'analytic_phase_error_rad',
                'phase_difference_rad', 'peak_lag_samples', 'amplitude_error', 'phase_error_rad', 'max_error'):
        values = [abs(float(r[key])) for r in data if key in r and math.isfinite(float(r[key]))]
        if values:
            result[key] = max(values)
    for key in ('bit_mismatches', 'nonfinite', 'callbacks', 'cpp_new', 'cpp_delete', 'heap_alloc', 'heap_free', 'wait', 'lock', 'io'):
        if data and key in data[0]:
            result[key] = sum(int(r[key]) for r in data)
    if data and 'sine_amplitude' in data[0]:
        values = [abs(float(r['sine_amplitude']) - float(r['reference_sine_amplitude'])) for r in data]
        finite = [v for v in values if math.isfinite(v)]
        result['oracle_amplitude_difference_max'] = max(finite) if finite else None
    return result

summary = {}
for name in ('numerical', 'transitions', 'reactivation'):
    data = read(root / (name + '.csv'))
    for oracle in sorted({r['oracle'] for r in data}):
        summary[name + '-' + oracle] = summarize([r for r in data if r['oracle'] == oracle])
summary['parameters'] = {'rows': len(parameters), 'value_differences':
                         sum(r['native_value'] != r['V07_value'] for r in parameters)}
for name in ('identity', 'controls', 'delay-impulses', 'transition-windows', 'realtime'):
    summary[name] = summarize(read(root / (name + '.csv')))
before, after = read(root / 'before/benchmark.csv'), read(root / 'after/benchmark.csv')
assert len(before) == len(after) == 224
keys = ('module', 'program', 'configuration', 'frames')
indexed = {tuple(r[k] for k in keys): r for r in before}
assert len(indexed) == 224
assert len({tuple(r[k] for k in keys) for r in after}) == 224
comparison = []
for a in after:
    b = indexed[tuple(a[k] for k in keys)]
    row = {k: a[k] for k in keys}
    for metric in ('median_us', 'p99_us', 'max_us', 'median_thread_cycles', 'p99_thread_cycles', 'max_thread_cycles'):
        row['before_' + metric] = b[metric]
        row['after_' + metric] = a[metric]
        row[metric + '_ratio_after_before'] = float(a[metric]) / float(b[metric]) if float(b[metric]) else None
    row.update(before_status=b['status'], after_status=a['status'], budget_us=a['budget_us'])
    comparison.append(row)
write(root / 'benchmark-before-after.csv', comparison)
summary['benchmark-before'] = summarize(before)
summary['benchmark-after'] = summarize(after)
before_fade, after_fade = read(root / 'before/fade-benchmark.csv'), read(root / 'after/fade-benchmark.csv')
assert len(before_fade) == len(after_fade) == 80
indexed_fade = {tuple(r[k] for k in keys): r for r in before_fade}
assert len(indexed_fade) == len({tuple(r[k] for k in keys) for r in after_fade}) == 80
for phase, aggregated in (('before', before_fade), ('after', after_fade)):
    sample_counts = Counter()
    for r in read(root / phase / 'fade-samples.csv'):
        assert None not in r and all(v is not None for v in r.values()), 'Malformed fade sample CSV'
        assert 0 <= int(r['round']) < 20
        assert 0 <= int(r['callback']) < math.ceil(19200 / int(r['frames']))
        assert math.isfinite(float(r['wall_us'])) and float(r['wall_us']) >= 0
        assert int(r['thread_cycles']) >= 0
        sample_counts[tuple(r[k] for k in keys)] += 1
    assert sample_counts == {tuple(r[k] for k in keys): int(r['count']) for r in aggregated}
fade_comparison = []
for a in after_fade:
    b = indexed_fade[tuple(a[k] for k in keys)]
    row = {k: a[k] for k in keys}
    for metric in ('median_us', 'p99_us', 'max_us', 'median_thread_cycles', 'p99_thread_cycles', 'max_thread_cycles'):
        row['before_' + metric] = b[metric]
        row['after_' + metric] = a[metric]
        row[metric + '_ratio_after_before'] = float(a[metric]) / float(b[metric]) if float(b[metric]) else None
    row.update(before_status=b['status'], after_status=a['status'], budget_us=a['budget_us'])
    fade_comparison.append(row)
write(root / 'fade-before-after.csv', fade_comparison)
summary['fade-before'] = summarize(before_fade)
summary['fade-after'] = summarize(after_fade)
(root / 'summary.json').write_text(json.dumps(summary, indent=2), encoding='utf-8')
print(json.dumps(summary, indent=2))
