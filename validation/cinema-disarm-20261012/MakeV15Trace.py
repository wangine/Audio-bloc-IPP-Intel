import pathlib
p=pathlib.Path(__file__).parent
code=(p/'MakeObservation.py').read_text().replace('CinemaDecoder-v0.2.14','CinemaDecoder-v0.2.15').replace("/'decoder-observation'","/'decoder-observation-v15'")
exec(compile(code,'MakeV15Observation','exec'),{'__file__':str(p/'MakeV15Trace.py')})
out=p/'decoder-observation-v15'
f=out/'EntropySurroundDecoder-observe.cpp'
f.write_text(f.read_text().replace('ENTROPY_SURROUND_VERSION','"0.2.15-observe"'))
build=(p/'decoder-observation/Build.cmd').read_text().replace('CinemaDecoder-v0.2.14','CinemaDecoder-v0.2.15').replace('decoder-observation\\','decoder-observation-v15\\')
(out/'Build.cmd').write_text(build)
code=(p/'MakeTrace.py').read_text().replace('decoder-observation/','decoder-observation-v15/')
code=code.replace("rep('ffmpeg.stop();\\n        if (workerError'", "rep('ffmpeg.stop();\\n        if(pacerSchedulingFailure'")
code=code.replace('        if (workerError\'\'\')','        if(pacerSchedulingFailure\'\'\')')
exec(compile(code,'MakeV15Trace','exec'),{'__file__':str(p/'MakeV15Trace.py')})
