#!/usr/bin/env python3
"""G925 2.25: compile exact restart-conversion regression and validate production path."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SDK = ROOT / 'glue/rexglue-sdk-main'
header = (SDK / 'src/graphics/gta4_native/native_index_conversion.h').read_text()
assert 'std::memcpy(destination.data(), source.data() + size_t(key.first) * element,' in header
assert 'if ((value & key.index_mask) == key.restart_index)' in header
assert 'if (value == key.restart_index)' in header
assert header.count('std::memcpy(dst + size_t(i) * sizeof(value), &value, sizeof(value));') == 2
# The masked/restart fallback must be a single decode/write traversal, while
# the full-width all-ones restart sentinel stays on the optimized memcpy path.
tail = header.split('  } else {\n    // Masked restart conversion', 1)[1]
assert 'std::memcpy(dst, src, size_t(key.count) * element)' not in tail
assert tail.count('for (uint32_t i = 0; i < key.count; ++i)') == 2

compiler = os.environ.get('G925_TEST_CXX') or shutil.which('clang++') or shutil.which('g++')
if not compiler:
    raise SystemExit('A C++20 compiler is required')
with tempfile.TemporaryDirectory(prefix='g925-index-singlepass-') as tmp:
    exe = Path(tmp) / ('test.exe' if os.name == 'nt' else 'test')
    args = [compiler, '-std=c++20', '-O2', '-Wall', '-Wextra', '-Werror',
            '-I' + str(SDK / 'src'),
            str(ROOT / 'tools/test_g925_single_pass_index_v225.cpp'),
            '-o', str(exe)]
    if os.environ.get('G925_TEST_SANITIZE') == '1':
        args[2:2] = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
    subprocess.run(args, check=True)
    subprocess.run([str(exe)], check=True, timeout=45)
print('PASS: masked index restart conversion uses exactly one read/write pass')
