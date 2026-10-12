"""Copy complete v0003 sources and compact evidence to a fresh Git directory."""
import pathlib, shutil, sys
source=pathlib.Path(sys.argv[1]).absolute()
repo=pathlib.Path(sys.argv[2]).absolute()
target=repo/'Integration/Music/Stage1-v0003'
if target.exists():raise RuntimeError('Existing source snapshot is preserved')
target.mkdir(parents=True)
for name in ('ABI','Engine','Providers','ThirdParty','UI','tools','tests','reference-runtime','docs','CMakeLists.txt','README.md'):
    p=source/name
    if p.is_dir():shutil.copytree(p,target/name,ignore=shutil.ignore_patterns('__pycache__'))
    else:shutil.copy2(p,target/name)
for p in sorted((source/'evidence').rglob('*')):
    if not p.is_file() or p.stat().st_size>1048576:continue
    rel=p.relative_to(source/'evidence')
    if any('diagnostic' in part or part.startswith('source-closure') for part in rel.parts[:-1]) or p.name=='msvc-environment.cmd':continue
    if p.suffix not in ('.json','.jsonl','.csv','.txt','.log'):continue
    dst=target/'evidence'/rel;dst.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,dst)
print(target)
