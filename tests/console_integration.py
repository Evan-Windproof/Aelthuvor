#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Audit the actual Aelthuvor ELF with QMP PS/2 input and hardware observations.
Uses Liberums' GPL test transport/helpers, never its Console implementation.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import struct
import sys
from check_sdk import ROOT, check


def actions(terminal):
    from phase72 import SCANS
    result = []
    line = ''

    def key(code, after, contains=None, clear=False, final=False):
        parts = code.split('-')
        raw = [0xe0,0x4d,0xe0,0xcd] if code=='right' else [SCANS[p] for p in parts]+[SCANS[p]|0x80 for p in reversed(parts)]
        if final: raw = raw[:1] # EXIT releases input before the key-up is consumed.
        result.append(dict(key=code,raw=raw,line=after,contains=contains,clear=clear))

    def text(value):
        nonlocal line
        for ch in value:
            line += ch; key(ch,line)

    def command(value, expected=None):
        nonlocal line
        text(value); line=''; key('ret',line,expected,clear=value=='clear')

    command('help','help clear about status memtest exit')
    text('clx'); line=line[:-1]; key('backspace',line); text('ear')
    line=''; key('ret',line,clear=True)
    key('backspace',''); key('right','')
    key('caps_lock',''); key('h','H'); key('shift-e','He'); key('caps_lock','He')
    key('backspace','H'); key('backspace','')
    for i in range(65): key('a','a'*min(i+1,63))
    for i in range(64): key('backspace','a'*max(62-i,0))
    command('about','Aelthuvor: independent Ring 3 Library OS')
    command('status','ABI 1.2 features=119')
    command('memtest','memtest: PASS (96 + 4096 bytes, freed, trimmed)')
    command('nope','Unknown command')
    # Exercise actual framebuffer scrolling, retaining the Heap input buffer.
    for _ in range(8): command('about','Aelthuvor: independent Ring 3 Library OS')
    if terminal:
        text('exit'); key('ret','','Console exiting',final=True)
    return result

