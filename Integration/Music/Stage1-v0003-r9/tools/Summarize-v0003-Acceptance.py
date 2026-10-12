"""Summarize measured reports without promoting failed historical runs."""
import csv, hashlib, json, pathlib
import argparse
parser=argparse.ArgumentParser()
parser.add_argument('--revision',default='r3')
revision=parser.parse_args().revision
root=pathlib.Path(__file__).absolute().parent.parent
e=root/'evidence'
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest().upper()
csv_path=e/f'active-v0003-{revision}-dual-5min.csv'
rows=list(csv.DictReader(csv_path.open(encoding='utf-8-sig')))
cols=('music_underruns','aux_underruns','music_faults','aux_faults')
dual={'durationMs':int(rows[-1]['time_ms'])-int(rows[0]['time_ms']), 'samples':len(rows),
 'counterMax':{c:max(int(r[c]) for r in rows) for c in cols},
 'musicFramesDelta':int(rows[-1]['music_frames_in'])-int(rows[0]['music_frames_in']),
 'auxFramesDelta':int(rows[-1]['aux_frames_in'])-int(rows[0]['aux_frames_in']),
 'music':'TIDAL legacy.music-transport; original PCM not certified',
 'aux':'Spotify default render AUX / windows.capture.v2 Shared',
 'controlReadDelayMs':25, 'sourceStateValues':{c:sorted(set(int(r[c]) for r in rows)) for c in ('music_state','aux_state')},
 'rates':{c:sorted(set(int(r[c]) for r in rows)) for c in ('music_rate','aux_rate')},
 'zeroMetadataSnapshots':sum(int(r['music_rate'])==0 or int(r['aux_rate'])==0 for r in rows),
 'regressedFrameSnapshots':sum(int(rows[n][c])<int(rows[n-1][c]) for n in range(1,len(rows)) for c in ('music_frames_in','aux_frames_in')),
 'csvSha256':sha(csv_path)}
dual['pass']=dual['durationMs']>=299000 and all(v==0 for v in dual['counterMax'].values()) and all(v==[2] for v in dual['sourceStateValues'].values()) and not dual['zeroMetadataSnapshots'] and not dual['regressedFrameSnapshots'] and (e/f'active-v0003-{revision}-control-error.txt').stat().st_size==0
raw=read(e/f'{revision}-driver-0.3.0.11-all-format-raw-10s/results.json')
native_folder=e/f'{revision}-driver-0.3.0.11-native-60s'
native=read(native_folder/'results.json')
native_reports=[]
for case in native:
    folder=native_folder/f"192000-{case['case']['bits']}-event"
    native_reports.append({'bits':case['case']['bits'],'pass':case['pass'],'raw':read(folder/'comparison.json'),'fp64':read(folder/'output-comparison.json')})
aux=read(e/'aux-exclusive-mmcss-120s.jsonl')
result={'date':'2026-10-11','runtimeRevision':revision,'coreSha256':sha(root/f'release/stage1-native-music-v0003-{revision}/core/EntropyMusicSourceCore.exe'),
 'providerAbiSha256':sha(root/'ABI/endpoint_provider_v2.h'), 'dualLive':dual,
 'driverVersion':'0.3.0.11','driverInstalled':read(e/'installed-driver-0.3.0.11.json'),
 'rawMatrix':{'pass':len(raw)==12 and all(c['pass'] for c in raw),'cases':len(raw),'frames':sum(c['frames'] for c in raw),'rates':[44100,48000,88200,96000,176400,192000],'formats':['PCM16','PCM24 MSB valid in32'],'servicing':'WASAPI Exclusive Event-Driven'},
 'native192k':{'pass':all(c['pass'] for c in native),'frames':sum(c['frames'] for c in native),'cases':native_reports,'scope':'Exact normalized binary64, complete drain, zero interior invalid frames; start/stop invalid clock blocks reported separately'},
 'auxExclusiveMmcss':aux,
 'priorNative60s':{'frames':23040000,'scope':'r1 before control-only ACK fix; PASS reports preserved in music-final-60s'},
 'historicalFailures':['Frozen v0002 output temporal gaps and ambiguous state0 completion remain FAIL','Ordinary nonMMCSS AUX Exclusive: position gap1323 at13.37s, max read45.34ms','Shared baseline: unmarked353 frame gap after67.53s','Control delayed-read response discarded before ACK fix','Architecture synthetic test assumed metadata ready at50ms; bounded readiness regression corrected','Kernel0.3.0.10 rare16.1713ms passive read-finish stall produced interiorinvalid1024frames; generator maxcommit11.34ms, raw still exact; newkernel wakes on gate release'],
 'limitations':['No TIDAL raw Shared/Exclusive equivalence claim','Legacy MUSIC raw identity not certified','V2 polling unsupported','Short finite live checks do not guarantee all host load conditions'],
 'review':'No Critical/Important findings in read-only closure review; live qualification assessed by measured reports'}
if not (dual['pass'] and result['rawMatrix']['pass'] and result['native192k']['pass'] and not aux['failed'] and aux['position_gaps']==0):raise RuntimeError('Acceptance report contains failed current case')
(e/'V0003-ACCEPTANCE.json').write_text(json.dumps(result,indent=2,ensure_ascii=False)+'\n',encoding='utf-8')
print(json.dumps({'dual':dual,'rawFrames':result['rawMatrix']['frames'],'nativeFrames':result['native192k']['frames'],'auxExclusive':aux},indent=2))
