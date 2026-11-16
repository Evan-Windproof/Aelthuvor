#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Reject core and optional-extension drift without conflating runtime compatibility."""
import contextlib
import io
import json
from pathlib import Path
import shutil
import tempfile
import check_sdk

original_root = check_sdk.ROOT
baseline = json.loads((original_root/'sdk/manifest.json').read_text())
assert baseline['commit'] == '85a493466afe2038ca22a9870f6f9ce37dd7f3a8'
assert baseline['abi'] == dict(major=1, minor=2)
assert set(baseline['sha256']) == {'syscall.h','elf.h','event.h','resource.h'}
assert baseline['extensions']['devices']['commit'] == '5d9c8f35a4ae54ef08b93b7f4d5fecd9e350acbf'


def manifest_change(root, operation):
    path = root/'sdk/manifest.json'
    data = json.loads(path.read_text())
    operation(data)
    path.write_text(json.dumps(data))


mutations = [
    lambda r: (r/'sdk/include/abi/syscall.h').write_bytes(b'corrupt'),
    lambda r: (r/'sdk/include/abi/private.h').write_text('unexpected'),
    lambda r: (r/'sdk/include/abi/event.h').unlink(),
    lambda r: manifest_change(r, lambda m: m['abi'].update(major=9)),
]
for name in ('vga.h', 'keyboard.h'):
    mutations.extend([
        lambda r, name=name: (r/'sdk/include/abi'/name).unlink(),
        lambda r, name=name: (r/'sdk/include/abi'/name).write_bytes(b'corrupt'),
    ])
mutations.extend([
    lambda r: manifest_change(r, lambda m: m['extensions']['devices'].update(commit='main')),
    lambda r: manifest_change(r, lambda m: m['extensions']['devices']['sha256'].update(m['sha256'])),
])
for mutate in mutations:
    with tempfile.TemporaryDirectory() as temporary:
        root = Path(temporary)
        shutil.copytree(original_root/'sdk', root/'sdk')
        check_sdk.ROOT = root
        with contextlib.redirect_stdout(io.StringIO()):
            check_sdk.check()
        mutate(root)
        try:
            check_sdk.check()
        except ValueError:
            pass
        else:
            raise RuntimeError('SDK drift incorrectly accepted')
check_sdk.ROOT = original_root
print(f'PASS: {len(mutations)} core/extension SDK drift rejection tests')