AUDIT = r'''python
seen=dict(events=0,irq1=0,returns=0,failed=0)
index=raw_index=last_sequence=0
finished=False
last_irq=None
expected=ACTIONS
mode=MODE
policy=POLICY
def c(field): return v('ael_console.'+field)
def framebuffer():
    b=bytes(gdb.selected_inferior().read_memory(0xb8000,4000))
    return '\n'.join(b[r*160:(r+1)*160:2].decode('ascii',errors='replace').rstrip() for r in range(25))
def pte(va):
    directory=words(d(0,'space.pages[0]'),1024)
    return words((directory[va>>22]&~4095)+((va>>12)&1023)*4,1)[0] if directory[va>>22]&1 else 0
def complete_input():
    global finished
    if mode=='persistent': finished=True
    elif mode=='fault': gdb.selected_inferior().write_memory(v('&ael_console_test_request'),struct.pack('<I',2))
    elif mode.startswith('revoke-'):
        mask=1 if mode=='revoke-vga' else 4
        gdb.selected_inferior().write_memory(v('&diagnostic_revoke'),struct.pack('<I',mask))
def send_next():
    if index<len(expected): inject([expected[index]['key']])
    else: complete_input()
def observed():
    global index,raw_index,last_sequence
    check(v('$cs')==0x1b and v('$ss')==0x23 and v('$cr3')==d(0,'space.pages[0]') and 0x083fc000<=v('$esp')<0x08400000,'Console CPL3/CR3/private stack')
    sequence=v('ael_console_sequence')
    if sequence==last_sequence: return
    check(sequence==last_sequence+1,'missed app observation')
    last_sequence=sequence
    error=v('ael_console_error')
    if error:
        check(mode.startswith('revoke-') and error==(-116&0xffffffff),'unexpected device failure')
        if mode=='revoke-vga':
            check(v('vga_device.active')==0 and v('keyboard.active')==1,'VGA revoke affected keyboard')
            check(c('vga.mapped')==0 and c('keyboard.owned')==1 and pte(0x09400000)==0,'VGA local invalidation/PTE removal')
        else:
            check(v('keyboard.active')==0 and v('vga_device.mapped')==1,'keyboard revoke affected VGA')
            check(c('keyboard.owned')==0 and c('vga.mapped')==1 and pte(0x09400000)&7==7,'keyboard local invalidation/VGA preserved')
        seen['failed']+=1
        return
    check(v('$eflags')&0x3200==0x200,'Console not preemptive or unsafe IOPL')
    check(pte(0x09400000)&0xfffff09f==0xb801f,'VGA mapping/cache permission')
    line=c('line')
    ram=pte(line)
    check(0x09000000<=line<0x09010000 and ram&7==7 and ram&~4095!=0xb8000,'Heap/device RAM mixed')
    check(v('ael_runtime.heap.policy')==policy and v('ael_runtime.heap.live')==1,'Console not using allocator')
    check(c('vga.handle.index')==0x10001 and c('keyboard.handle.index')==0x10002,'device namespaces')
    check(c('vga.handle.generation')==v('vga_device.cap.generation') and c('keyboard.handle.generation')==v('keyboard.cap.generation'),'device generation identity')
    check(v('keyboard.cap.owner_id')==1 and v('keyboard.cap.owner_epoch')==d(0,'epoch'),'keyboard owner')
    check(bytes(gdb.selected_inferior().read_memory(0xb8001,1))[0]==0x1f,'VGA attribute')
    if sequence==1:
        check('Aelthuvor Phase 3 Console' in framebuffer(),'wrong ELF or welcome')
        check(c('length')==0 and v('keyboard.count')==0,'input inherited')
        send_next(); return
    action=expected[index]
    check(v('ael_console_raw')==action['raw'][raw_index],'actual raw PS/2 byte mismatch')
    raw_index+=1; seen['events']+=1
    if raw_index<len(action['raw']): return
    actual=bytes(gdb.selected_inferior().read_memory(line,64)).split(b'\0')[0].decode()
    check(actual==action['line'] and c('length')==len(actual),'input editing/bound')
    if action['contains']: check(action['contains'] in framebuffer(),'missing Console output: '+action['contains'])
    if action['clear']: check(framebuffer().strip()=='ael>','clear failed')
    if action['contains'] and action['contains'].startswith('memtest: PASS'):
        check(c('memtests')==1 and v('ael_runtime.pages.owned')==1,'memtest did not free/trim')
    index+=1; raw_index=0; send_next()
def capture():
    global last_irq
    sp=v('$esp')+4
    cpu=words(sp,3)
    cpu=words(sp,5) if cpu[1]&3 else cpu
    regs=tuple(v('$'+r) for r in ('eax','ebx','ecx','edx','esi','edi','ebp','ds','es','fs','gs'))
    snapshot=(sp,cpu,regs,v('$cr3'))
    ordinal=v('keyboard_irqs')
    if ordinal==seen['irq1']-1:
        check(snapshot==last_irq,'duplicate IRQ1 stop changed frame')
        return
    check(ordinal==seen['irq1'],'missed IRQ1 capture')
    last_irq=snapshot
    check(v('$cs')==8 and v('$eflags')&0x200==0,'IRQ1 protection')
    if cpu[1]&3: check(sp==v('tss.esp0')-20,'IRQ1 trusted stack')
    seen['irq1']+=1
def returned():
    sp,cpu,regs,cr3=last_irq
    check(v('$esp')==sp and words(sp,len(cpu))==cpu,'IRQ1 frame changed')
    check(tuple(v('$'+r) for r in ('eax','ebx','ecx','edx','esi','edi','ebp','ds','es','fs','gs'))==regs and v('$cr3')==cr3,'IRQ1 damaged context')
    check(v('keyboard_irqs')==v('keyboard_eois'),'IRQ1 EOI missing')
    ordinal=v('keyboard_irqs')
    if ordinal==seen['returns']: return
    check(ordinal==seen['returns']+1,'missed IRQ1 return')
    seen['returns']+=1
class Observer(gdb.Breakpoint):
    def __init__(self,address,action):
        super().__init__('*'+address,type=gdb.BP_HARDWARE_BREAKPOINT,internal=True); self.action=action
    def stop(self):
        try:
            self.action()
            return finished
        except Exception:
            import traceback
            gdb.write(traceback.format_exc()); gdb.execute('quit 1')
end
'''


