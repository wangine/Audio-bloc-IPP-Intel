import json,pathlib
root=pathlib.Path(__file__).absolute().parents[1]/'evidence'
def read(path):return json.loads(path.read_text(encoding='utf-8-sig'))
coverage=read(root/'R6-CONTINUITY-COVERAGE.json')
assert len(coverage)==14
coverageByCase={c['case']:c['result'] for c in coverage}
def summarize(name,expected,fp64):
    folder=root/name
    results=read(folder/'results.json')
    assert len(results)==expected and all(c['pass'] for c in results)
    cases=[]
    for c in results:
        case=c['case'];d=folder/f"{case['rate']}-{case['bits']}-event"
        raw=read(d/'comparison.json');final=read(d/'core-final.json');render=read(d/'render.json')
        s=final['sources'][0]
        assert raw['pass'] and raw['mismatchedBytes']==0 and raw['lostFrames']==0
        assert all(s[k]==0 for k in ('overruns','faults','lost_frames'))
        assert final['publisher_drops']==0 and final['clock_late_blocks']==0
        assert render['drained'] and render['emittedFrames']==raw['frames']
        checked=coverageByCase[name+'/'+d.name]
        assert checked['underrunsBeforeLastPacket']==0 and checked['verifiedFrames']==raw['frames']
        result=dict(case,frames=raw['frames'],rawIdentical=True,sourceCounters=checked,pause=render['pause'])
        if fp64:
            assert c['zeroSourceCountersRequired'] and s['underruns']==0
            out=read(d/'output-comparison.json')
            assert out['pass'] and out['exactBinary64'] and out['mismatchedSamples']==0 and out['interiorInvalidFrames']==0 and out['footer']
            result.update(exactBinary64=True,invalidBoundaryFrames=out['invalidClockFrames'],interiorInvalidFrames=out['interiorInvalidFrames'])
        else:
            p=render['pause']
            assert p['observed'] and p['clockBefore']==p['clockAfter'] and p['sourceBefore']==p['sourceAfter'] and p['generationBefore']==p['generationAfter']
        cases.append(result)
    return {'scope':name,'cases':cases,'frames':sum(c['frames'] for c in cases),'provenance':read(folder/'provenance.json')}
result={'version':'stage1-native-music-v0003-r6','native192k':summarize('idle-r6-native192k-20261011',2,True),
 'allRatesPause':summarize('idle-r6-all-rates-pause-20261011',12,False),
 'limits':['Known test PCM; not a reference capture of original TIDAL PCM','Native 192k FP64 equality only; SRC at other rates changes sampling grid','Finite probes, not long stress qualification','Kernel packet mode0 remains unverified Shared/Exclusive access','Earlier r4 raw PASS does not qualify its false startup underrun counters','AUX format/stereo investigation remains unresolved']}
(root/'IDLE-R6-RESULT.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
print(json.dumps({'version':result['version'],'nativeFrames':result['native192k']['frames'],'pauseFrames':result['allRatesPause']['frames'],'cases':14,'pass':True},indent=2))
