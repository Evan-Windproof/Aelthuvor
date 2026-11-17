#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Consumer builds from exported files ONLY, including a non-Aelthuvor LibOS."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
from check_sdk import ROOT

def run(args,**kwargs):
    return subprocess.run(args,check=True,**kwargs)

def identity(path):
    return (path.stat().st_mtime_ns,hashlib.sha256(path.read_bytes()).hexdigest())

sdk=ROOT/'build/app-sdk'
for archive in ('libaelthuvor.a','libaeldevices.a'):
    symbols=subprocess.check_output(['nm','--defined-only',str(sdk/'lib'/archive)],text=True)
    assert not any(line.split()[-1] in ('ael_main','ael_app_main','_start','ael_start') for line in symbols.splitlines() if line.split())
for path in (ROOT/'build/apps/hello/app.elf', ROOT/'build/apps/memory/app.elf'):
    symbols=subprocess.check_output(['nm',str(path)],text=True)
    assert not any(x in symbols for x in ('ael_vga_','ael_keyboard_','ael_text_')),'optional device code pulled into a plain app'
    assert not subprocess.check_output(['nm','-u',str(path)]).strip()

isolated=Path(tempfile.mkdtemp(prefix='libos-phase4-consumer-',dir='/tmp'))
shutil.copytree(sdk,isolated/'sdk',dirs_exist_ok=True)
shutil.copytree(ROOT/'examples/application-template',isolated/'application',dirs_exist_ok=True)
application=isolated/'application'
command=['make','-C',str(application),'AEL_SDK='+str(isolated/'sdk'),'OUT='+str(isolated/'app-out')]
run(command+['clean'])
run(command)
first=hashlib.sha256((isolated/'app-out/app.elf').read_bytes()).hexdigest()
retained=list((ROOT/'build/src').glob('*.o'))+list((ROOT/'build').glob('*.a'))+list((sdk/'lib').iterdir())
before={str(p):identity(p) for p in retained}
source=application/'main.c'
source.write_text(source.read_text().replace('Standalone Aelthuvor application template passed','Edited independent application passed'))
run(command)
after=hashlib.sha256((isolated/'app-out/app.elf').read_bytes()).hexdigest()
assert first!=after,'edited application was not rebuilt'
assert before=={str(p):identity(p) for p in retained},'application change rebuilt the LibOS'
run(command+['APP_POLICY=best'])
best_policy=hashlib.sha256((isolated/'app-out/app.elf').read_bytes()).hexdigest()
assert best_policy!=after,'policy selection retained the previous runtime'
run(command+['APP_POLICY=first'])
assert hashlib.sha256((isolated/'app-out/app.elf').read_bytes()).hexdigest()==after,'returning to first-fit did not restore the selected runtime'

for dependency in (isolated/'app-out').glob('*.d'):
    data=dependency.read_text()
    assert str(ROOT/'src') not in data and '/include/kernel/' not in data
    assert '/src/runtime' not in data

# An application must choose exactly one entry adapter. A real conflicting legacy
# object plus the SDK adapter must FAIL to link, not win by archive ordering.
conflict=isolated/'conflict.c'
conflict.write_text('int ael_main(void *heap) { (void)heap; return 0; }\n')
run(['gcc','-m32','-ffreestanding','-fno-pie','-c',str(conflict),'-o',str(isolated/'conflict.o')])
result=subprocess.run(['ld','-m','elf_i386','-nostdlib','-T',str(sdk/'linker.ld'),str(isolated/'conflict.o'),
    str(isolated/'app-out/main.o'),str(sdk/'lib/crt0.o'),str(sdk/'lib/runtime-preempt-first.o'),
    str(sdk/'lib/app-entry.o'),str(sdk/'lib/libaelthuvor.a'),'-o',str(isolated/'conflicting.elf')],text=True,capture_output=True)
assert result.returncode!=0 and 'multiple definition' in result.stderr and 'ael_main' in result.stderr

reference=isolated/'reference'
shutil.copytree(ROOT/'reference/independent-libos',reference,dirs_exist_ok=True)
shutil.copytree(ROOT/'sdk/include/abi',isolated/'public/include/abi',dirs_exist_ok=True)
run(['make','-C',str(reference),'SDK='+str(isolated/'public/include'),'OUT='+str(isolated/'ref-out')])
ref=isolated/'ref-out/reference.elf'
symbols=subprocess.check_output(['nm',str(ref)],text=True)
assert 'ael_' not in symbols and not subprocess.check_output(['nm','-u',str(ref)]).strip()
assert 'libael' not in (isolated/'ref-out/reference.map').read_text()
for dependency in (isolated/'ref-out').glob('*.d'):
    data=dependency.read_text()
    assert '/include/aelthuvor/' not in data and '/include/kernel/' not in data and str(ROOT/'src') not in data
for file in reference.glob('*'):
    if file.suffix in ('.c','.h','.S'):
        assert 'aelthuvor/' not in file.read_text() and 'kernel/' not in file.read_text()

# Retain the separately built, edited consumer artifacts for REAL guest execution.
shutil.copy2(isolated/'app-out/app.elf',ROOT/'build/third-party-app.elf')
shutil.copy2(ref,ROOT/'build/independent-isolated.elf')
(ROOT/'build/phase4-build-results.json').write_text(json.dumps(dict(
    isolation_directory=str(isolated),application_before=first,application_after=after,core_unchanged=True,
    reference_sha256=hashlib.sha256(ref.read_bytes()).hexdigest(),
    entry_collision_rejected=True,reference_no_ael_dependencies=True),indent=2)+'\n')
print('PASS: exported SDK, edited external app, unchanged LibOS, rejected entry collision, isolated independent LibOS')
