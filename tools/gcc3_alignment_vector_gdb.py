"""Observe installed cc1plus stack requests; no inferior calls or state writes."""
import json
from pathlib import Path
import gdb

TARGETS = {'generateJdPhase', 'generateV92Jd', 'generateTRN1d'}
events, errors, pending = [], [], []
counts = {}
frames = []
callees = []

def integer(expression):
    value = gdb.parse_and_eval(expression)
    assert not value.is_optimized_out, expression
    return int(value)

def enum(value, typename):
    return str(gdb.Value(value).cast(gdb.lookup_type(typename)))

def rtl(value, depth=0):
    assert value is not None and int(value)
    node = value.dereference()
    code = enum(int(node['code']), 'enum rtx_code')
    mode = enum(int(node['mode']), 'enum machine_mode')
    result = {'code': code, 'mode': mode}
    if code == 'REG': result['regno'] = int(node['u']['fld'][0]['rtint'])
    elif code == 'CONST_INT': result['value'] = int(node['u']['hwint'][0])
    elif code in ('MEM', 'PLUS') and depth < 4:
        result['operands'] = [rtl(node['u']['fld'][i]['rtx'], depth+1) for i in range(2 if code == 'PLUS' else 1)]
    return result

class Done(gdb.FinishBreakpoint):
    def __init__(self, event):
        super().__init__(gdb.newest_frame(), internal=True)
        self.event = event
    def stop(self):
        try:
            self.event['returned'] = rtl(self.return_value)
            self.event['frame_after'] = integer('((struct function *)%d)->x_frame_offset' % self.event['function_pointer'])
            pending.remove(self.event)
        except Exception as exc:
            errors.append(str(exc))
        return False

class Stack(gdb.Breakpoint):
    def stop(self):
        try:
            stack = integer('$esp')
            arg = lambda offset: integer('*(unsigned int*)%d' % (stack+offset))
            signed = lambda value: value-(1 << 32) if value & (1 << 31) else value
            function = arg(16)
            name = gdb.parse_and_eval('((struct function *)%d)->decl->decl.name->identifier.id.str' % function).string()
            counts[name] = counts.get(name, 0)+1
            if name not in TARGETS:
                return False
            frames = []
            frame = gdb.newest_frame()
            while frame and len(frames) < 10:
                frames.append(frame.name())
                frame = frame.older()
            event = {'function': name, 'function_pointer': function,
                     'mode': enum(arg(4), 'enum machine_mode'), 'size': signed(arg(8)),
                     'align': signed(arg(12)), 'frame_before': integer('((struct function *)%d)->x_frame_offset' % function),
                     'callers': frames}
            assert 0 <= event['size'] <= 1024 and -1 <= event['align'] <= 128
            events.append(event)
            pending.append(event)
            Done(event)
        except Exception as exc:
            errors.append(str(exc))
        return False

class FrameDone(gdb.FinishBreakpoint):
    def __init__(self, event, pointer):
        super().__init__(gdb.newest_frame(), internal=True)
        self.event, self.pointer = event, pointer
    def stop(self):
        try:
            self.event['layout'] = {field: integer('((struct ix86_frame *)%d)->%s' % (self.pointer, field))
                                   for field in ['nregs', 'padding1', 'padding2', 'va_arg_size', 'outgoing_arguments_size',
                                                 'to_allocate', 'frame_pointer_offset', 'stack_pointer_offset', 'save_regs_using_mov']}
            pending.remove(self.event)
        except Exception as exc:
            errors.append(str(exc))
        return False

class Frame(gdb.Breakpoint):
    def stop(self):
        try:
            name = gdb.parse_and_eval('cfun->decl->decl.name->identifier.id.str').string()
            if name not in TARGETS:
                return False
            pointer = integer('*(unsigned int*)($esp+4)')
            event = {'function': name, 'locals_size': -integer('cfun->x_frame_offset'),
                     'stack_alignment_needed': integer('cfun->stack_alignment_needed'),
                     'preferred_stack_boundary': integer('cfun->preferred_stack_boundary'),
                     'outgoing_args_size': integer('cfun->outgoing_args_size'),
                     'reload_completed': integer('reload_completed'),
                     'frame_pointer_needed': integer('frame_pointer_needed')}
            frames.append(event)
            pending.append(event)
            FrameDone(event, pointer)
        except Exception as exc:
            errors.append(str(exc))
        return False

class CalleeDone(gdb.FinishBreakpoint):
    def __init__(self, event):
        super().__init__(gdb.newest_frame(), internal=True)
        self.event = event
    def stop(self):
        try:
            value = self.return_value
            self.event['known_incoming_boundary'] = int(value.dereference()['preferred_incoming_stack_boundary']) if value is not None and int(value) else None
            pending.remove(self.event)
        except Exception as exc:
            errors.append(str(exc))
        return False

class Callee(gdb.Breakpoint):
    def stop(self):
        try:
            if gdb.parse_and_eval('current_function_decl->decl.name->identifier.id.str').string() not in TARGETS:
                return False
            decl = integer('*(unsigned int*)($esp+4)')
            event = {'caller': gdb.parse_and_eval('current_function_decl->decl.name->identifier.id.str').string(), 'callee': gdb.parse_and_eval('((tree)%d)->decl.name->identifier.id.str' % decl).string(),
                     'asm_written': integer('((tree)%d)->common.asm_written_flag' % decl)}
            callees.append(event)
            pending.append(event)
            CalleeDone(event)
        except Exception as exc:
            errors.append(str(exc))
        return False

assert gdb.lookup_type('long').sizeof == 4, 'unsupported host-wide integer ABI'
Stack('*assign_stack_local_1', internal=True)
Frame('*ix86_compute_frame_layout', internal=True)
Callee('*cgraph_rtl_info', internal=True)
exits = []
gdb.events.exited.connect(lambda event: exits.append(getattr(event, 'exit_code', None)))
gdb.execute('run')
report = {'events': events, 'frames': frames, 'callees': callees, 'allocation_counts': counts, 'errors': errors, 'exit_codes': exits, 'pending': len(pending)}
Path('stack-observe.json').write_text(json.dumps(report, indent=2)+'\n')
assert exits == [0] and not errors and not pending and frames and callees, 'incomplete target observation'
print('observed', sum(counts.values()), 'stack allocations;', len(events), 'target allocations')
