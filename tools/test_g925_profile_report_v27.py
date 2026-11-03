#!/usr/bin/env python3
"""Compile the actual Android report class with a BuildConfig-only host stub."""
from pathlib import Path
import shutil
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parents[1]
java = shutil.which('java')
javac = shutil.which('javac')
if not java:
    raise SystemExit('Java 17 is required; report behavioral tests were NOT run')
compiler = [javac] if javac else [java, '-m', 'jdk.compiler/com.sun.tools.javac.Main']
with tempfile.TemporaryDirectory(prefix='g925-report-') as tmp:
    path = Path(tmp)
    stub = path / 'BuildConfig.java'
    stub.write_text('package com.libertyrecomp; final class BuildConfig { static final String VERSION_NAME = "2.10-test"; }\n')
    output = path / 'classes'
    output.mkdir()
    fixture = path / 'fixture'
    fixture.mkdir()
    subprocess.run(compiler + ['--release', '17', '-d', str(output), str(stub),
                    str(ROOT / 'os/android/app/src/main/java/com/libertyrecomp/G925NativeProfileReport.java'),
                    str(ROOT / 'tools/fixtures/g925/G925ProfileReportTest.java')], check=True)
    subprocess.run([java, '-cp', str(output), 'com.libertyrecomp.G925ProfileReportTest', str(fixture)], check=True)
