#!/usr/bin/env python3
"""Exercise actual publication gate and pipeline, plus production ordering."""
from pathlib import Path
import os, shutil, subprocess, tempfile
ROOT = Path(__file__).resolve().parents[1]
SDK = ROOT/'glue/rexglue-sdk-main'
src = (SDK/'src/ui/presenter.cpp').read_text()
body = src.split('bool Presenter::RefreshGuestOutput(', 1)[1].split('\nvoid Presenter::', 1)[0]
assert body.index('RefreshGuestOutputImpl(') < body.index('frame_publication_pipeline_.BeforePublish(')
assert body.index('frame_publication_pipeline_.BeforePublish(') < body.index('frame_publication_gate_.Publish(')
assert body.index('frame_publication_gate_.Publish(') < body.index('compare_exchange_weak(')
assert body.index('compare_exchange_weak(') < body.index('frame_publication_pipeline_.AfterPublish(')
assert body.count('frame_publication_gate_.Wait(') == 1
assert 'producer_wait_observer(true)' in body and 'producer_wait_observer(false)' in body
wait = body.split('const auto wait_for_publication =', 1)[1].split('\n  };', 1)[0]
assert 'lock_guard' not in wait and 'unique_lock' not in wait
native = (SDK/'src/graphics/gta4_native/graphics_system.cpp').read_text()
assert '#if REX_PLATFORM_ANDROID\n          .producer_render_ahead = true,\n#endif' in native
assert 'producer_wait_scope.emplace(profile::CpuOp::kPresenterWait)' in native
compiler = os.environ.get('G925_TEST_CXX') or shutil.which('g++') or shutil.which('clang++')
if not compiler: raise SystemExit('C++20 compiler required')
with tempfile.TemporaryDirectory(prefix='g925-present-') as tmp:
    exe = Path(tmp)/('test.exe' if os.name == 'nt' else 'test')
    args = [compiler, '-std=c++20', '-O2', '-Wall', '-Wextra', '-Werror',
            '-I'+str(SDK/'include'), '-I'+str(SDK/'src'),
            str(ROOT/'tools/test_g925_present_overlap_v212.cpp'), '-o', str(exe)]
    if os.name != 'nt': args.append('-pthread')
    if os.environ.get('G925_TEST_SANITIZE') == '1':
        args[2:2] = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
    subprocess.run(args, check=True)
    subprocess.run([str(exe)], check=True, timeout=10)
print('PASS: render-before-wait-before-publish ordering and Android-only opt-in')
