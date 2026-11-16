#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Rebuild the genuine Phase 1 source/SDK, never a new binary with a changed note."""
import hashlib
import io
import json
from pathlib import Path
import subprocess
import tarfile

ROOT = Path(__file__).resolve().parents[1]
COMMIT = 'b10d337d254b18e98204990203a77a215c82c568'

def build():
    archive = subprocess.check_output(['git', '-C', str(ROOT), 'archive', COMMIT])
    directory = ROOT / 'build/abi11'
    directory.mkdir(parents=True, exist_ok=True)
    # Existing generated outputs are invalidated when historical source is extracted.
    with tarfile.open(fileobj=io.BytesIO(archive)) as source:
        source.extractall(directory, filter='data')
    subprocess.run(['make', '-C', str(directory), '-B', '-j2', 'all'], check=True)
    image = directory / 'build/first.elf'
    provenance = dict(commit=COMMIT, archive_sha256=hashlib.sha256(archive).hexdigest(),
                      elf_sha256=hashlib.sha256(image.read_bytes()).hexdigest(),
                      sdk=json.loads((directory/'sdk/manifest.json').read_text()))
    (directory/'provenance.json').write_text(json.dumps(provenance, indent=2)+'\n')
    print('PASS: unchanged ABI 1.1 historical source and SDK rebuilt:', COMMIT)
    return image

if __name__ == '__main__':
    build()
