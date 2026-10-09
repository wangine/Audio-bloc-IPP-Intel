"""Report exact protected source/native dependency integrity for passive integration."""
import argparse, hashlib, json, pathlib, subprocess


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--source-original', required=True)
    parser.add_argument('--stage2-original', required=True)
    parser.add_argument('--source-runtime', required=True)
    parser.add_argument('--stage2-runtime', required=True)
    parser.add_argument('--output', required=True)
    args=parser.parse_args()
    root=pathlib.Path(__file__).resolve().parents[1]
    source_original=pathlib.Path(args.source_original)
    source_new=root/'Integration/Stage1/source'
    allowed={
        'CMakeLists.txt',
        'Host/EntropySourceEngine/CMakeLists.txt',
        'Host/EntropySourceEngine/Core/SourceController.cpp',
        'Host/EntropySourceEngine/Core/SourceController.h',
        'Host/EntropySourceEngine/App/SourceMain.cpp',
    }
    unchanged, changed, errors=[], [], []
    for path in source_original.rglob('*'):
        if not path.is_file():
            continue
        relative=path.relative_to(source_original).as_posix()
        target=source_new/relative
        same=target.exists() and digest(path)==digest(target)
        (unchanged if same else changed).append(relative)
        if not same and relative not in allowed:
            errors.append('Protected Stage1 source changed: '+relative)
    stage2_original=pathlib.Path(args.stage2_original)
    native_files=['TransparentDspExecutor.cpp','TransparentDspExecutor.h',
                  'DspCenterModule.cpp','DspCenterModule.h','DspEqModule.cpp','DspEqModule.h',
                  'DspLrcModule.cpp','DspLrcModule.h','DspAlignment.cpp','DspAlignment.h']
    stage2_rows=[]
    for name in native_files:
        relative=pathlib.Path('Host/EntropyDspEngine/Core')/name
        old=stage2_original/relative
        new=root/'Integration/Stage2/source'/relative
        equal=old.exists() and new.exists() and digest(old)==digest(new)
        stage2_rows.append({'path':relative.as_posix(), 'identical':equal,
                            'sha256':digest(new) if new.exists() else None})
        if not equal:
            errors.append('Protected Stage2 DSP source changed: '+name)
    native_rows=[]
    for stage, runtime in [('Stage1',pathlib.Path(args.source_runtime)),('Stage2',pathlib.Path(args.stage2_runtime))]:
        manifest=json.loads((runtime/'package-manifest.json').read_text(encoding='utf-8-sig'))
        for item in manifest['native_modules']:
            actual=digest(runtime/'modules'/item['name'])
            equal=actual==item['sha256'].lower()
            native_rows.append({'stage':stage,'name':item['name'],'sha256':actual,'identical':equal})
            if not equal:
                errors.append('Native dependency changed: '+item['name'])
    protected=subprocess.check_output(['git','diff','--name-only','7222030a9473a8e141d55313f785c06c5a336553',
                                       '--','DSP','Egress','Engine','UI','IPC','Devices'],cwd=root,text=True).splitlines()
    if protected:
        errors.append('Protected Core source changed: '+', '.join(protected))
    result={'status':'FAIL' if errors else 'PASS','baseline':'7222030a9473a8e141d55313f785c06c5a336553',
            'stage1UnchangedCount':len(unchanged),'stage1ChangedIntegrationFiles':changed,
            'stage1ProtectedFiles':unchanged,'stage2ProtectedDsp':stage2_rows,'nativeModules':native_rows,
            'coreDspUiEgressChanges':protected,'errors':errors}
    pathlib.Path(args.output).write_text(json.dumps(result,indent=2),encoding='utf-8')
    print(result['status'], 'Stage1 protected',len(unchanged),'Stage2 DSP',len(stage2_rows),'native DLLs',len(native_rows))
    if errors:
        raise AssertionError('\n'.join(errors))


if __name__=='__main__':
    main()
