"""Read-only GDB observer of the installed compiler's scratch searches.

Load only through the replay driver, which pins the compiler and instruction
offsets. No inferior function calls, register writes or compiler-state edits.
"""
import hashlib
import json
import os
from pathlib import Path
import gdb

expected = os.environ['PEEP2_COMPILER_SHA256']
PINS = {
    '80a79e520ca77fb1efa3bbd5ac4d927d075d6c1fdc6dd908fd83b437cc47febc': (0x081f07fc, 0x0834f064),
    'b778f44bd1a5e8184ca34b11c9701d0a8d67d15406282c1a204237d03c82082d': (0x0823c710, 0x083b4924),
}
if expected not in PINS:
    raise RuntimeError('unknown compiler pin')
if hashlib.sha256(Path(gdb.current_progspace().filename).read_bytes()).hexdigest() != expected:
    raise RuntimeError('compiler identity differs from replay pin')

sequence = 0
active = None
matches = {}
match_sequence = 0
word = gdb.lookup_type('unsigned long').pointer()

def value(expr):
    return gdb.parse_and_eval(expr)

def integer(expr):
    return int(value(expr))

def bitmap(pointer):
    pointer = pointer.cast(word)
    return [int(pointer[i]) for i in range(2)]

def cursor():
    return integer('*(int*)0x%x' % PINS[expected][1])

def identity():
    decl = value('cfun->decl')
    name = decl['decl']['name']['identifier']['id']['str'].string()
    asm = decl['decl']['assembler_name']
    return name, asm['identifier']['id']['str'].string() if int(asm) else name

def emit(kind, **fields):
    print('PEEP2 ' + json.dumps({'kind': kind, **fields}, sort_keys=True))

def pattern(pointer, depth=0):
    node = pointer.dereference()
    code = str(node['code'])
    result = {'code': code, 'mode': str(node['mode'])}
    if code == 'REG':
        result['reg'] = int(node['u']['fld'][0]['rtuint'])
    elif code == 'CONST_INT':
        result['value'] = int(node['u']['hwint'][0])
    elif depth < 5 and code == 'PARALLEL':
        vector = node['u']['fld'][0]['rtvec'].dereference()
        count = int(vector['num_elem'])
        if count > 8:
            raise RuntimeError('RTL vector outside observer bound')
        result['operands'] = [pattern(vector['elem'][i], depth+1) for i in range(count)]
    elif depth < 5 and code in ('SET', 'PLUS', 'COMPARE', 'MEM', 'SUBREG', 'ZERO_EXTEND'):
        count = 2 if code in ('SET', 'PLUS', 'COMPARE') else 1
        result['operands'] = [pattern(node['u']['fld'][i]['rtx'], depth+1) for i in range(count)]
    return result

class MatchFinished(gdb.FinishBreakpoint):
    def __init__(self, frame, key, event):
        super().__init__(frame, internal=True)
        self.key, self.event = key, event

    def stop(self):
        result = self.return_value
        if result is None:
            raise RuntimeError('enclosing match result unavailable')
        emit('match_return', match=self.event['match'], uid=self.event['uid'],
             searches=self.event['searches'], replacement_returned=bool(int(result)),
             cursor_after=cursor())
        del matches[self.key]
        return False

class Finished(gdb.FinishBreakpoint):
    def __init__(self, event):
        super().__init__(gdb.newest_frame(), internal=True)
        self.event = event

    def stop(self):
        global active
        result = self.return_value
        if result is None:
            raise RuntimeError('scratch return value unavailable')
        selected = None
        if int(result):
            node = result.dereference()
            if str(node['code']) != 'REG':
                raise RuntimeError('scratch search returned non-register')
            selected = int(node['u']['fld'][0]['rtuint'])
        emit('return', search=self.event['search'], selected=selected,
             cursor_after=cursor(), reserved_after=bitmap(self.event['reserved']))
        active = None
        return False

class Enter(gdb.Breakpoint):
    def stop(self):
        global sequence, active, match_sequence
        if active is not None:
            raise RuntimeError('unexpected nested scratch search')
        sequence += 1
        if sequence == 1:
            emit('allocation_order', registers=[integer('reg_alloc_order[%d]' % i) for i in range(53)])
        start, end = integer('*(int*)($ebp+8)'), integer('*(int*)($ebp+12)')
        slot = (integer('peep2_current') + start) % 5
        insn = value('peep2_insn_data[%d].insn' % slot)
        uid = int(insn.dereference()['u']['fld'][0]['rtint'])
        function, assembler_name = identity()
        caller = gdb.newest_frame().older()
        generator = caller.name()
        while caller is not None and not (caller.name() or '').startswith('peephole2_'):
            caller = caller.older()
        if caller is None or caller.name() == 'peephole2_optimize':
            raise RuntimeError('generated matcher frame not found')
        key = int(caller.read_register('ebp'))
        if key not in matches:
            match_sequence += 1
            caller_insn = caller.read_var('insn')
            match = {'match': match_sequence,
                     'uid': int(caller_insn.dereference()['u']['fld'][0]['rtint']),
                     'searches': []}
            matches[key] = match
            MatchFinished(caller, key, match)
        matches[key]['searches'].append(sequence)
        reserved = value('*(unsigned long**)($ebp+24)')
        active = {'search': sequence, 'reserved': reserved}
        emit('enter', search=sequence, function=function, assembler_name=assembler_name,
             uid=uid, from_insn=start, to_insn=end, slot=slot,
             match=matches[key]['match'], match_uid=matches[key]['uid'],
             generator=generator,
             input_pattern=pattern(insn.dereference()['u']['fld'][5]['rtx']),
             constraint=value('*(char**)($ebp+16)').string(),
             mode=integer('*(int*)($ebp+20)'), cursor_before=cursor(),
             reserved_before=bitmap(reserved))
        Finished(active)
        return False

class Candidate(gdb.Breakpoint):
    def stop(self):
        if active is None:
            raise RuntimeError('candidate without search')
        reg = integer('$ebx')
        cls = integer('*(int*)($ebp-36)')
        emit('candidate', search=active['search'], raw_reg=integer('$esi'), reg=reg,
             live=bitmap(value('$ebp-24')), reserved=bitmap(active['reserved']),
             register_class=cls, class_contents=bitmap(value('&reg_class_contents[%d]' % cls)),
             fixed=integer('fixed_regs[%d]' % reg),
             call_used=integer('call_used_regs[%d]' % reg),
             ever_live=integer('regs_ever_live[%d]' % reg),
             frame_protected=reg in (6, 20) and
                 (not integer('reload_completed') or bool(integer('frame_pointer_needed'))))
        return False

class ModeResult(gdb.Breakpoint):
    def stop(self):
        if active is None:
            raise RuntimeError('mode check without search')
        emit('mode_check', search=active['search'], reg=integer('$ebx'), allowed=integer('$eax'))
        return False

base = integer('&peep2_find_free_register')
if base != PINS[expected][0]:
    raise RuntimeError('scratch-search function address differs from pin')
Enter('*0x%x' % (base+9), internal=True)
Candidate('*0x%x' % (base+428), internal=True)
ModeResult('*0x%x' % (base+509), internal=True)
emit('observer', compiler_sha256=expected, source_or_rtl_mutation=False,
     inferior_function_calls=False)
