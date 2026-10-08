"""Render recorded before/after CPU tables without excluding failed rows."""
import csv
import sys
from pathlib import Path

root, destination = Path(sys.argv[1]), Path(sys.argv[2])
with (root / 'benchmark-before-after.csv').open(newline='', encoding='utf-8-sig') as stream:
    rows = list(csv.DictReader(stream))
assert len(rows) == 224
text = ['# Stage03 — benchmark CPU înainte / după', '',
        '192 kHz, toate cele 16 ieșiri active în FIR, MSVC x64 / Intel IPP 2026. '
        'Steady: 10.000 măsurători/rând; tranziții: 200 adoptări/rând. '
        'QPC include scheduling; ciclurile sunt costul thread-ului plus wrapper, fără conversie în timp. '
        'Condițiile și limitele sunt în [raportul principal](VALIDATION-STAGE03.md).', '',
        '## Timp QPC în µs', '',
        '| Modul | Program | Configurație | Frames | Înainte median/p99/max µs | După median/p99/max µs | Buget µs | Înainte | După |',
        '| --- | --- | --- | --- | --- | --- | --- | --- | --- |']
def triple(row, phase, suffix):
    return ' / '.join(f"{float(row[phase + '_' + metric + '_' + suffix]):.2f}" for metric in ('median', 'p99', 'max'))
for row in rows:
    text.append(f"| {row['module']} | {'CINEMA' if row['program']=='1' else 'MUSIC'} | {row['configuration']} | {row['frames']} | {triple(row,'before','us')} | {triple(row,'after','us')} | {float(row['budget_us']):.2f} | {row['before_status']} | {row['after_status']} |")
text += ['', '## Cicluri CPU ale thread-ului', '',
         'Statisticile de cicluri sunt sortate separat. Maximum în cicluri nu este obligatoriu '
         'același callback cu maximum QPC. Perechile brute și `thread_cycles_at_max_wall` '
         'rămân în arhiva locală; overhead-ul API este raportat în logurile fiecărei execuții.', '',
         '| Modul | Program | Configurație | Frames | Înainte median/p99/max cycles | După median/p99/max cycles | Ratio median după/înainte |',
         '| --- | --- | --- | --- | --- | --- | --- |']
for row in rows:
    text.append(f"| {row['module']} | {'CINEMA' if row['program']=='1' else 'MUSIC'} | {row['configuration']} | {row['frames']} | {triple(row,'before','thread_cycles')} | {triple(row,'after','thread_cycles')} | {float(row['median_thread_cycles_ratio_after_before']):.4f} |")
with (root / 'fade-before-after.csv').open(newline='', encoding='utf-8-sig') as stream:
    fade = list(csv.DictReader(stream))
assert len(fade) == 80
text += ['', '## Fiecare callback în timpul fade-ului', '',
         '20 tranziții alternante per rând, 19.200 frames sau mai mult urmărite per tranziție. '
         'Include filtrul FIR, IIR/FIR, lungimea FIR, FIR/All-Pass și programul. '
         'Refill, control, validarea finite/active și scrierea rezultatelor sunt în afara intervalului măsurat.', '',
         '| Modul | Program | Configurație | Frames | Înainte median/p99/max µs | După median/p99/max µs | Buget µs | Înainte | După |',
         '| --- | --- | --- | --- | --- | --- | --- | --- | --- |']
for row in fade:
    text.append(f"| {row['module']} | {'CINEMA' if row['program']=='1' else 'MUSIC'} | {row['configuration']} | {row['frames']} | {triple(row,'before','us')} | {triple(row,'after','us')} | {float(row['budget_us']):.2f} | {row['before_status']} | {row['after_status']} |")
text += ['', '## Cicluri CPU în fade', '',
         '| Modul | Program | Configurație | Frames | Înainte median/p99/max cycles | După median/p99/max cycles | Ratio median după/înainte |',
         '| --- | --- | --- | --- | --- | --- | --- |']
for row in fade:
    text.append(f"| {row['module']} | {'CINEMA' if row['program']=='1' else 'MUSIC'} | {row['configuration']} | {row['frames']} | {triple(row,'before','thread_cycles')} | {triple(row,'after','thread_cycles')} | {float(row['median_thread_cycles_ratio_after_before']):.4f} |")
destination.write_text('\n'.join(text)+'\n', encoding='utf-8')
print('Rendered', len(rows), 'wall and CPU-cycle before/after comparisons:', destination)
