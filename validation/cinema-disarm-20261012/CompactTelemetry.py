"""Lossless JSONL delta encoding. Every reconstructed original byte is verified.

python CompactTelemetry.py encode original.jsonl original.jsonl.delta.gz
python CompactTelemetry.py decode original.jsonl.delta.gz restored.jsonl
Decode without an output path verifies SHA256 without writing a second copy.
"""
import gzip
import hashlib
import json
import pathlib
import sys

def difference(old, new):
    if type(old) is not type(new):
        return [1, new]
    if isinstance(new, dict):
        if list(old) != list(new):
            return [1, new]
        changes = {}
        for key, value in new.items():
            patch = difference(old[key], value)
            if patch is not None:
                changes[key] = patch
        return [0, changes] if changes else None
    return [1, new] if old != new else None

def apply(old, patch):
    if patch is None:
        return old
    if patch[0] == 1:
        return patch[1]
    for key, value in patch[1].items():
        old[key] = apply(old[key], value)
    return old

def serialize(value, newline):
    return (json.dumps(value) + newline).encode('utf-8')

def encode(source, output):
    with source.open('rb') as f:
        sha = hashlib.file_digest(f, 'sha256').hexdigest()
    with source.open('rb') as f:
        first = f.readline()
    newline = '\r\n' if first.endswith(b'\r\n') else '\n'
    header = dict(format='EntropyTelemetryDeltaV1', source=source.name,
                  source_sha256=sha, newline=newline)
    state = None
    rows = 0
    with gzip.open(output, 'wt', encoding='utf-8', newline='\n') as dest, source.open('rb') as f:
        dest.write(json.dumps(header) + '\n')
        for original in f:
            value = json.loads(original)
            patch = difference(state, value)
            state = apply(state, patch)
            assert serialize(state, newline) == original, ('Not byte-exact', rows)
            dest.write(json.dumps(patch, separators=(',', ':')) + '\n')
            rows += 1
    assert rows > 0
    result = decode(output)
    assert result['sha256'] == sha
    result.update(file=output.name, bytes=output.stat().st_size, rows=rows)
    return result

def decode(source, output=None):
    digest = hashlib.sha256()
    state = None
    rows = 0
    target = output.open('xb') if output else None
    try:
        with gzip.open(source, 'rt', encoding='utf-8') as f:
            header = json.loads(next(f))
            assert header['format'] == 'EntropyTelemetryDeltaV1'
            for line in f:
                state = apply(state, json.loads(line))
                raw = serialize(state, header['newline'])
                digest.update(raw)
                if target:
                    target.write(raw)
                rows += 1
        sha = digest.hexdigest()
        assert sha == header['source_sha256'], (sha, header['source_sha256'])
        return dict(source=header['source'], rows=rows, sha256=sha, byte_exact=True)
    finally:
        if target:
            target.close()

if __name__ == '__main__':
    mode, source = sys.argv[1], pathlib.Path(sys.argv[2])
    if mode == 'encode':
        result = encode(source, pathlib.Path(sys.argv[3]))
    elif mode == 'decode':
        result = decode(source, pathlib.Path(sys.argv[3]) if len(sys.argv) > 3 else None)
    else:
        raise ValueError(mode)
    print(json.dumps(result, indent=2))
