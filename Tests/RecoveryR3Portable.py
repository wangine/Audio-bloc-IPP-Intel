"""Focused R3 observer/ratio checks. No Windows/MSVC/driver or audio-quality verdict.
Not part of the owner's Codex build-only task. Installs/downloads nothing.
"""
import argparse
import pathlib
import shutil
import subprocess
import tempfile

BASE = '2cd916e30cd0f3a6253e429c68e9208f76cd1942'
ROOT = pathlib.Path(__file__).resolve().parents[1]
CLOCK = pathlib.Path('Egress/ClockSync')
CONFIG_FIXTURE = '''#pragma once
#include <stdint.h>
// Exact config fields only; this fallback is NOT a full module/Windows ABI check.
typedef struct EntropyEgressClockConfig {
 uint32_t struct_size,revision,engine_rate,sink_rate,channels;
 uint32_t maximum_input_frames,maximum_render_frames,fifo_capacity_frames,target_fill_frames;
 double maximum_correction_ppm,correction_smoothing_ms;
 uint64_t reserved[6];
} EntropyEgressClockConfig;
'''

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--compiler', default='g++')
    p.add_argument('--baseline-directory', type=pathlib.Path)
    p.add_argument('--config-only-fixture', action='store_true', help='Explicit partial-checkout fallback, not ABI qualification')
    p.add_argument('--check-baseline', action='store_true')
    p.add_argument('--sanitize', action='store_true')
    args = p.parse_args()
    cc = shutil.which(args.compiler)
    if not cc:
        p.error('C++20 g++/clang++ compiler not found; no dependencies installed')
    def original(rel):
        if args.baseline_directory:
            return (args.baseline_directory / rel).read_text(encoding='utf-8')
        return subprocess.check_output(['git', 'show', f'{BASE}:{rel.as_posix()}'], cwd=ROOT).decode('utf-8')
    with tempfile.TemporaryDirectory(prefix='entropy-r3-') as td:
        tmp = pathlib.Path(td)
        rh = original(CLOCK / 'include/EgressRatioController.h')
        rc = original(CLOCK / 'src/EgressRatioController.cpp')
        (tmp / 'R2Ratio.h').write_text(rh.replace('RatioDecision', 'R2RatioDecision').replace('RatioController', 'R2RatioController'))
        (tmp / 'R2Ratio.cpp').write_text(rc.replace('#include "EgressRatioController.h"', '#include "R2Ratio.h"').replace('RatioDecision', 'R2RatioDecision').replace('RatioController', 'R2RatioController'))
        includes = [ROOT / CLOCK / 'include', tmp]
        if args.config_only_fixture:
            (tmp / 'entropy').mkdir()
            (tmp / 'entropy/egress_clock_abi.h').write_text(CONFIG_FIXTURE)
            print('CONFIG-ONLY FIXTURE: full ABI and Windows build NOT checked', flush=True)
        else:
            includes.append(ROOT / 'Shared/include')
        flags = ['-std=c++20', '-O2', '-ffp-contract=off', '-pthread', '-Wall', '-Wextra', '-Werror']
        if args.sanitize:
            flags += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        def run(name, units, incs=includes, expected=0):
            exe = tmp / name
            extra = ['-Wno-error=unused-but-set-variable'] if expected else []
            cmd = [cc, *flags, *extra, *[f'-I{i}' for i in incs], *map(str, units), '-o', str(exe)]
            subprocess.run(cmd, check=True)
            out = subprocess.run([str(exe)], text=True, capture_output=True)
            print(out.stdout, end='', flush=True)
            if out.stderr:
                print(out.stderr, end='', flush=True)
            if (expected == 0 and out.returncode != 0) or (expected != 0 and out.returncode != expected):
                raise RuntimeError(f'{name}: exit {out.returncode}, expected {expected}')
            if expected and ('failures=3' not in out.stdout or 'peak_error_ppm=12' not in out.stdout):
                raise RuntimeError('Baseline did not fail the intended three phase-noise checks')
        est = ROOT / CLOCK / 'src/EgressClockEstimator.cpp'
        ratio = ROOT / CLOCK / 'src/EgressRatioController.cpp'
        if args.check_baseline:
            old = tmp / 'baseline'; old.mkdir()
            for name in ('EgressClockEstimator.h', 'EgressClockEstimator.cpp'):
                rel = CLOCK / ('include' if name.endswith('.h') else 'src') / name
                (old / name).write_text(original(rel))
            run('old-observer', [old/'EgressClockEstimator.cpp', ROOT/'Tests/RecoveryR3ClockTests.cpp'], [old], expected=1)
        run('clock', [est, ROOT/'Tests/RecoveryR3ClockTests.cpp'])
        run('observer', [est, ROOT/'Tests/RecoveryR3ObserverTests.cpp'])
        run('ratio', [ratio, tmp/'R2Ratio.cpp', ROOT/'Tests/RecoveryR3RatioTests.cpp'])
        # Confirm passive instrumentation did not rewrite the original ratio law.
        actual = ratio.read_text().replace('RatioController::resetImpl()', 'RatioController::reset()').replace('RatioController::updateImpl(', 'RatioController::update(')
        if actual != rc.replace('\r\n', '\n'):
            raise RuntimeError('Numeric ratio implementation differs from R2 beyond wrapper method names')
        print('PASS focused observer/ratio checks; NOT a Windows/live/audio-quality qualification', flush=True)

if __name__ == '__main__':
    main()
