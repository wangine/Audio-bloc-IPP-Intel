import pathlib,re,sys
lines=pathlib.Path(sys.argv[1]).read_text().splitlines()
rows=[]
for line in lines:
 if 'pacerUnderflows=' not in line:continue
 rows.append({key:int(value) for key,value in re.findall(r'([A-Za-z]+)=(\d+)',line)})
assert len(rows)>=2,'insufficient continuous live observation'
first,last=rows[0],rows[-1]
keys=['captureDiscontinuities','recoveries','pacerUnderflows','pacerGateLoss','cinemaDrops','decodedDiscarded']
deltas={k:last[k]-first[k] for k in keys}
print('continuous records',len(rows),'deltas',deltas)
ok=all(x==0 for x in deltas.values())
print(('PASS' if ok else 'FAIL')+' continuous CINEMA capture/decode/publication')
sys.exit(0 if ok else 1)
