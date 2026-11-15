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

def check(source=None):
    manifest = json.loads((ROOT / 'sdk/manifest.json').read_text())
    if not re.fullmatch('[0-9a-f]{40}', manifest['commit']):
        raise ValueError('SDK source must be an exact commit')
    directory = ROOT / 'sdk/include/abi'
    if set(p.name for p in directory.iterdir()) != set(manifest['sha256']):
        raise ValueError('SDK file set drifted')
    for name, digest in manifest['sha256'].items():
        data = (directory / name).read_bytes()
        if hashlib.sha256(data).hexdigest() != digest:
            raise ValueError('SDK hash mismatch: ' + name)
        if source is not None:
            pinned = subprocess.check_output(['git', '-C', str(source), 'show',
                       manifest['commit'] + ':include/abi/' + name])
            if pinned != data or (source / 'include/abi' / name).read_bytes() != data:
                raise ValueError('SDK differs from pinned or working source: ' + name)
    header = (directory / 'syscall.h').read_text()
    for field in ('major', 'minor'):
        if not re.search(r'#define LIBERUMS_ABI_' + field.upper() + r' ' + str(manifest['abi'][field]) + r'u\b', header):
            raise ValueError('SDK version differs from manifest')
    print('PASS: SDK SHA256, file set and ABI version' + ('; pinned Liberums source' if source else ''))

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path)
    args = parser.parse_args()
    try:
        check(args.source)
    except (ValueError, OSError, subprocess.SubprocessError) as error:
        print('FAIL:', error, file=sys.stderr)
        sys.exit(1)
