#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Pinned v1.0.0 MMU/PIT/ABI audits, plus Ael-owned lifecycle/pressure cases.

Reuse the reference kernel's trusted, gated diagnostic and GDB assertions without
editing them. Debugger writes are scalar application inputs or diagnostic mailbox
requests, never resource metadata, page tables, successful returns or timer ticks.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
from check_sdk import ROOT, check

PIN = '7d5d574e8643c3645fa3ce2a126dcbd4ae43662f'


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--liberums', required=True, type=Path)
    p.add_argument('--qemu', default='qemu-system-i386')
    p.add_argument('--gdb', default='gdb')
    a = p.parse_args()
    kernel = a.liberums.resolve()

    def git(*args):
        return subprocess.check_output(['git', '-C', str(kernel), *args], text=True).strip()

    if git('rev-parse', 'HEAD') != PIN or git('rev-parse', 'v1.0.0^{commit}') != PIN:
        raise SystemExit('Phase 5 requires the exact Liberums v1.0.0 checkout')
    if git('status', '--porcelain', '--untracked-files=no'):
        raise SystemExit('Reference kernel tracked worktree must be clean')
    check(kernel)
    subprocess.run(['make', '-C', str(kernel), '-j2', 'build/liberums.elf',
                    'phase8-fixtures', 'phase9-fixtures', 'test-phase6-native',
                    'test-abi-contract'], check=True)
    sys.path.insert(0, str(kernel/'tests'))
    import phase9
    from phase8 import FIX, RESULTS, stress

    a.aelthuvor = ROOT/'build'
    inputs = [kernel/'build/liberums.elf', kernel/'build/phase9/abi-probe.elf',
              FIX/'worker-a.elf', FIX/'worker-b.elf',
              a.aelthuvor/'independent-libos/reference.elf']
    inputs += [ROOT/f'build/phase9/ael-{policy}/app.elf' for policy in ('first', 'best')]

    def fingerprint():
        return {str(f): dict(sha256=hashlib.sha256(f.read_bytes()).hexdigest(),
                             mtime_ns=f.stat().st_mtime_ns) for f in inputs}

    before = fingerprint()
    phase9.abi(a)
    phase9.routing(a)
    phase9.diagnostics(a)
    stress(a, FIX/'worker-a.elf', FIX/'worker-b.elf', rounds=128)
    # The reference runner consumes our independently built, extended SDK ELF.
    phase9.BUILD = ROOT/'build'
    phase9.ram_cases(a)

    for policy in ('first', 'best'):
        app = ROOT/f'build/phase9/ael-{policy}/app.elf'
        names = phase9.symbols(app)
        files = [app, a.aelthuvor/'independent-libos/reference.elf',
                 FIX/'worker-a.elf', FIX/'worker-b.elf']
        prefix = 'R='+repr(names)+'\n'+phase9.RAM_HELPERS+'\nbackground()\n'
        end = '''for i in (3,4): command(i,1)
finished()
for i in (1,2,3,4):
    check(dd(i,'last_state')==3 and dd(i,'exit_status')==0,'lifecycle/isolation terminal result')
'''
        body = prefix + '''ram_op(12,9)
check(dd(1,'resource_count')==1,'seeded pressure leaked pages')
check(ur(1,R['ram_pressure_oom'])>0,'seeded guest pressure never reached quota')
print('PRESSURE policy=POLICY seed=0x5a17c0de operations=512 OOM=%d'%ur(1,R['ram_pressure_oom']))
ram_op(13,10)
ci=ur(1,R['cycle_index']);cg=ur(1,R['cycle_generation']);pa=ur(1,R['cycle_pfn'])<<12
check(dd(1,'resource_count')==2 and pt(1,0x09002000)&~4095==pa,'CLAIM/MAP accounting/PTE')
check(words(pa,1024)==tuple(0xa5e10000|j for j in range(1024)),'actual CPL3 page writes')
ram_op(14,11)
check(pt(1,0x09002000)==0 and dd(1,'resource_count')==2,'UNMAP incorrectly released ownership')
check(words(pa,1024)==tuple(0xa5e10000|j for j in range(1024)),'UNMAP corrupted RAM')
ram_op(15,12)
check(pt(1,0x09002000)==0 and dd(1,'resource_count')==1,'RELEASE mapping/accounting')
check(v('states[%d]'%(pa>>12))==2 and not any(words(pa,1024)),'RELEASE did not zero/free')
check(v('resources[%d].state'%(ci-1))==0 and rr('result')==(-116&0xffffffff),'released capability still valid')
print('NORMAL lifecycle: CLAIM MAP 1024 CPL3 writes UNMAP REMAP RELEASE stale QUERY; seeded=512')
rw('command',7)
'''.replace('POLICY', policy) + end
        phase9.case(a, 'phase5-'+policy+'-normal-pressure', files, body)
        for mode in ('ack-reuse', 'idle-force-reuse'):
            body = prefix + '''h,pa=ram_owner()
rw('index',h[0]);rw('generation',h[1]);rw('pfn',pa>>12)
ram_op(1,2)
'''
            if mode == 'ack-reuse':
                body += '''req,deadline=request(1,h,1000)
rw('request',req)
rw('generation',h[1]+1);ram_op(6,6)
check(rr('result')==(-116&0xffffffff) and dd(1,'resource_count')==1,'wrong-generation ACK mutated owner')
rw('generation',h[1]);rw('request',req+1);ram_op(6,6)
check(rr('result')==(-116&0xffffffff) and v('acknowledged')==0,'wrong-request ACK completed event')
rw('request',req);rw('command',2)
go('revoke_completed');reclaimed(h,pa)
check(v('acknowledged')==1 and v('forced')==0 and v('ticks')<deadline,'idle ACK timing')
ram_idle(3)
'''
            else:
                body += '''req,deadline=request(1,h,3)
go('revoke_completed');reclaimed(h,pa)
check(v('forced')==1 and v('ticks')>=deadline,'idle force not real PIT completion')
ram_op(2,3)
'''
            body += '''ram_op(16,13)
check(dd(1,'resource_count')==0,'fresh SDK generation allocation/trim leaked')
rw('command',7)
''' + end
            phase9.case(a, 'phase5-'+policy+'-'+mode, files, body)

    if fingerprint() != before or git('status', '--porcelain', '--untracked-files=no'):
        raise SystemExit('Frozen inputs or reference kernel tracked files changed')
    evidence = ROOT/'build/phase5-evidence'
    evidence.mkdir(exist_ok=True)
    for case in RESULTS:
        name = case['log']
        for filename in (name, name.replace('-gdb.log', '-serial.log'),
                         name.replace('-gdb.log', '.gdb')):
            shutil.copy2(kernel/'build'/filename, evidence/filename)
        case['log'] = 'phase5-evidence/'+name
    report = dict(kernel_commit=PIN, tag='v1.0.0', guests=len(RESULTS),
                  slot_reuse_rounds=128, seed='0x5a17c0de',
                  guest_pressure_operations_per_policy=512, cases=RESULTS,
                  frozen_inputs=before, kernel_tracked_worktree_clean=True)
    (ROOT/'build/phase5-results.json').write_text(json.dumps(report, indent=2)+'\n')
    print(f'PASS: Phase 5 {len(RESULTS)} real QEMU/GDB guests; immutable v1.0.0 kernel')


if __name__ == '__main__':
    main()
