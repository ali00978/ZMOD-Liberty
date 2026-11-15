#!/usr/bin/env python3
"""Run cache/lifetime behavior and check every production invalidation site."""
from pathlib import Path
import os, re, shutil, subprocess, tempfile
ROOT=Path(__file__).resolve().parents[1]
SDK=ROOT/'glue/rexglue-sdk-main'
src=(SDK/'src/graphics/gta4_native/graphics_system.cpp').read_text()
for mutation, invalidation in (
    (r'virtual_resource_registry_\.(?:Register\(registration\)|Erase\(release.resource\)|Clear\(\));',
     'virtual_surface_registry_revision_.fetch_add(1, std::memory_order_release);'),
    (r'(?:reflection_resources_\.clear\(\)|RegisterNativeReflectionTarget\(reflection_resources_, target\)|EraseNativeReflectionRegistration\(reflection_resources_, release.resource\));',
     'surface_lookup_cache_.Invalidate();')):
    sites=list(re.finditer(mutation, src))
    assert sites
    for site in sites:
        assert src[site.end():].lstrip().startswith(invalidation)
for name, arg in [('DestroyNativeSurfaceImage','NativeSurfaceImage& image'),
                  ('QueueSurfaceImageRelease','uint32_t handle')]:
    assert f'{name}({arg}) {{\n  surface_lookup_cache_.Invalidate();' in src
hit=src[src.index('surface_lookup_cache_.Find('):]
hit=hit[:hit.index('return image;')]
assert 'MarkNativeSurfaceImageUsed(*image);' in hit
assert src.count('surface_lookup_cache_.Store(lookup_key, registry_revision,') == 2
assert 'required_streams[stream]' not in src
assert 'std::array<bool, kVertexStreamCount> required_streams' not in src
compiler=os.environ.get('G925_TEST_CXX') or shutil.which('g++') or shutil.which('clang++')
if not compiler: raise SystemExit('A C++20 host compiler is required')
with tempfile.TemporaryDirectory(prefix='g925-lookup-') as tmp:
    exe=Path(tmp)/('lookup.exe' if os.name=='nt' else 'lookup')
    args=[compiler,'-std=c++20','-O2','-Wall','-Wextra','-Werror','-I'+str(SDK/'src'),
          str(ROOT/'tools/test_g925_draw_lookup_v28.cpp'),
          str(SDK/'src/graphics/gta4_native/native_performance_samples.cpp'),'-o',str(exe)]
    if os.environ.get('G925_TEST_SANITIZE')=='1':
        args[2:2]=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
    subprocess.run(args,check=True)
    subprocess.run([str(exe)],check=True)
print('PASS: registry/reflection/release/destruction integration and preserved submission tracking')
