#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Build the independent kernel and pass only external ELF paths to its auditor."""
import argparse
from pathlib import Path
import subprocess
import sys
from check_sdk import ROOT, check

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--liberums',type=Path,default=ROOT.parent/'Liberums')
parser.add_argument('--qemu',default='qemu-system-i386')
parser.add_argument('--gdb',default='gdb')
args=parser.parse_args()
try:
    kernel=args.liberums.resolve()
    check()
    # Verify current public headers too. Pinned Git provenance is checked by
    # make check-sdk SDK_SOURCE=... when updating the SDK, independently of build.
    for file in (ROOT/'sdk/include/abi').iterdir():
        if file.read_bytes() != (kernel/'include/abi'/file.name).read_bytes():
            raise ValueError('public ABI drift: '+file.name)
    subprocess.run(['make','-C',str(kernel),'-j2'],check=True)
    subprocess.run([sys.executable,str(kernel/'tests/phase5.py'),'--aelthuvor',str(ROOT/'build'),
                    '--qemu',args.qemu,'--gdb',args.gdb],check=True)
except (OSError,ValueError,subprocess.SubprocessError) as error:
    print('FAIL:',error,file=sys.stderr)
    sys.exit(1)
