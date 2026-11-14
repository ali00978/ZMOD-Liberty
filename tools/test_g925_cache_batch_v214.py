#!/usr/bin/env python3
"""Exercise real native cache code and guard exact GPU lifetime integration."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SDK = ROOT / 'glue/rexglue-sdk-main'
arena = (SDK / 'src/graphics/gta4_native/frame_constant_arena.h').read_text()
lookup = (SDK / 'src/graphics/gta4_native/native_draw_lookup_cache.h').read_text()

insert = arena.split('InsertResult Insert(const Key& key, const Value& value) {', 1)[1]
insert = insert.split('\n  bool CanResetGeneration()', 1)[0]
assert 'if (Value* existing = Find(key))' not in insert
assert insert.count('const size_t hash = hasher_(key);') == 1
assert 'const uint64_t wanted = (generation_ << kTagBits) | Tag(hash);' in insert
assert 'if (stamp == wanted && equal_(buckets_[index].key, key))' in insert
assert 'if (!EnsureInsertionCapacity()) return {};' in insert
assert 'Bucket* grown = InsertWithoutGrowth(key, value);' in insert
assert '++size_;' in insert
assert 'FrameGenerationMap<FrameConstantIdentity, Reservation, FrameConstantIdentityHash>' in arena
assert 'ResetAfterCompletion(uint64_t completed_submission)' in arena

assert 'kWays = Capacity >= 2 ? 2 : 1' in lookup
assert 'entry.registry_revision == registry_revision && entry.key == key' in lookup
assert 'entry.epoch == epoch_' in lookup
assert 'if (++epoch_ == 0)' in lookup
assert 'return entry.image;' in lookup

compiler = os.environ.get('G925_TEST_CXX') or shutil.which('g++') or shutil.which('clang++')
if not compiler:
    raise SystemExit('A C++20 compiler is mandatory for native cache validation')
with tempfile.TemporaryDirectory(prefix='g925-cache-v214-') as tmp:
    exe = Path(tmp) / ('cache.exe' if os.name == 'nt' else 'cache')
    args = [compiler, '-std=c++20', '-O2', '-Wall', '-Wextra', '-Werror',
            '-I' + str(SDK/'src'),
            str(ROOT/'tools/test_g925_cache_batch_v214.cpp'), '-o', str(exe)]
    if os.environ.get('G925_TEST_SANITIZE') == '1':
        args[2:2] = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
    subprocess.run(args, check=True)
    subprocess.run([str(exe)], check=True, timeout=35)
print('PASS: production generation cache, single-hash inserts and GPU-fence lifetime replay')
