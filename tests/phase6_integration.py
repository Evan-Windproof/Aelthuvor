#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Run actual package-only applications and every tutorial on pinned Liberums."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
from check_sdk import ROOT, check
from package_tests import consumers, identity

PIN = '7d5d574e8643c3645fa3ce2a126dcbd4ae43662f'


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--liberums', required=True, type=Path)
    p.add_argument('--qemu', default='qemu-system-i386')
    p.add_argument('--gdb', default='gdb')
    a = p.parse_args(); kernel = a.liberums.resolve()
    assert subprocess.check_output(['git','-C',str(kernel),'rev-parse','HEAD'],text=True).strip() == PIN
    assert not subprocess.check_output(['git','-C',str(kernel),'status','--porcelain','--untracked-files=no'])
    check(kernel)
    subprocess.run(['make','-C',str(kernel),'-j2','build/liberums.elf','phase8-fixtures'],check=True)
    elf = kernel/'build/liberums.elf'; frozen = identity(elf)
    archive = ROOT/('build/release/aelthuvor-sdk-'+(ROOT/'VERSION').read_text().strip()+'.tar.gz')
    package_identity = identity(archive)
    sys.path.append(str(kernel/'tests'))
    from phase1 import require, wait_for
    from phase2 import connect_and_run
    from phase3 import HELPERS
    from phase5 import headers, symbols
    from phase72 import vm, injector, INJECT
    from phase73 import lifecycle
    from phase8 import COMMON
    from phase4_integration import case
    results=[]

    def audited(label, file, console=False):
        names=symbols(file); blob=file.read_bytes(); guest='ael-phase6-'+label
        with vm(a.qemu,'multi-external'+(' multi-vga=1 multi-keyboard=1' if console else ''),guest,file) as (proc,log,port,endpoint):
            with injector(endpoint) as control:
                commands=HELPERS+f'add-symbol-file "{file}"\n'+INJECT.replace('CONTROL',repr(control))
                commands+='python\n'+COMMON.replace('SYMBOLS','{}')
                commands+=f'used=ready([0],{[(headers(blob),blob)]!r})\nN={names!r}\n'
                commands+='if live_bp is not None: live_bp.delete();live_bp=None\nseen=[]\npa=0\n'
                commands+=r'''
def observed():
    global pa
    if v('current_domain')==0 or v('current_domain->id')!=1: return
    user(1)
    stage=words(v('$esp')+4,1)[0];seen.append(stage)
'''
                if console:
                    commands+=r'''
    if stage==0:
        check(pt(1,0x09400000)&0xfffff09f==0xb801f,'Console mapping/permissions')
        check(v('vga_device.active')==1 and v('keyboard.active')==1,'Console capabilities not acquired')
        inject(['h','e','l','l','o','ret'])
    elif stage==1:
        line=b''.join(bytes(gdb.selected_inferior().read_memory(physical(1,N['lesson_line']+i),1)) for i in range(64)).split(b'\0')[0]
        check(line==b'hello','real PS2 line did not reach tutorial')
        check(b'hello' in bytes(gdb.selected_inferior().read_memory(0xb8000,4000))[::2],'typed line absent from actual VGA')
    elif stage==2:
        check(v('vga_device.active')==0 and v('keyboard.active')==0 and pt(1,0x09400000)==0,'explicit device close leaked')
        check(dd(1,'resource_count')==0,'project leaked RAM')
    else: check(False,'unexpected Console stage')
'''
                    address=names['lesson_console_observed']
                else:
                    commands+=r'''
    if stage==1:
        ptr=ur(1,N['lesson_pointer']);pa=pt(1,ptr)&~4095
        check(ptr==0x09000000 and pt(1,ptr)&7==7 and dd(1,'resource_count')==1,'actual SDK ownership/PTE')
        check(words(pa,1024)==tuple(0xa6e10000|i for i in range(1024)),'actual CPL3 tutorial writes')
        check(v('ael_runtime.heap.live')==1 and v('ael_runtime.pages.owned')==1,'SDK metadata accounting')
    elif stage==2:
        check(ur(1,N['lesson_pointer'])==0 and pt(1,0x09000000)==0 and dd(1,'resource_count')==0,'tutorial Trim/clear')
        check(v('states[%d]'%(pa>>12))==2 and not any(words(pa,1024)),'tutorial physical RAM not zero/free')
    else: check(False,'unexpected debug stage')
'''
                    address=names['lesson_observed']
                commands+=r'''
class Observe(gdb.Breakpoint):
    def stop(self):
        try: observed()
        except Exception:
            import traceback;gdb.write(traceback.format_exc());gdb.execute('quit 1')
        return False
'''
                commands+=f'observer=Observe("*{address:#x}",type=gdb.BP_HARDWARE_BREAKPOINT,internal=True)\nfinished()\n'
                commands+=f'check(seen=={[0,1,2] if console else [1,2]!r},"tutorial stages skipped")\n'
                commands+='check(dd(1,"last_state")==3 and dd(1,"exit_status")==0,"tutorial terminal status")\n'
                if console: commands+='check(v("keyboard_irqs")>0,"no real keyboard IRQ")\n'
                commands+=f'print("PASS: Phase6 {label}")\nend\ndetach\nquit\n'
                output=connect_and_run(proc,log,port,a.gdb,commands,guest)
                require('PASS: Phase6 '+label in output,'tutorial audit incomplete')
                wait_for(proc,log,lambda s:'Phase 8: multi external execution completed' in s,'tutorial complete')
                require('PANIC' not in log.read_text(),'tutorial kernel panic')
        results.append(dict(label=label,lifetimes=1,log=guest+'-gdb.log'))

    with tempfile.TemporaryDirectory(prefix='ael-release-run-') as path:
        artifacts, build_report=consumers(archive,Path(path))
        retained=ROOT/'build/phase6-consumers';retained.mkdir(exist_ok=True)
        for label,file in artifacts.items():
            shutil.copy2(file,retained/(label+'.elf'))
            shutil.copy2(file.with_suffix('.map'),retained/(label+'.map'))
        for profile in ('preempt','coop'):
            for policy in ('first','best'):
                label='external-'+profile+'-'+policy;file=artifacts[label]
                if profile=='preempt':
                    case(a,'phase6-'+label,[file]);log='ael-phase4-phase6-'+label+'-gdb.log';lifetimes=1
                    serial=(kernel/'build'/log.replace('-gdb.log','-serial.log')).read_text()
                    require('Released SDK independent application passed' in serial,'edited package-only source did not execute')
                else:
                    lifecycle(a.qemu,a.gdb,'phase6-'+label,file,'external')
                    log='phase73-phase6-'+label+'-gdb.log';lifetimes=2
                results.append(dict(label=label,lifetimes=lifetimes,log=log))
        for label,file in artifacts.items():
            if label.startswith('external-'): continue
            if label in ('05-console','10-project-console'): audited(label,file,True)
            elif label=='08-debug': audited(label,file)
            else:
                case(a,'phase6-'+label,[file])
                results.append(dict(label=label,lifetimes=1,log='ael-phase4-phase6-'+label+'-gdb.log'))
        case(a,'phase6-independent-peer',[artifacts['08-debug'],ROOT/'build/independent-libos/reference.elf'])
        results.append(dict(label='independent-peer',lifetimes=2,log='ael-phase4-phase6-independent-peer-gdb.log'))
    assert identity(elf)==frozen and identity(archive)==package_identity,'consumer work changed frozen kernel/package'
    assert not subprocess.check_output(['git','-C',str(kernel),'status','--porcelain','--untracked-files=no'])
    evidence=ROOT/'build/phase6-evidence';evidence.mkdir(exist_ok=True)
    for result in results:
        name=result['log']
        for f in (name,name.replace('-gdb.log','-serial.log'),name.replace('-gdb.log','.gdb')):
            shutil.copy2(kernel/'build'/f,evidence/f)
        result['log']='phase6-evidence/'+name
    report=dict(kernel_commit=PIN,kernel=frozen,kernel_unchanged=True,sdk_archive=package_identity,
                guests=len(results),completed_lifetimes=sum(r['lifetimes'] for r in results),
                cases=results,consumer_build=build_report,
                elf_sha256={label:identity(retained/(label+'.elf'))['sha256'] for label in artifacts})
    (ROOT/'build/phase6-results.json').write_text(json.dumps(report,indent=2)+'\n')
    print('PASS: Phase 6',report['guests'],'real QEMU/GDB guests /',report['completed_lifetimes'],'completed lifetimes; package-only consumers')


if __name__=='__main__': main()
