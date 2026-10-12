import pathlib
import shutil

p = pathlib.Path(__file__).parent
out = p / 'decoder-observation-v18'
code = (p / 'MakeObservation.py').read_text().replace(
    'CinemaDecoder-v0.2.14', 'CinemaDecoder-v0.2.18').replace(
    "/'decoder-observation'", "/'decoder-observation-v18'")
exec(compile(code, 'MakeV18Observation', 'exec'), {'__file__': str(__file__)})
source = out / 'EntropySurroundDecoder-observe.cpp'
source.write_text(source.read_text().replace('ENTROPY_SURROUND_VERSION', '"0.2.18-observe"'))
build = (p / 'decoder-observation/Build.cmd').read_text().replace(
    'CinemaDecoder-v0.2.14', 'CinemaDecoder-v0.2.18').replace(
    'decoder-observation\\', 'decoder-observation-v18\\')
(out / 'Build.cmd').write_text(build)
code = (p / 'MakeTrace.py').read_text().replace('decoder-observation/', 'decoder-observation-v18/')
code = code.replace('262144', '524288')
code = code.replace(
    "rep('capture->GetBuffer(&data, &frames, &flags, nullptr, nullptr)', 'capture->GetBuffer(&data, &frames, &flags, &devicePosition, &deviceQpc)')",
    "rep('capture->GetBuffer(\\n                &data, &frames, &flags, nullptr, nullptr)', 'capture->GetBuffer(\\n                &data, &frames, &flags, &devicePosition, &deviceQpc)')")
code = code.replace("rep('ffmpeg.stop();\\n        if (workerError'", "rep('ffmpeg.stop();\\n        if(pacerSchedulingFailure'")
code = code.replace("        if (workerError''')", "        if(pacerSchedulingFailure''')")
exec(compile(code, 'MakeV18Trace', 'exec'), {'__file__': str(__file__)})
shutil.copy2('C:/Program Files/Entropy Audio/Surround Decoder/ffmpeg.exe', out / 'ffmpeg.exe')
