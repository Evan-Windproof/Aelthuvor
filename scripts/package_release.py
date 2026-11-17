#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Deterministic SDK tarball; clean-commit source bundle for official releases."""
import argparse
import gzip
import hashlib
import io
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tarfile
ROOT = Path(__file__).resolve().parents[1]


def git(*args):
    return subprocess.check_output(['git', '-C', str(ROOT), *args])


def digest(data):
    return hashlib.sha256(data).hexdigest()


def encoded(value):
    return (json.dumps(value, indent=2, sort_keys=True)+'\n').encode()


def source_state():
    probe = subprocess.run(['git', '-C', str(ROOT), 'rev-parse', '--show-toplevel'],
                           stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True)
    if probe.returncode == 0 and Path(probe.stdout.strip()).resolve() == ROOT:
        commit = git('rev-parse', 'HEAD').decode().strip()
        dirty = bool(git('status', '--porcelain', '--untracked-files=normal'))
        epoch = int(git('show', '-s', '--format=%ct', 'HEAD'))
        names = git('ls-tree', '-r', '--name-only', 'HEAD').decode().splitlines()
        files = {name: git('show', 'HEAD:'+name) for name in names}
        metadata = dict(source_commit=commit, source_date_epoch=epoch,
                        files={name: digest(data) for name, data in files.items()})
    else:
        metadata = json.loads((ROOT/'SOURCE.json').read_text())
        commit = metadata['source_commit']; epoch = metadata['source_date_epoch']
        if any(Path(name).is_absolute() or '..' in Path(name).parts for name in metadata['files']):
            raise SystemExit('Invalid path in source archive metadata')
        files = {name: (ROOT/name).read_bytes() for name in metadata['files']}
        dirty = any(digest(data) != metadata['files'][name] for name, data in files.items())
        extras = {str(f.relative_to(ROOT)) for f in ROOT.rglob('*') if f.is_file()
                  and not any(part in ('build', '__pycache__', '.git') for part in f.relative_to(ROOT).parts)}
        dirty |= extras != set(files) | {'SOURCE.json'}
    if not re.fullmatch('[0-9a-f]{40}', commit): raise SystemExit('Invalid source commit metadata')
    return commit, dirty, epoch, files, metadata


def compressed(data):
    out = io.BytesIO()
    with gzip.GzipFile(filename='', mode='wb', fileobj=out, mtime=0) as f:
        f.write(data)
    return out.getvalue()


def archive(files, prefix, epoch):
    out = io.BytesIO()
    with tarfile.open(fileobj=out, mode='w', format=tarfile.USTAR_FORMAT) as tar:
        for name, data in sorted(files.items()):
            info = tarfile.TarInfo(prefix+'/'+name)
            info.size = len(data)
            info.mode = 0o644
            info.mtime = epoch
            tar.addfile(info, io.BytesIO(data))
    return compressed(out.getvalue())


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--release', action='store_true', help='require clean commit and add source archive')
    a = p.parse_args()
    version = (ROOT/'VERSION').read_text().strip()
    if not re.fullmatch(r'\d+\.\d+\.\d+', version):
        raise SystemExit('VERSION must be an unprefixed semantic release version')
    commit, dirty, source_epoch, source_files, source_metadata = source_state()
    if a.release and dirty:
        raise SystemExit('Official release requires a clean, committed source tree')
    epoch = int(os.environ.get('SOURCE_DATE_EPOCH', source_epoch))
    if epoch < 0:
        raise SystemExit('SOURCE_DATE_EPOCH must be nonnegative')
    # Recheck the generated export even when Make's stamp is already up to date.
    subprocess.run([sys.executable, str(ROOT/'scripts/export_sdk.py')], check=True)
    toolchain = {tool: subprocess.check_output([os.environ.get('CROSS_COMPILE', '')+tool, '--version'],
                                              text=True).splitlines()[0] for tool in ('gcc', 'ld', 'ar')}
    build = dict(version=version, application_api='1.0', kernel_abi='1.2',
                 liberums_commit='7d5d574e8643c3645fa3ce2a126dcbd4ae43662f',
                 source_commit=commit, source_dirty=dirty, source_date_epoch=epoch,
                 toolchain=toolchain, profiles=['coop', 'preempt'], policies=['first', 'best'])
    files = {str(f.relative_to(ROOT/'build/app-sdk')): f.read_bytes()
             for f in (ROOT/'build/app-sdk').rglob('*') if f.is_file() and f.name != '.stamp'}
    groups = [('examples/application-template', 'templates/application'),
              ('examples/tutorial', 'examples/tutorial'), ('apps/hello', 'examples/hello'),
              ('apps/memory', 'examples/memory'), ('apps/sdk-console', 'examples/console'),
              ('docs/application-development', 'docs/application-development')]
    for source, target in groups:
        for f in sorted((ROOT/source).iterdir()):
            if f.is_file() and (f.suffix in ('.c', '.md') or f.name == 'Makefile'):
                files[target+'/'+f.name] = f.read_bytes()
    for name in ('sdk-reference.md', 'build-guide.md', 'debugging-guide.md',
                 'known-limitations.md', 'architecture.md', 'compatibility.md',
                 'history.md', 'phase6-validation.md', 'maintainer-guide.md'):
        files['docs/'+name] = (ROOT/'docs'/name).read_bytes()
    files['BUILD-INFO.json'] = encoded(build)
    files['SDK-FILES.json'] = encoded({name: digest(data) for name, data in sorted(files.items())})
    for name, data in files.items():
        if any(value in data for value in (str(ROOT).encode(), b'/workspace/', b'/tmp/')):
            raise SystemExit('SDK contains build-machine path: '+name)
    out = ROOT/'build/release'
    out.mkdir(parents=True, exist_ok=True)
    assets = {'aelthuvor-sdk-'+version+'.tar.gz': archive(files, 'aelthuvor-sdk-'+version, epoch)}
    if a.release:
        source_files['SOURCE.json'] = encoded(source_metadata)
        assets['aelthuvor-'+version+'-source.tar.gz'] = archive(source_files, 'aelthuvor-'+version, epoch)
    for name in ('RELEASE-NOTES.md', 'LICENSE'):
        assets[name] = (ROOT/name).read_bytes()
    manifest = dict(build, assets={name: digest(data) for name, data in sorted(assets.items())},
                    sdk_file_count=len(files), source_archive_included=a.release)
    assets['release-manifest.json'] = encoded(manifest)
    for f in out.iterdir():
        if f.is_file() and f.name not in assets and f.name != 'SHA256SUMS': f.unlink()
    for name, data in assets.items():
        (out/name).write_bytes(data)
    (out/'SHA256SUMS').write_text(''.join(digest(data)+'  '+name+'\n' for name, data in sorted(assets.items())))
    print('PASS: SDK package', out/('aelthuvor-sdk-'+version+'.tar.gz'),
          'files='+str(len(files)), 'source='+commit, 'dirty='+str(dirty))


if __name__ == '__main__':
    main()
