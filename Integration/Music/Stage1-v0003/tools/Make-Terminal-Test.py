from pathlib import Path
root=Path(__file__).absolute().parent.parent
source=root.parent/'Audio-bloc-IPP-Intel/Drivers/EntropyMusicRenderV2/tests/OutputBoundaryTests.py'
text=source.read_text(encoding='utf-8-sig')
text=text.replace("kind in ('clean','extra')", "kind in ('clean','extra','clean-retire')")
text=text.replace("kind=='clean' else 2", "kind in ('clean','clean-retire') else 2")
text=text.replace('test_ambiguous_normal_stop_is_not_certification','test_durable_normal_stop_and_final_drain')
destination=root/'tests/OutputTerminalTests.py'
if destination.exists():raise SystemExit('Preserve the permanent expanded terminal regression; generator is historical bootstrap only')
destination.write_text(text,encoding='utf-8')
