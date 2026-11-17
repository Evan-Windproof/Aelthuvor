#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Same unmodified Phase 8 kernel, exported-SDK apps and an unrelated LibOS."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
from check_sdk import ROOT, check

AUDIT = r'''
seen={i:set() for i in used}
irq_seen=set()
ram={}
injected=False
probed=False
objects=[]
def stage(i,kind):
    global injected,probed
    # Different CR3s may map different functions at the same linear PC.
    # Python breakpoint callbacks can run before the GDB condition filter.
    if v('current_domain')==0 or v('current_domain->id')!=i: return
    user(i)
    n=words(v('$esp')+4,1)[0]
    seen[i].add(n)
    if n==2:
        check(dd(i,'resource_count')==1,'program did not acquire one real RAM page')
        va=0x09000000 if kind=='memory' else 0x0900f000
        pa=pt(i,va)&~4095
        check(pa and pt(i,va)&7==7,'bad user resource PTE')
        ram[i]=pa
        if kind=='memory':
            check(v('ael_runtime.heap.live')==1 and v('ael_runtime.pages.owned')==1,'SDK allocator state')
            check(v('ael_runtime.heap.policy')==POLICY,'first/best policy selection')
            check(v('memory_payload')==va,'unexpected application allocation VA')
            check(words(pa,1024)==tuple(0xa4e10000|j for j in range(1024)),'SDK application data')
        else:
            check(v('reference_pool.address')==va and v('reference_pool.live')==64 and v('reference_pool.available')==0,'independent object-stack policy')
            actual=bytes(gdb.selected_inferior().read_memory(pa,4096))
            check(actual==bytes((63-j//64)^(j%64)^0x6d for j in range(4096)),'independent pool data')
            if DIAGNOSTIC: check(v('reference_stale_result')==(-116&0xffffffff),'old generation not rejected')
        slots=[r for r in range(16) if v('resources[%d].state'%r)==1 and v('resources[%d].cap.owner_id'%r)==i]
        check(len(slots)==1,'owner count')
        r=slots[0]
        check(v('resources[%d].physical_address'%r)==pa and v('resources[%d].cap.owner_epoch'%r)==dd(i,'epoch'),'ownership epoch/PFN')
        check(v('resources[%d].cap.rights'%r)==47 and v('resources[%d].references'%r)==1,'resource rights/binding')
        if DIAGNOSTIC and len(ram)==2 and not injected:
            # Only scalar inputs in the REFERENCE APPLICATION are written.
            # Actual syscalls/MMU must still reject unauthorized access.
            refid=2
            r=next(r for r in range(16) if v('resources[%d].state'%r)==1 and v('resources[%d].cap.owner_id'%r)==1)
            uw(refid,RS['reference_peer_index'],r+1)
            uw(refid,RS['reference_peer_generation'],v('resources[%d].cap.generation'%r))
            uw(refid,RS['reference_peer_address'],ram[1])
            uw(refid,RS['reference_command'],2 if FAULT else 1)
            injected=True
    elif n==3:
        if kind=='memory': check(v('ael_runtime.heap.live')==0 and dd(i,'resource_count')==0,'SDK app cleanup')
        else: check(v('reference_pool.live')==0 and v('reference_pool.available')==64,'pool did not reclaim its objects')
    elif n==4:
        check(kind=='reference' and v('reference_query_result')==(-1&0xffffffff),'foreign handle not rejected')
        probed=True
    if len(ram)==2:
        check(ram[1]!=ram[2],'two LibOS instances share resource RAM')
        check(pt(1,0x0900f000)==0 and pt(2,0x09000000)==0,'resource VA isolation')
def irq():
    i=v('current_domain->id')
    if i not in roles: return
    cpu=inspect_trap(i-1)
    check(cpu[8]==32 and cpu[12]&0x3200==0x200,'not a protected user PIT IRQ')
    irq_seen.add(i)
class Observer(gdb.Breakpoint):
    def __init__(self,address,action,condition=None):
        super().__init__('*'+str(address),type=gdb.BP_HARDWARE_BREAKPOINT,internal=True)
        self.action=action
        if condition: self.condition=condition
    def stop(self):
        try: self.action()
        except Exception:
            import traceback
            gdb.write(traceback.format_exc());gdb.execute('quit 1')
        return False
for i,kind in roles.items():
    address=MS['memory_observed'] if kind=='memory' else RS['reference_observed']
    objects.append(Observer(hex(address),lambda i=i,kind=kind:stage(i,kind),'current_domain->id == %d'%i))
if roles: objects.append(Observer('domain_preempt',irq))
entry_seen=set()
entry_bps={}
def entered(i):
    if v('current_domain')==0 or v('current_domain->id')!=i: return
    user(i)
    entry_seen.add(i)
    entry_bps[i].enabled=False
if not roles:
    for i,address in zip(used,ENTRIES):
        entry_bps[i]=Observer(hex(address),lambda i=i:entered(i))
finished()
if not roles: check(entry_seen==set(used),'missing actual CPL3 application entry')
for i,status in EXPECTED.items():
    check(dd(i,'last_state')==status[0] and dd(i,'exit_status')==status[1],'wrong terminal state/status for %d'%i)
for i,kind in roles.items():
    check(2 in seen[i],'policy never ran in Ring3')
    if not (FAULT and kind=='reference'):
        check(3 in seen[i] and i in irq_seen and dd(i,'preemptions')>0,'no preempt/resume/data cleanup proof')
if DIAGNOSTIC:
    check(injected,'adversarial input not posted while both resources live')
    if FAULT:
        check(dd(2,'fault_vector')==14 and dd(2,'fault_error')==5 and dd(2,'fault_address')==ram[1],'foreign physical alias not MMU protected')
        check(dd(2,'fault_eip')==RS['reference_fault_load'],'wrong fault instruction')
        check(dd(1,'last_state')==3 and dd(1,'exit_status')==0,'reference fault damaged SDK application')
    else: check(probed,'foreign query result never observed')
print('POLICIES stages=',seen,'PIT domains=',irq_seen)
'''

