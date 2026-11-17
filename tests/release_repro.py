#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Extract the official source without .git, rebuild SDK, compare release bytes."""
import hashlib
import json
from pathlib import Path
import subprocess
import tarfile
import tempfile
from check_sdk import ROOT


def snapshot(directory):
    return {f.name: hashlib.sha256(f.read_bytes()).hexdigest() for f in directory.iterdir() if f.is_file()}


def main():
    assets=ROOT/'build/release'
    before=snapshot(assets)
    manifest=json.loads((assets/'release-manifest.json').read_text())
    assert manifest['source_archive_included'] and not manifest['source_dirty']
    version=manifest['version']; prefix='aelthuvor-'+version
    with tempfile.TemporaryDirectory(prefix='ael-source-repro-') as path:
        destination=Path(path)
        with tarfile.open(assets/(prefix+'-source.tar.gz')) as tar:
            for member in tar.getmembers():
                assert member.isfile() and Path(member.name).parts[0]==prefix
                assert '..' not in Path(member.name).parts
                assert 'build' not in Path(member.name).parts and '.git' not in Path(member.name).parts
            tar.extractall(destination,filter='data')
        source=destination/prefix
        assert not (source/'.git').exists()
        metadata=json.loads((source/'SOURCE.json').read_text())
        assert metadata['source_commit']==manifest['source_commit']
        for name,digest in metadata['files'].items():
            assert hashlib.sha256((source/name).read_bytes()).hexdigest()==digest,name
        result=subprocess.run(['make','-C',str(source),'-j2','app-sdk','release-package'],
                              stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
        (ROOT/'build/release-repro.log').write_text(result.stdout)
        assert result.returncode==0,'source-archive rebuild failed; see build/release-repro.log'
        assert snapshot(source/'build/release')==before,'source/SDK archives differ across checkout paths'
        edited=source/'examples/application-template/main.c'
        edited.write_bytes(edited.read_bytes()+b'\n')
        denied=subprocess.run(['make','-C',str(source),'release-package'],
                              stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
        assert denied.returncode!=0 and 'clean, committed source tree' in denied.stdout
    assert snapshot(assets)==before,'reproduction changed original assets'
    (ROOT/'build/release-repro-results.json').write_text(json.dumps(dict(
        source_commit=manifest['source_commit'],assets_sha256=before,
        source_archive_without_git=True,rebuilt_in_different_path=True,
        all_release_assets_byte_identical=True,modified_source_release_rejected=True),indent=2)+'\n')
    print('PASS: no-Git source archive rebuild; all release assets byte-identical across paths; modified source refused')


if __name__=='__main__': main()
