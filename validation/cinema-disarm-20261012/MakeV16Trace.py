import pathlib
p=pathlib.Path(__file__).parent
code=(p/'MakeV15Trace.py').read_text().replace('CinemaDecoder-v0.2.15','CinemaDecoder-v0.2.16').replace('decoder-observation-v15','decoder-observation-v16').replace('0.2.15-observe','0.2.16-observe')
exec(compile(code,'MakeV16Trace','exec'),{'__file__':str(p/'MakeV16Trace.py')})
