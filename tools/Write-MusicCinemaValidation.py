import json,pathlib,hashlib
root=pathlib.Path(__file__).resolve().parents[1];v=root/'validation/music-cinema-v2'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest().upper()
read=lambda p:json.loads(p.read_text(encoding='utf-8-sig'))
protected=read(v/'protected-baseline.json');assert all(sha(root/p['path'])==p['sha256'] for p in protected)
original=pathlib.Path('D:/Projects/3Stage-3/Audio-bloc-IPP-Intel/release');checked=0
for version in ['0.1.8','0.1.9']:
 for folder in (original/version).glob('EntropyAudioCore-*-win-x64'):
  for f in read(folder/'manifest.json')['files']:assert sha(folder/f['path'])==f['sha256'];checked+=1
music=pathlib.Path('D:/Projects/3Stage-3/Stage2/release/stage2-music-v0009/source/Transport/StageBusSDK/include/MusicStageBusHashes.h')
if not music.exists():music=next(p for p in pathlib.Path('D:/Projects/3Stage-3/Stage2').rglob('MusicStageBusHashes.h') if 'v0009' in str(p))
assert sha(music)==sha(root/'Transport/StageBusSDK/include/MusicStageBusHashes.h')
before=read(v/'upstream-before-64k.json');after=read(v/'upstream-after-64k.json')
assert before['sessionId']==after['sessionId'] and before['controlRevision']==after['controlRevision'] and after['blocksPublished']>before['blocksPublished']
for key in ('generation','lostFrames','overruns','underruns','faults'):assert before['sources'][0][key]==after['sources'][0][key]
report={'version':'0.1.8-v2','date':'2026-10-11','sourceBaseCommit':'2f3c45aa71cbf1bc352aece848060dc7523b0649','sourceBaseArchiveSha256':'AC20227D613AE12460789419EBA566CE0729070616F10F3BFA1632E9D6AABA81','compiler':'MSVC 19.51.36257.0 x64','ipp':'2026.0 static CPU dispatch; no global AVX2','qt':'6.11.2 MinGW13.1; UI only','native':{},'realMusic':{},'qtTests':read(v/'ui-tests.json'),'inheritedQtFailure':{'test':'EntropyStage05Ui','original018Core':'FAIL','candidateCore':'FAIL','assertionsUnmodified':True},'protectedSourceHashes':{'files':len(protected),'verdict':'PASS'},'originalReleaseManifests':{'files':checked,'verdict':'PASS'},'musicHashesUnchanged':'PASS','upstreamLifecycle':{'verdict':'PASS','sessionUnchanged':True,'controlRevisionUnchanged':True,'musicGenerationUnchanged':True,'counterIncrementsLostOverrunUnderrunFault':0,'blocksAdvanced':after['blocksPublished']-before['blocksPublished']},'physicalOutputOpened':False,'automaticArm':False,'realCinema':'PENDING','reported019ArmBehavior':'NOT DIAGNOSED; baseline018 selected by user','stage04Realtime':'FAIL inherited; no new physical qualification','v07':'BLOCKED inherited','analogAlignment':'UNMEASURED','review':'Author self-review only; user requested direct execution without delegation'}
for frames,variant in [(32768,'32k'),(65536,'64k')]:
 log=(v/('ctest-'+str(frames)+'.log')).read_text();assert '1 tests failed out of 51' in log and log.count('***Failed')==1 and 'EntropyStage04Offline' in log
 imports=(v/('core-imports-'+str(frames)+'.txt')).read_text();assert 'Qt' not in imports
 edge=(v/('final-edge-'+str(frames)+'.log')).read_text();assert '100% tests passed' in edge
 live=read(v/('live-32768-final' if frames==32768 else 'live-65536')/'live-input.json')
 report['native'][variant]={'total':51,'passed':50,'failed':['EntropyStage04Offline: unsafe PCM rejected before converter'],'coreSha256':sha(root/f'build/msvc-music-cinema-{frames}/EntropyAudioCore.exe'),'fifoCapacityFrames':frames,'targetFrames':4096,'qtImports':False,'probe':'PASS','graphDifferential':'PASS nonzero bit-exact Music3/Cinema6/8','rtAudit':'PASS no allocation/wait/lock/IO in audited ingress/Graph phase','finalEdgeSuite':'6/6 PASS including transition source loss, Infinity and inactive gap'}
 n=live['nativeSubscriber'];report['realMusic'][variant]={'seconds':live['seconds'],'graphBlocks':live['graphBlocks'],'nativeBlocks':n['blocks'],'peaks':[n['peakL'],n['peakR'],n['peakC']],'transportFailures':n['transportFailures'],'nonfiniteSamples':n['nonfiniteSamples'],'gaps':int(live['final']['music']['gaps']),'faults':int(live['final']['music']['faults']),'audioActive':live['audioActive'],'physicalArmed':False}
(root/'docs/VALIDATION-MUSIC-CINEMA-v2.json').write_text(json.dumps(report,indent=2));print(json.dumps({'protected':len(protected),'originalManifestFiles':checked,'native':'50/51 both variants','upstreamLifecycle':'PASS'}))
