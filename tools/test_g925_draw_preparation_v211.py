#!/usr/bin/env python3
"""Replay production draw preparation against its former behavior, without a GPU."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SDK = ROOT / 'glue/rexglue-sdk-main'
source = (SDK / 'src/graphics/gta4_native/graphics_system.cpp').read_text()
for kind in ('Texture', 'Surface'):
    assert (f'StageNative{kind}FlightResources(const Native{kind}Image& image) {{\n'
            '  if (!IsNativeFlightRecorderEnabled()) return;') in source
mark = source.split('void Gta4NativeGraphicsSystem::MarkNativeSurfaceImageUsed(', 1)[1].split('\n}', 1)[0]
assert 'image.last_used_frame = std::max' in mark and 'image.last_used_submission = std::max' in mark
assert 'arena.storage = replacement;\n    arena.shared_allocation_memo.Reset();' in source
assert 'if (index_reset) arena.shared_allocation_memo.Reset();' in source
assert 'arena = NativeFrameConstantArena{};' in source
assert source.count('shared_allocation_memo.Store(') == 1
assert 'constant_arena.shared_allocation_memo.Find(*shared_identity)' in source
assert 'native_raster_area_memo_.Get(' in source
assert 'const VkRect2D& scissor = raster_area.scissor;' in source
assert 'const float viewport_scale_x = raster_area.scale_x;' in source
assert 'SelectNativeDrawBlendConstants(' in source
# Derived-state reuse must not bypass recording into a fresh command buffer.
draw = source.split('bool Gta4NativeGraphicsSystem::BindCommonDrawState(', 1)[1].split('\nnamespace {', 1)[0]
assert 'native_draw_state_cache_.UpdateViewport(' in draw
assert 'native_draw_state_cache_.UpdateScissor(' in draw
assert 'native_draw_state_cache_.Reset(); ubo_bound_layout_ = VK_NULL_HANDLE;' in draw

candidates = []
if os.environ.get('G925_VULKAN_HEADERS'):
    candidates.append(Path(os.environ['G925_VULKAN_HEADERS']))
for variable in ('ANDROID_NDK_HOME', 'ANDROID_NDK_ROOT'):
    if os.environ.get(variable):
        candidates.extend(Path(os.environ[variable]).glob('toolchains/llvm/prebuilt/*/sysroot/usr/include'))
if os.environ.get('ANDROID_HOME'):
    candidates.extend(Path(os.environ['ANDROID_HOME']).glob('ndk/*/toolchains/llvm/prebuilt/*/sysroot/usr/include'))
candidates.append(Path('/usr/include'))
headers = next((p for p in candidates if (p/'vulkan/vulkan_core.h').is_file()), None)
if headers is None:
    raise SystemExit('Vulkan headers required: set G925_VULKAN_HEADERS or install the Android NDK')
compiler = os.environ.get('G925_TEST_CXX') or shutil.which('g++') or shutil.which('clang++')
if not compiler:
    raise SystemExit('A C++20 host compiler is required')
with tempfile.TemporaryDirectory(prefix='g925-preparation-') as tmp:
    tmp = Path(tmp)
    # Use only API headers from the NDK, not Android's host-incompatible libc.
    include = tmp/'include'
    for name in ('vulkan', 'vk_video'):
        if (headers/name).is_dir(): shutil.copytree(headers/name, include/name)
    exe = tmp/('preparation.exe' if os.name == 'nt' else 'preparation')
    args = [compiler, '-std=c++20', '-O2', '-Wall', '-Wextra', '-Werror',
            '-I'+str(include), '-I'+str(SDK/'src'),
            str(ROOT/'tools/test_g925_draw_preparation_v211.cpp'), '-o', str(exe)]
    if os.environ.get('G925_TEST_SANITIZE') == '1':
        args[2:2] = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
    subprocess.run(args, check=True)
    subprocess.run([str(exe)], check=True)
print('PASS: production integration, allocation invalidation and resource tracking guards')
