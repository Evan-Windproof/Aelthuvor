#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Release archive integrity and real package-only compiler/linker consumers."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tarfile
import tempfile
from check_sdk import ROOT


def identity(path):
    return dict(sha256=hashlib.sha256(path.read_bytes()).hexdigest(), mtime_ns=path.stat().st_mtime_ns)


def unpack(archive, destination):
    with tarfile.open(archive) as tar:
        names = tar.getnames()
        prefix = 'aelthuvor-sdk-'+(ROOT/'VERSION').read_text().strip()
        if len(set(names)) != len(names): raise ValueError('duplicate SDK members')
        for m in tar.getmembers():
            parts = Path(m.name).parts
            if not m.isfile() or not parts or parts[0] != prefix or '..' in parts or m.uid or m.gid:
                raise ValueError('unsafe or non-normalized SDK member: '+m.name)
        tar.extractall(destination, filter='data')
    sdk = destination/prefix
    validate(sdk)
    return sdk


def validate(sdk):
    recorded = json.loads((sdk/'SDK-FILES.json').read_text())
    actual = {str(f.relative_to(sdk)): identity(f)['sha256'] for f in sdk.rglob('*')
              if f.is_file() and f.name != 'SDK-FILES.json'}
    if actual != recorded: raise ValueError('SDK file set or member SHA256 mismatch')
    provenance = json.loads((sdk/'SDK-PROVENANCE.json').read_text())
    for origin in [provenance, *provenance['extensions'].values()]:
        for name, digest in origin['sha256'].items():
            assert identity(sdk/'include/abi'/name)['sha256'] == digest
    for name in actual:
        data = (sdk/name).read_bytes()
        assert not any(x in data for x in (str(ROOT).encode(), b'/workspace/', b'/tmp/')), name
    assert not any(f.endswith(('.elf', '.log', '.pyc')) or '/src/' in f for f in actual)
    for lib in ('libaelthuvor.a', 'libaeldevices.a'):
        nm = subprocess.check_output(['nm', '--defined-only', str(sdk/'lib'/lib)], text=True)
        assert not any(row.split()[-1] in ('_start', 'ael_start', 'ael_main', 'ael_app_main')
                       for row in nm.splitlines() if row.split())


def consumers(archive, directory):
    sdk = unpack(archive, directory)
    frozen = {str(f): identity(f) for f in (sdk/'lib').iterdir()}
    frozen.update({str(f): identity(f) for f in (ROOT/'build').glob('*.a')})
    application = directory/'application'
    shutil.copytree(sdk/'templates/application', application)

    def build(folder, out, **settings):
        command = ['make', '-C', str(folder), 'AEL_SDK='+str(sdk), 'OUT='+str(out)]
        subprocess.run(command+[str(k)+'='+str(v) for k, v in settings.items()], check=True)
        elf = out/'app.elf'
        assert not subprocess.check_output(['nm', '-u', str(elf)]).strip()
        header = subprocess.check_output(['readelf', '-h', str(elf)], text=True)
        assert 'ELF32' in header and 'Intel 80386' in header and 'EXEC' in header
        for dep in out.glob('*.d'):
            text = dep.read_text()
            assert str(ROOT) not in text and 'kernel/' not in text
            assert 'src/app_internal.h' not in text
        linkmap = (out/'app.map').read_text()
        assert str(ROOT) not in linkmap and str(sdk/'lib/libaelthuvor.a') in linkmap
        return elf

    before = build(application, directory/'initial')
    before_hash = identity(before)['sha256']
    main = application/'main.c'
    old = main.read_text()
    edited = old.replace('Standalone Aelthuvor application template passed', 'Released SDK independent application passed')
    assert old != edited
    main.write_text(edited)
    artifacts = {}
    for profile in ('preempt', 'coop'):
        for policy in ('first', 'best'):
            label = 'external-'+profile+'-'+policy
            artifacts[label] = build(application, directory/label, APP_PROFILE=profile, APP_POLICY=policy)
    assert identity(artifacts['external-preempt-first'])['sha256'] != before_hash
    # The same source/prefix produces identical ELF bytes when only OUT changes.
    repeated = build(application, directory/'repeat', APP_PROFILE='preempt', APP_POLICY='first')
    assert repeated.read_bytes() == artifacts['external-preempt-first'].read_bytes()
    tutorial = sdk/'examples/tutorial'
    for source in sorted(tutorial.glob('*.c')):
        name = source.stem
        artifacts[name] = build(tutorial, directory/name, LESSON=name)
    artifacts['03-policy-best'] = build(tutorial, directory/'03-policy-best', LESSON='03-policy', APP_POLICY='best')
    artifacts['10-project-console'] = build(tutorial, directory/'10-project-console', LESSON='10-project', APP_DEVICES=1)
    for name in ('external-preempt-first', '02-memory', '10-project'):
        nm = subprocess.check_output(['nm', str(artifacts[name])], text=True)
        assert not any(x in nm for x in ('ael_vga_', 'ael_keyboard_', 'ael_text_'))
    for name, old_identity in frozen.items(): assert identity(Path(name)) == old_identity, name
    return artifacts, dict(initial_sha256=before_hash,
                           edited_sha256=identity(artifacts['external-preempt-first'])['sha256'],
                           isolated_directory=str(directory), sdk_libraries_unchanged=True,
                           core_libraries_unchanged=True)


def main():
    archive = ROOT/('build/release/aelthuvor-sdk-'+(ROOT/'VERSION').read_text().strip()+'.tar.gz')
    before = archive.read_bytes()
    stale = ROOT/'build/app-sdk/obsolete-private-kernel.o'
    stale.write_bytes(b'obsolete generated file must not ship')
    subprocess.run([sys.executable, str(ROOT/'scripts/package_release.py')], check=True)
    assert not stale.exists(), 'obsolete SDK export was packaged'
    assert archive.read_bytes() == before, 'SDK packaging is not deterministic'
    with tempfile.TemporaryDirectory(prefix='ael-release-consumer-') as path:
        directory = Path(path)
        artifacts, report = consumers(archive, directory)
        sdk = directory/('aelthuvor-sdk-'+(ROOT/'VERSION').read_text().strip())
        # A modified public header and an unexpected private file are both rejected.
        header = sdk/'include/aelthuvor/app.h'; original = header.read_bytes()
        header.write_bytes(original+b'\n')
        try: validate(sdk)
        except ValueError: pass
        else: raise AssertionError('SDK mutation accepted')
        header.write_bytes(original)
        extra = sdk/'unexpected-private.h'; extra.write_text('not in manifest')
        try: validate(sdk)
        except ValueError: pass
        else: raise AssertionError('SDK extra file accepted')
        extra.unlink()
        report.update(artifacts={k: identity(v)['sha256'] for k,v in artifacts.items()},
                      archive_sha256=hashlib.sha256(before).hexdigest(), artifact_count=len(artifacts))
    (ROOT/'build/phase6-build-results.json').write_text(json.dumps(report, indent=2)+'\n')
    print('PASS: deterministic SDK package, member/provenance hashes, mutation rejection, 16 package-only ELF consumers')


if __name__ == '__main__':
    main()
