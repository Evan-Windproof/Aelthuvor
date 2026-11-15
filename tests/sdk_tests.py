#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Ensure hash, file-set and version drift actually fail the build gate."""
import contextlib
import io
import json
from pathlib import Path
import shutil
import tempfile
import check_sdk

with tempfile.TemporaryDirectory() as temporary:
    root=Path(temporary)
    shutil.copytree(check_sdk.ROOT/'sdk',root/'sdk')
    check_sdk.ROOT=root
    header=root/'sdk/include/abi/syscall.h'
    original=header.read_bytes()
    with contextlib.redirect_stdout(io.StringIO()):
        check_sdk.check()
    mutations=[lambda: header.write_bytes(original+b'\n'),
               lambda: (header.parent/'private.h').write_text('/* unexpected */'),
               lambda: header.unlink()]
    for mutate in mutations:
        mutate()
        try:
            check_sdk.check()
        except ValueError:
            pass
        else:
            raise RuntimeError('SDK drift incorrectly accepted')
        header.write_bytes(original)
        (header.parent/'private.h').unlink(missing_ok=True)
    manifest=root/'sdk/manifest.json'
    data=json.loads(manifest.read_text())
    data['abi']['major']=9
    manifest.write_text(json.dumps(data))
    try:
        check_sdk.check()
    except ValueError:
        pass
    else:
        raise RuntimeError('ABI manifest mismatch incorrectly accepted')
print('PASS: 4 SDK drift rejection tests')