def case(args,label,files,kinds=None,policy=0,diagnostic=False,fault=False,expected_load=None,terminal=None):
    from phase1 import require,wait_for
    from phase2 import connect_and_run
    from phase3 import HELPERS
    from phase5 import headers,symbols
    from phase72 import vm
    from phase8 import COMMON
    expected_load=expected_load or [0]*len(files)
    images=[(headers(f.read_bytes()),f.read_bytes()) if r==0 else ([],b'') for f,r in zip(files,expected_load)]
    MS=symbols(files[0])
    RS=next((symbols(f) for f in files if 'reference_observed' in symbols(f)),{})
    roles={i+1:k for i,k in enumerate(kinds or []) if k}
    expected=terminal or {i+1:(3,0) for i,r in enumerate(expected_load) if r==0}
    with vm(args.qemu,'multi-external','ael-phase4-'+label,','.join(map(str,files))) as (p,log,port,_):
        commands=HELPERS
        for f in files: commands+='add-symbol-file "'+str(f)+'"\n'
        commands+='python\n'+COMMON.replace('SYMBOLS','{}')
        commands+=f'used=ready({expected_load!r},{images!r})\n'
        commands+="if live_bp is not None: live_bp.delete(); live_bp=None\n"
        commands+=f'MS={MS!r}\nRS={RS!r}\nroles={roles!r}\n'
        commands+=AUDIT.replace('POLICY',str(policy)).replace('DIAGNOSTIC',repr(diagnostic)).replace('FAULT',repr(fault)).replace('EXPECTED',repr(expected)).replace('ENTRIES',repr([symbols(f)['_start'] for f,r in zip(files,expected_load) if r==0]))
        commands+=f"print('PASS: Phase4 {label}')\nend\ndetach\nquit\n"
        output=connect_and_run(p,log,port,args.gdb,commands,'ael-phase4-'+label)
        require('PASS: Phase4 '+label in output,'assertions not completed')
        wait_for(p,log,lambda t:'Phase 8: multi external execution completed' in t,'multi completion')
        require('PANIC' not in log.read_text(),'kernel panic')
        if label=='hello': require('Hello from the Aelthuvor Application SDK' in log.read_text(),'Hello output not executed')
        if label=='external-application': require('Edited independent application passed' in log.read_text(),'edited consumer artifact not executed')
    print('PASS: Phase4',label)


