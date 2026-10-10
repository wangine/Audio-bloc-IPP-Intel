"""Source identity against stable 0.1.5; no audio or timed qualification."""
import hashlib, json, pathlib, re, subprocess, sys
root = pathlib.Path(__file__).resolve().parents[1]
base = '3afb8d77b3bdd984a2f5bb53095cb99c37de72c0'
def original(path):
    return subprocess.check_output(['git', '-C', str(root), 'show', base + ':' + path])
def methods(text):
    text = text.replace('\r\n', '\n')
    result = {}
    for match in re.finditer(r'EntropyCrossoverAudioProcessor::(\w+)\s*\(', text):
        start = text.find('{', match.end()); depth = 0; index = start; quote = None
        while index < len(text):
            char = text[index]
            if quote:
                if char == '\\': index += 2; continue
                if char == quote: quote = None
            elif text.startswith('//', index): index = text.find('\n', index); continue
            elif text.startswith('/*', index): index = text.find('*/', index + 2) + 2; continue
            elif char in '\"\'': quote = char
            elif char == '{': depth += 1
            elif char == '}':
                depth -= 1
                if depth == 0: result[match.group(1)] = text[start:index + 1]; break
            index += 1
    return result
path = 'DSP/Crossover/src/PluginProcessor.cpp'
old = methods(original(path).decode()); current = methods((root / path).read_text(encoding='utf-8'))
changed = [name for name, body in old.items() if current.get(name) != body]
assert changed == ['publishControlSnapshot'], changed
protected = []
for path in subprocess.check_output(['git', '-C', str(root), 'ls-files'], text=True).splitlines():
    if (path.startswith('DSP/') and path not in ['DSP/Crossover/src/PluginProcessor.cpp', 'DSP/Crossover/include/PluginProcessor.h']) or path.startswith(('Egress/', 'UI/Legacy/')) or path in ['Engine/Egress/NativeEgress.cpp', 'Engine/AudioScheduler.cpp', 'Engine/Input/Stage2Ingress.cpp']:
        before = original(path); after = (root / path).read_bytes()
        assert before.replace(b'\r\n', b'\n') == after.replace(b'\r\n', b'\n'), path
        protected.append({'path': path, 'sha256': hashlib.sha256(after).hexdigest().upper(), 'unchangedFromBaseline': True})
def qualification_body(text):
    text = text.replace('\r\n', '\n'); start = text.index('ipc::Value liveProfilePreflight(')
    text = text[text.index('{', start):]; depth = 0
    for index, char in enumerate(text):
        if char == '{': depth += 1
        elif char == '}':
            depth -= 1
            if depth == 0: return text[:index + 1]
diagnostic = (root / 'Engine/Control/LiveQualification.cpp').read_text(encoding='utf-8').replace(' recordDiagnosticBenchmarkCall();\n', '')
assert qualification_body(original('Engine/Control/LiveEligibility.cpp').decode()) == qualification_body(diagnostic)
result = {'PASS': True, 'baseline': base, 'crossoverOriginalMethodsChecked': len(old), 'changedControlPreparationOnly': changed, 'strictDiagnosticBodyUnchanged': True, 'protectedFiles': protected}
if len(sys.argv) > 1: pathlib.Path(sys.argv[1]).write_text(json.dumps(result, indent=2), encoding='utf-8')
print(json.dumps({'PASS': True, 'originalCrossoverMethods': len(old), 'unchangedProtectedFiles': len(protected), 'strictQualificationUnchanged': True}))
