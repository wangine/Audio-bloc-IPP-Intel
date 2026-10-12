import csv
import pathlib
import sys

# QPC frequency measured for this host is 10 MHz. This check measures
# ownership of the first acquired WASAPI buffer; it does not excuse flags
# or discard startup samples from the audio comparison.
with pathlib.Path(sys.argv[1]).open(newline='') as capture:
    first = next(row for row in csv.DictReader(capture) if row['kind'] == '1')
held_ms = (int(first['qpc']) - int(first['a'])) / 10_000
period_ms = float(sys.argv[2]) if len(sys.argv) > 2 else 10.0
print(f'first capture buffer held {held_ms:.4f} ms; endpoint period {period_ms:.4f} ms')
ok = held_ms < period_ms
print(('PASS' if ok else 'FAIL') + ' cold startup capture deadline')
sys.exit(0 if ok else 1)
