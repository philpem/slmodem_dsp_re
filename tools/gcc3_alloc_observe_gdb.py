"""Read-only GDB callbacks for the installed Gentoo GCC3 cc1.

Run through gcc3_alloc_observe.py, not the ordinary Python interpreter.
Breakpoints observe execution; they never call into or modify the inferior.
Source line locations are bounded to this compiler's DWARF build provenance.
"""
import gdb
import json
from pathlib import Path

events = []
active = []

def value(expression):
    try:
        result = gdb.parse_and_eval(expression)
        if result.is_optimized_out:
            return {'unavailable': 'optimized out'}
        try:
            return int(result)
        except (ValueError, gdb.error):
            return str(result)
    except gdb.error as exc:
        return {'unavailable': str(exc)}

def name():
    try:
        return gdb.parse_and_eval('current_function_decl->decl.name->identifier.id.str').string()
    except gdb.error:
        return None

def mask(expression):
    words = [value(expression+'[%d]'%i) for i in range(2)]
    if all(isinstance(word, int) for word in words):
        return [r for r in range(53) if words[r//32] & (1 << (r%32))]
    return {'unavailable': words}

class Done(gdb.FinishBreakpoint):
    def __init__(self, event):
        super().__init__(gdb.newest_frame(), internal=True)
        self.event = event
    def stop(self):
        self.event['return'] = int(self.return_value) if self.return_value is not None else {'unavailable':'return value'}
        active.remove(self.event)
        return False

class Allocation(gdb.Breakpoint):
    def stop(self):
        if name() != 'updateAlpha':
            return False
        event = {'kind': 'allocation', 'args': {k:value(k) for k in ('class','mode','qtyno','accept_call_clobbered','just_try_suggested','born_index','dead_index')}}
        event['quantity'] = {k:value('qty[qtyno].'+k) for k in ('n_refs','freq','birth','death','size','n_calls_crossed','first_reg','min_class','alternate_class','phys_reg')}
        event['members'] = []
        reg = event['quantity']['first_reg']
        while isinstance(reg,int) and reg >= 0:
            if reg in event['members']:
                raise RuntimeError('cyclic quantity membership')
            event['members'].append(reg)
            reg = value('reg_next_in_qty[%d]'%reg)
        event['copy_suggestions'] = mask('qty_phys_copy_sugg[qtyno]')
        event['arithmetic_suggestions'] = mask('qty_phys_sugg[qtyno]')
        events.append(event)
        active.append(event)
        Done(event)
        return False

class Exclusions(gdb.Breakpoint):
    def stop(self):
        if name() == 'updateAlpha' and active and 'exclusions' not in active[-1]:
            active[-1]['exclusions'] = {'used':mask('used'),'first_used':mask('first_used')}
        return False

class Spill(gdb.Breakpoint):
    def stop(self):
        if name() != 'updateAlpha':
            return False
        event={'kind':'spill','frame':gdb.newest_frame().name(),'args':{}}
        block=gdb.newest_frame().block()
        for symbol in block:
            if symbol.is_argument:
                event['args'][symbol.name]=value(symbol.name)
        event['homes']={r:value('reg_renumber[%d]'%r) for r in (71,73,74,77,67)}
        events.append(event)
        return False

class HomeChange(gdb.Breakpoint):
    def __init__(self, reg):
        self.reg=reg
        super().__init__('reg_renumber[%d]'%reg,type=gdb.BP_WATCHPOINT,internal=True)
    def stop(self):
        frames=[]
        frame=gdb.newest_frame()
        while frame and len(frames)<5:
            frames.append(frame.name())
            frame=frame.older()
        event={'kind':'home_change','reg':self.reg,'home':value('reg_renumber[%d]'%self.reg),'frames':frames}
        if frames[0]=='find_reg' and 'global_alloc' in frames:
            event['eviction']={k:value(k) for k in ('num','regno','tmp1','tmp2','allocno[num].reg','allocno[num].freq','allocno[num].live_length','local_reg_freq[regno]','local_reg_live_length[regno]')}
        events.append(event)
        return False

class Global(gdb.Breakpoint):
    def stop(self):
        if name()=='updateAlpha':
            watches=[]
            for reg in (73,74,77):
                watches.append(HomeChange(reg))
            GlobalDone(watches)
        return False

class GlobalDone(gdb.FinishBreakpoint):
    def __init__(self,watches):
        super().__init__(gdb.newest_frame(),internal=True)
        self.watches=watches
    def stop(self):
        for watch in self.watches:
            watch.delete()
        return False

Allocation('find_free_reg', internal=True)
Exclusions('/build/gcc-3.4.2/gcc/local-alloc.c:2206', internal=True)
Spill('spill_hard_reg', internal=True)
Global('global_alloc', internal=True)
exit_codes=[]
gdb.events.exited.connect(lambda event:exit_codes.append(getattr(event,'exit_code',None)))
gdb.execute('run')
if exit_codes != [0]:
    raise RuntimeError('compiler did not exit successfully: %r'%exit_codes)
Path('observe.json').write_text(json.dumps(events,indent=2)+'\n')
print('observed %d allocation/spill events'%len(events))
