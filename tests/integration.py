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
    # Provenance is exact; runtime compatibility is independently checked by the
    # loader, runtime and GDB auditor, not by comparing the kernel worktree SDK.
    check(kernel, working=False)
    subprocess.run(['make','-C',str(kernel),'-j2'],check=True)
    sys.path.append(str(kernel/'tests'))
    from guest_audit import suite
    suite(ROOT/'build',args.qemu,args.gdb)
except (OSError,ValueError,RuntimeError,subprocess.SubprocessError) as error:
    print('FAIL:',error,file=sys.stderr)
    sys.exit(1)