def sdk_console(args):
    from phase1 import require,wait_for
    from phase2 import connect_and_run
    from phase3 import HELPERS
    from phase5 import headers,symbols
    from phase72 import vm,injector,INJECT
    from phase8 import COMMON
    image=ROOT/'build/apps/console/app.elf'; blob=image.read_bytes(); names=symbols(image)
    actions=[(['h','e','l','p','ret'],'help','help clear about status memtest exit'),
        (['c','l','x','backspace','e','a','r','ret'],'clear',None),
        (['a','b','o','u','t','ret'],'about','independent Ring 3'),
        (['s','t','a','t','u','s','ret'],'status','ABI 1.2 API=1.0 live=1'),
        (['m','e','m','t','e','s','t','ret'],'memtest','memtest: PASS'),
        (['n','o','p','e','ret'],'nope','Unknown command'),
        (['e','x','i','t','ret'],'exit',None)]
    with vm(args.qemu,'multi-external multi-vga=1 multi-keyboard=1','ael-phase4-sdk-console',image) as (p,log,port,endpoint):
        with injector(endpoint) as control:
            commands=HELPERS+f'add-symbol-file "{image}"\n'+INJECT.replace('CONTROL',repr(control))
            commands+='python\n'+COMMON.replace('SYMBOLS','{}')
            commands+=f'used=ready([0],{[(headers(blob),blob)]!r})\n'
            commands+="if live_bp is not None: live_bp.delete(); live_bp=None\n"
            commands+=f'actions={actions!r}\n'
            commands+=r'''
index=0
last=-1
stages=[]
def observed():
    global index,last
    user(1)
    sequence=v('sdk_console_sequence')
    stage=words(v('$esp')+4,1)[0]
    if sequence==last: return
    check(sequence==last+1,'SDK console skipped observation')
    last=sequence;stages.append(stage)
    check(pt(1,0x09400000)&0xfffff09f==0xb801f,'SDK VGA PTE')
    line=v('sdk_console_line')
    check(0x09000000<=line<0x09010000 and pt(1,line)&7==7,'SDK console did not allocate input')
    check(v('ael_runtime.heap.live')==1,'SDK Console leaked memory')
    if stage==0:
        inject(actions[0][0]);return
    actual=bytes(gdb.selected_inferior().read_memory(line,64)).split(b'\0')[0].decode()
    check(actual==actions[index][1],'SDK readline/editing changed command')
    if stage==1: return
    check(stage==2,'wrong SDK stage')
    b=bytes(gdb.selected_inferior().read_memory(0xb8000,4000))
    screen='\n'.join(b[r*160:(r+1)*160:2].decode().rstrip() for r in range(25))
    wanted=actions[index][2]
    if wanted: check(wanted in screen,'missing SDK command output')
    if actual=='clear': check(not screen.strip(),'SDK clear')
    if actual=='memtest':
        check(v('ael_runtime.pages.owned')==1,'SDK memtest did not trim')
        query('screendump',{'filename':SCREENSHOT,'format':'png'})
    index+=1
    if index<len(actions): inject(actions[index][0])
class Observation(gdb.Breakpoint):
    def stop(self):
        try: observed()
        except Exception:
            import traceback
            gdb.write(traceback.format_exc());gdb.execute('quit 1')
        return False
'''.replace('SCREENSHOT',repr(str(ROOT/'build/phase4-sdk-console.png')))
            commands+=f"observation=Observation('*{names['sdk_console_observed']:#x}',type=gdb.BP_HARDWARE_BREAKPOINT,internal=True)\n"
            commands+="finished()\ncheck(index==len(actions),'SDK console input incomplete')\ncheck(dd(1,'last_state')==3 and dd(1,'exit_status')==0,'SDK Console did not clean up')\ncheck(dd(1,'preemptions')>0 and v('keyboard_irqs')>0,'no real IRQs')\nprint('PASS: Phase4 SDK Console PS2/commands/Heap/devices/EXIT')\nend\ndetach\nquit\n"
            output=connect_and_run(p,log,port,args.gdb,commands,'ael-phase4-sdk-console')
            require('PASS: Phase4 SDK Console' in output,'SDK Console not validated')
            wait_for(p,log,lambda t:'Phase 8: multi external execution completed' in t,'SDK console completion')
    print('PASS: Phase4 SDK Console')


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--liberums',type=Path,required=True);p.add_argument('--qemu',default='qemu-system-i386');p.add_argument('--gdb',default='gdb')
    args=p.parse_args();kernel=args.liberums.resolve();check(kernel,working=False)
    subprocess.run(['make','-C',str(kernel),'-j2'],check=True)
    elf=kernel/'build/liberums.elf';before=(elf.stat().st_mtime_ns,hashlib.sha256(elf.read_bytes()).hexdigest())
    # Rebuild/edit the OUTSIDE consumer after freezing the kernel identity.
    # The resulting artifacts below are the exact ones executed by the guests.
    subprocess.run([sys.executable,str(ROOT/'tests/sdk_builds.py')],cwd=ROOT,check=True)
    sys.path.append(str(kernel/'tests'))
    hello=ROOT/'build/apps/hello/app.elf'; memory=ROOT/'build/apps/memory/app.elf'; best=ROOT/'build/apps/memory-best/app.elf'
    ref=ROOT/'build/independent-isolated.elf'; diag=ROOT/'build/independent-diagnostic/reference.elf'
    third=ROOT/'build/third-party-app.elf'
    case(args,'hello',[hello])
    case(args,'external-application',[third])
    case(args,'memory',[memory],['memory'])
    case(args,'memory-best',[best],['memory'],policy=1)
    case(args,'independent',[ref],['reference'])
    case(args,'two-libos',[memory,ref],['memory','reference'])
    case(args,'two-libos-query',[memory,diag],['memory','reference'],diagnostic=True)
    case(args,'two-libos-fault',[memory,diag],['memory','reference'],diagnostic=True,fault=True,terminal={1:(3,0),2:(4,0)})
    case(args,'sdk-leak',[ROOT/'build/apps/leak/app.elf'],terminal={1:(3,95)})
    case(args,'sdk-fault-isolation',[ROOT/'build/apps/fault/app.elf',ref],[None,'reference'],terminal={1:(4,0),2:(3,0)})
    # Reject a cooperative ELF in Phase 8 without blocking the other LibOS.
    case(args,'profile-rejected',[ROOT/'build/apps/hello-coop/app.elf',ref],expected_load=[-93,0],terminal={1:(3,0)})
    sdk_console(args)
    from phase73 import lifecycle
    lifecycle(args.qemu,args.gdb,'app-sdk-cooperative',ROOT/'build/apps/hello-coop/app.elf','external')
    assert before==(elf.stat().st_mtime_ns,hashlib.sha256(elf.read_bytes()).hexdigest()),'application testing relinked kernel'
    files=[hello,memory,best,ref,diag,third,ROOT/'build/apps/console/app.elf']
    (ROOT/'build/phase4-results.json').write_text(json.dumps(dict(kernel_commit=subprocess.check_output(['git','-C',str(kernel),'rev-parse','HEAD'],text=True).strip(),
        kernel_sha256=before[1],kernel_unchanged=True,guests=13,completed_lifetimes=18,
        elf_sha256={str(f.relative_to(ROOT)):hashlib.sha256(f.read_bytes()).hexdigest() for f in files}),indent=2)+'\n')
    print('PASS: Phase4 13 QEMU guests / 18 completed lifetimes; one unchanged Phase 8 kernel')

if __name__=='__main__':
    try:main()
    except (OSError,ValueError,RuntimeError,subprocess.SubprocessError) as e:
        print('FAIL:',e,file=sys.stderr);sys.exit(1)