def run_console(qemu,gdb,image,mode,policy=0):
    from phase1 import require, wait_for, BUILD
    from phase2 import connect_and_run
    from phase3 import HELPERS
    from phase5 import GDB_HELPERS, symbols, headers
    from phase72 import vm, injector, INJECT
    blob=image.read_bytes(); names=symbols(image)
    note=next(h for h in headers(blob) if h[0]==4)
    require(struct.unpack_from('<III',blob,note[1]+24)==(1,2,0x75),'Console ELF note')
    label='ael-phase3-'+mode+('-best' if policy else '')
    append='external-console'+(' selftest=device-revoke' if mode.startswith('revoke-') else '')
    with vm(qemu,append,label,image) as (process,log,port,endpoint):
        with injector(endpoint) as control:
            script=f'add-symbol-file "{image}"\n'+HELPERS+GDB_HELPERS+INJECT.replace('CONTROL',repr(control))
            script+='hbreak *external_prepared\ncontinue\npython\n'
            script+=f"inspect_external({headers(blob)!r},{blob!r})\ncheck(v('external_load_result')==0,'Console rejected')\nend\ndelete breakpoints\n"
            script+=AUDIT.replace('ACTIONS',repr(actions(mode=='exit'))).replace('MODE',repr(mode)).replace('POLICY',str(policy))
            script+=f"python\nObserver('{names['ael_console_observed']:#x}',observed)\nObserver('keyboard_irq_capture',capture)\nObserver('keyboard_irq_return',returned)\nend\n"
            if mode!='persistent': script+='hbreak *external_reaped\n'
            script+='continue\npython\n'
            script+="check(index==len(expected) and seen['events']>0,'input audit incomplete')\ncheck(seen['irq1']>=seen['returns']>0,'no actual IRQ1 delivery: %r'%seen)\ncheck(v('irq_count')==v('eoi_count') and v('ticks')>0,'IRQ0/EOI')\ncheck(d(0,'preemptions')>0,'Console did not preempt/resume')\n"
            if mode=='persistent': script+="check(finished and c('running')==1,'persistent Console not running')\n"
            else:
                script+="inspect_external_reaped()\ncheck(d(0,'space.device_table')==0 and v('vga_device.active')==0 and v('vga_device.grant_id')==0,'VGA cleanup')\ncheck(v('keyboard.active')==0 and v('keyboard.enabled')==0 and v('keyboard.grant_id')==0 and v('keyboard.count')==0,'keyboard cleanup')\n"
                if mode=='fault': script+="check(d(0,'state')==4 and d(0,'fault_vector')==6,'diagnostic UD fault missing')\n"
                else: script+=f"check(d(0,'state')==3 and d(0,'exit_status')=={98 if mode.startswith('revoke-') else 0},'wrong Console exit')\n"
                if mode.startswith('revoke-'): script+="check(seen['failed']==1,'device invalidation was not observed')\n"
            screenshot=ROOT/'build'/f'{label}.png'
            script+=f"query('screendump',{{'filename':{str(screenshot)!r},'format':'png'}})\nprint('PASS: Aelthuvor actual ELF/Heap/PS2/VGA/IRQ',seen)\nend\ndetach\nquit\n"
            output=connect_and_run(process,log,port,gdb,script,label)
            require('PASS: Aelthuvor actual' in output,'GDB assertions did not complete')
            require(screenshot.read_bytes().startswith(b'\x89PNG'),'missing rendered screenshot')
            if mode!='persistent': wait_for(process,log,lambda s:'Phase 5: external execution completed' in s,'Console cleanup')
    print('PASS:',label,'QMP -> PS/2 -> IRQ1 -> Aelthuvor -> VGA')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--liberums',type=Path,required=True)
    parser.add_argument('--qemu',default='qemu-system-i386');parser.add_argument('--gdb',default='gdb')
    args=parser.parse_args(); kernel=args.liberums.resolve()
    check(kernel,working=False)
    subprocess.run(['make','-C',str(kernel),'-j2'],check=True)
    elf=kernel/'build/liberums.elf'
    before=hashlib.sha256(elf.read_bytes()).hexdigest(); mtime=elf.stat().st_mtime_ns
    # This second invocation demonstrates application rebuilds do not relink the kernel.
    subprocess.run(['make','-B','-C',str(ROOT),'console','build/console-best.elf','build/console-test.elf','build/device-probe.elf'],check=True)
    sys.path.append(str(kernel/'tests'))
    from phase73 import lifecycle
    for mode,file,policy in [('persistent','ael-console',0),('exit','ael-console',0),('exit','console-best',1),
                              ('fault','console-test',0),('revoke-vga','ael-console',0),('revoke-keyboard','ael-console',0)]:
        run_console(args.qemu,args.gdb,ROOT/'build'/f'{file}.elf',mode,policy)
    image=ROOT/'build/ael-console.elf'
    lifecycle(args.qemu,args.gdb,'ael-console-no-grant',image,'external preempt',exit_status=97)
    lifecycle(args.qemu,args.gdb,'console-readonly',image,'external preempt external-vga-readonly external-keyboard',exit_status=97)
    lifecycle(args.qemu,args.gdb,'device-probe',ROOT/'build/device-probe.elf','external-console')
    assert hashlib.sha256(elf.read_bytes()).hexdigest()==before and elf.stat().st_mtime_ns==mtime,'Console build relinked kernel'
    evidence=dict(kernel_commit=subprocess.check_output(['git','-C',str(kernel),'rev-parse','HEAD'],text=True).strip(),
                  kernel_sha256=before,console_sha256=hashlib.sha256(image.read_bytes()).hexdigest(),
                  console_best_sha256=hashlib.sha256((ROOT/'build/console-best.elf').read_bytes()).hexdigest(),
                  qemu_guests=9,completed_lifetimes=11,persistent_guests=1)
    (ROOT/'build/phase3-results.json').write_text(json.dumps(evidence,indent=2)+'\n')
    print('PASS: Phase 3: 9 guests, 11 completed lifetimes + 1 persistent Console; kernel ELF unchanged')

if __name__=='__main__':
    try: main()
    except (OSError,RuntimeError,ValueError,subprocess.SubprocessError) as error:
        print('FAIL:',error,file=sys.stderr);sys.exit(1)
