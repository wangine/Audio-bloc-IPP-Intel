import pathlib

p = pathlib.Path(__file__).parent
glitch_us = 257722423
capture_tid, io_tid = 6168, 8252
ready = {}
lines = []
max_dpc_us = 0
with (p / 'windows-audio-glitch-window.csv').open(errors='replace') as source:
    for line in source:
        fields = line.split(',')
        if len(fields) < 3:
            continue
        try:
            timestamp = int(fields[1].strip())
        except ValueError:
            continue
        if not glitch_us-30000 <= timestamp <= glitch_us+10000:
            continue
        kind = fields[0].strip()
        if kind == 'ReadyThread':
            tid = int(fields[5].strip())
            if tid in (capture_tid, io_tid):
                ready[tid] = timestamp
                lines.append(line.strip())
        elif kind == 'CSwitch':
            new_tid, old_tid = int(fields[3]), int(fields[9])
            if new_tid in (capture_tid, io_tid) or old_tid in (capture_tid, io_tid):
                delay = timestamp-ready[new_tid] if new_tid in ready else None
                lines.append(line.strip()+f' | newOwnerReadyDelayUs={delay}')
        elif kind in ('DPC', 'Interrupt'):
            duration = int(fields[2].strip())
            max_dpc_us = max(max_dpc_us, duration)
            if duration > 1000:
                lines.append(line.strip())
        elif 'Microsoft.Windows.Audio.KernelStreamingEndpoint/Glitch/' == kind:
            lines.append(line.strip())
print('UR23 KSE glitch time us', glitch_us, 'capture TID', capture_tid, 'internal I/O TID', io_tid)
print('DPC/ISR maximum duration in the 40ms window (us)', max_dpc_us)
for line in lines:
    print(line)
