import csv, json, sys
from pathlib import Path
p=Path(sys.argv[1]); f=json.loads((p/'core-final.json').read_text(encoding='utf-8-sig'))['clock_qpc_frequency']
rows=list(csv.DictReader((p/'timing/music-read.csv').open()))
ticks=list(csv.DictReader((p/'timing/ticks.csv').open()))
waits=[(int(r['end'])-int(r['start']))*1000/f for r in rows if int(r['rc'])==0]
reads=[r for r in rows if int(r['rc'])>0]
spans=[(int(b['processed'])-int(a['processed']))*1000/f for a,b in zip(reads,reads[1:]) if int(a['processed']) and int(b['processed'])]
seen=False; gaps=[]
for t in ticks:
    if int(t['valid'])&1:seen=True
    elif seen:
        q=int(t['qpc']); before=[r for r in reads if int(r['processed']) and int(r['processed'])<q]
        after=[r for r in reads if int(r['processed'])>q]
        gaps.append({'time_qpc':q,'fill':int(t['music_fill']),'since_packet_ms':(q-int(before[-1]['processed']))*1000/f if before else None,'until_packet_ms':(int(after[0]['processed'])-q)*1000/f if after else None})
print(json.dumps({'no_data_read_ms':{'count':len(waits),'max':max(waits,default=0),'mean':sum(waits)/len(waits) if waits else 0},'processed_packet_interval_max_ms':max(spans,default=0),'zero_ticks_after_first_valid':gaps},indent=2))
