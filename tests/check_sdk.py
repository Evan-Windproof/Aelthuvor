#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Verify the vendored public SDK; optionally compare its pinned source commit."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]

def check(source=None, working=True):
    manifest = json.loads((ROOT / 'sdk/manifest.json').read_text())
    sources = [manifest] + list(manifest.get('extensions', {}).values())
    files = {}
    for origin in sources:
        if not re.fullmatch('[0-9a-f]{40}', origin['commit']):
            raise ValueError('SDK source must be an exact commit')
        for name, digest in origin['sha256'].items():
            if name in files or not re.fullmatch(r'[a-z]+[.]h', name):
                raise ValueError('SDK duplicate or invalid file')
            files[name] = (digest, origin['commit'])
    directory = ROOT / 'sdk/include/abi'
    if set(p.name for p in directory.iterdir()) != set(files):
        raise ValueError('SDK file set drifted')
    for name, (digest, commit) in files.items():
        data = (directory / name).read_bytes()
        if hashlib.sha256(data).hexdigest() != digest:
            raise ValueError('SDK hash mismatch: ' + name)
        if source is not None:
            pinned = subprocess.check_output(['git', '-C', str(source), 'show',
                       commit + ':include/abi/' + name])
            if pinned != data or (working and (source / 'include/abi' / name).read_bytes() != data):
                raise ValueError('SDK differs from pinned or working source: ' + name)
    header = (directory / 'syscall.h').read_text()
    for field in ('major', 'minor'):
        if not re.search(r'#define LIBERUMS_ABI_' + field.upper() + r' ' + str(manifest['abi'][field]) + r'u\b', header):
            raise ValueError('SDK version differs from manifest')
    print('PASS: SDK SHA256, file set and ABI version' + ('; pinned Liberums source' if source else ''))

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path)
    parser.add_argument('--pinned-only', action='store_true', help='compare Git provenance only, not current worktree headers')
    args = parser.parse_args()
    try:
        check(args.source, working=not args.pinned_only)
    except (ValueError, OSError, subprocess.SubprocessError) as error:
        print('FAIL:', error, file=sys.stderr)
        sys.exit(1)
