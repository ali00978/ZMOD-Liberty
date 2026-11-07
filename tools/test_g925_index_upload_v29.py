#!/usr/bin/env python3
"""Execute production index conversion and generation-cache regressions."""
from pathlib import Path
import os, shutil, subprocess, tempfile
ROOT=Path(__file__).resolve().parents[1]
SDK=ROOT/'glue/rexglue-sdk-main'
src=(SDK/'src/graphics/gta4_native/graphics_system.cpp').read_text()
header=(SDK/'src/graphics/gta4_native/graphics_system.h').read_text()
part=src[src.index('bool Gta4NativeGraphicsSystem::RecordIndexedPrimitive('):]
part=part[:part.index('bool Gta4NativeGraphicsSystem::RecordClear(')]
assert part.count('UploadBufferResource(command.index_buffer,')==1
fallback=part.index('  if (!host_index_buffer) {')
assert fallback>part.index('NativeIndexConversion::kRestart')
assert fallback>part.index('NativeVertexCacheOptimizer::Get().Enqueue(')
assert part.index('UploadBufferResource(command.index_buffer,')>fallback
assert part.count('resources.converted_index_uploads.Find(key)')==2
assert part.count('resources.converted_index_uploads.Insert(key,')==2
assert part.count('WriteNativeConvertedIndices(host_indices, key,')==2
assert 'converted_index_uploads.CanResetGeneration()' in header
assert 'converted_index_uploads.ResetGeneration()' in header
assert 'const auto reset_recording_resources = MakeScopeExit(' in src
assert 'return NativeQuadIndexCount(quad_list_index_count);' in src
converted=(SDK/'src/graphics/gta4_native/native_index_conversion.h').read_text()
assert 'key.index_mask == UINT32_MAX' in converted
assert 'key.restart_index == UINT16_MAX' in converted
assert 'std::memcpy(destination.data(), source.data() + size_t(key.first) * element,' in converted
assert 'values such as 0xAAFFFFFF' in converted
assert 'std::memcpy(dst, src, 3 * element);' in converted
assert 'std::memcpy(dst + 3 * element, src, element);' in converted
assert 'std::memcpy(dst + 4 * element, src + 2 * element, 2 * element);' in converted
compiler=os.environ.get('G925_TEST_CXX') or shutil.which('g++') or shutil.which('clang++')
if not compiler: raise SystemExit('A C++20 host compiler is required')
with tempfile.TemporaryDirectory(prefix='g925-indices-') as tmp:
    exe=Path(tmp)/('indices.exe' if os.name=='nt' else 'indices')
    args=[compiler,'-std=c++20','-O2','-Wall','-Wextra','-Werror','-I'+str(SDK/'src'),
          str(ROOT/'tools/test_g925_index_upload_v29.cpp'),'-o',str(exe)]
    if os.environ.get('G925_TEST_SANITIZE')=='1':
        args[2:2]=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
    subprocess.run(args,check=True)
    subprocess.run([str(exe)],check=True)
print('PASS: lazy original upload follows final index selection; conversion memos reset with recording resources')
