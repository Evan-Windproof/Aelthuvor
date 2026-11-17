#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Copy the complete, built Application SDK; preserve mtimes of equal files."""
from pathlib import Path
import shutil
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/app-sdk'
expected={OUT/'.stamp'}
def copy(source,target):
    expected.add(target)
    target.parent.mkdir(parents=True,exist_ok=True)
    if not target.exists() or target.read_bytes()!=source.read_bytes(): shutil.copy2(source,target)
for directory in ('include/aelthuvor','sdk/include/abi'):
    for file in (ROOT/directory).glob('*.h'):
        copy(file,OUT/'include'/('aelthuvor' if directory.startswith('include/') else 'abi')/file.name)
for name in ('libaelthuvor.a','libaeldevices.a'):
    copy(ROOT/'build'/name,OUT/'lib'/name)
for source,target in [('build/startup.o','lib/crt0.o'),('build/src/app_entry.o','lib/app-entry.o'),
                       ('linker.ld','linker.ld'),('mk/app.mk','app.mk'),('sdk/manifest.json','SDK-PROVENANCE.json'),
                       ('sdk/README.md','README.md'),('VERSION','VERSION'),('LICENSE','LICENSE')]:
    copy(ROOT/source,OUT/target)
for profile in ('coop','preempt'):
    for policy in ('first','best'):
        copy(ROOT/f'build/api-{profile}-{policy}/runtime.o',OUT/f'lib/runtime-{profile}-{policy}.o')
# Generated export: remove obsolete objects and accidentally copied private files.
for file in OUT.rglob('*'):
    if file.is_file() and file not in expected: file.unlink()
(OUT/'.stamp').touch()
