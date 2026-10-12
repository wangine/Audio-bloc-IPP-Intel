"""Offline only: preserve every captured byte and inspect flagged WASAPI packets."""
import collections
import hashlib
import pathlib
import struct
import sys
import numpy as np

trace = pathlib.Path(sys.argv[1])
raw_path = pathlib.Path(sys.argv[2])
rows = np.genfromtxt(trace, delimiter=',', names=True, dtype=np.uint64)
packets = rows[rows['kind'] == 6]
raw = raw_path.read_bytes()
print('RAW bytes', len(raw), 'SHA256', hashlib.sha256(raw).hexdigest())
print('captured packets', len(packets), 'packet flags', np.unique(packets['c'], return_counts=True))
print('offset continuity errors', np.count_nonzero(
    packets['a'][1:] != packets['a'][:-1] + packets['b'][:-1]))
assert len(raw) == int(packets[-1]['a'] + packets[-1]['b'])

headers = []
offset = 0
while True:
    offset = raw.find(b'\x72\xf8\x1f\x4e', offset)
    if offset < 0:
        break
    if offset + 8 <= len(raw):
        pc, bits = struct.unpack_from('<HH', raw, offset + 4)
        if pc & 31 in (1, 11, 12, 13):
            headers.append((offset, pc & 31, bits))
    offset += 1
print('IEC burst counts', collections.Counter(h[1] for h in headers))
print('IEC preamble byte alignment mod 4', collections.Counter(h[0] % 4 for h in headers))
print('IEC header spacing bytes', collections.Counter(b[0]-a[0] for a,b in zip(headers,headers[1:])))

for i in np.flatnonzero(packets['c'] & 1):
    packet = packets[i]
    offset, size = int(packet['a']), int(packet['b'])
    data = raw[offset:offset+size]
    print('\nFLAGGED packet', int(i), 'offset', offset, 'device frame', int(packet['d']),
          'flags', int(packet['c']), 'nonzero bytes', sum(v != 0 for v in data),
          'SHA256', hashlib.sha256(data).hexdigest())
    for j in range(max(0,int(i)-3), min(len(packets),int(i)+4)):
        near = packets[j]
        begin, count = int(near['a']), int(near['b'])
        block = raw[begin:begin+count]
        print('neighbor', j, 'same bytes', block == data,
              'offset', begin, 'frame', int(near['d']), 'flags', int(near['c']),
              'source qpc', int(near['e']), 'pointer', hex(int(near['f'])),
              'hash', hashlib.sha256(block).hexdigest()[:16])
    near_headers = [h for h in headers if offset-14000 <= h[0] <= offset+14000]
    for j,h in enumerate(near_headers):
        length = (h[2]+7)//8
        payload = raw[h[0]+8:h[0]+8+length]
        print('near IEC header', h,
              'spacing', h[0]-near_headers[j-1][0] if j else None,
              'payload hash', hashlib.sha256(payload).hexdigest()[:24])
print('\nGetBuffer results', collections.Counter(hex(int(r['b'])) for r in rows if r['kind'] == 7))
print('ReleaseBuffer results', collections.Counter(hex(int(r['a'])) for r in rows if r['kind'] == 8))
