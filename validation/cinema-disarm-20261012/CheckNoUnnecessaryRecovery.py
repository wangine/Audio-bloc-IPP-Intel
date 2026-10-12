"""Live regression for a carrier separately validated offline, not arbitrary input."""
import pathlib
import re
import sys

log = pathlib.Path(sys.argv[1]).read_text()
records = []
for line in log.splitlines():
    if 'captureDiscontinuities=' in line:
        records.append({k: int(v) for k,v in re.findall(r'(\w+)=(\d+)', line)})
assert records, 'No diagnostic counters in capture'
last = records[-1]
warnings = last['captureDiscontinuities']
failures = {k: last[k] for k in (
    'recoveries', 'pacerUnderflows', 'pacerGateLoss', 'cinemaDrops', 'decodedDiscarded')
    if last[k] != 0}
print('capture warnings', warnings, 'unexpected resets/losses', failures)
if warnings < 1:
    print('INCONCLUSIVE: repeatable warning was not exercised')
    sys.exit(2)
if failures:
    print('FAIL: validated continuous carrier caused recovery or loss')
    sys.exit(1)
print('PASS: reported capture warning did not reset or discard continuous carrier')
