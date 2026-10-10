from pathlib import Path
s = Path('Engine/Control/CoreRuntime.cpp').read_text(encoding='utf-8')
arm = s.split('else if(action=="arm") {', 1)[1].split('else if(action=="disarm")',1)[0]
assert 'scheduler.stop()' not in arm, 'FAIL: ARM stops Graph and resets source subscription'
assert 'scheduler.start()' not in arm, 'FAIL: ARM restarts Graph and DSP histories'
print('PASS: normal ARM does not stop/restart the graph')
