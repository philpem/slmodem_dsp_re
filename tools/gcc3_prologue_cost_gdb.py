"""Observe the installed period compiler without inferior calls or state edits."""
import json
import os
from pathlib import Path
import gdb

TARGET = os.environ.get('GCC3_PROLOGUE_TARGET', 'silence_progress')
frames, costs, errors, pending = [], [], [], []


def number(expression):
    return int(gdb.parse_and_eval(expression))


def selected():
    try:
        name = gdb.parse_and_eval('cfun->decl->decl.name->identifier.id.str').string()
        assembler = gdb.parse_and_eval('cfun->decl->decl.assembler_name->identifier.id.str').string()
        return TARGET in (name, assembler)
    except Exception:
        return False


def cache():
    return {field: number('cfun->machine->'+field) for field in
            ('use_fast_prologue_epilogue', 'use_fast_prologue_epilogue_nregs')}


class FrameDone(gdb.FinishBreakpoint):
    def __init__(self, event, pointer):
        super().__init__(gdb.newest_frame(), internal=True)
        self.event, self.pointer = event, pointer

    def stop(self):
        try:
            self.event['cache_after'] = cache()
            self.event['layout'] = {
                field: number('((struct ix86_frame*)%d)->%s' % (self.pointer, field))
                for field in ('nregs', 'padding1', 'padding2', 'outgoing_arguments_size',
                              'to_allocate', 'save_regs_using_mov')}
            pending.remove(self.event)
        except Exception as exc:
            errors.append(str(exc))
        return False


class Frame(gdb.Breakpoint):
    def stop(self):
        if not selected():
            return False
        try:
            pointer = number('*(unsigned int*)($esp+4)')
            event = {'assembler_name': gdb.parse_and_eval('cfun->decl->decl.assembler_name->identifier.id.str').string(),
                     'cache_before': cache(), 'reload_completed': number('reload_completed'),
                     'locals': -number('cfun->x_frame_offset'),
                     'caller': gdb.newest_frame().older().name()}
            frames.append(event)
            pending.append(event)
            FrameDone(event, pointer)
        except Exception as exc:
            errors.append(str(exc))
        return False


class CostDone(gdb.FinishBreakpoint):
    def __init__(self, event):
        super().__init__(gdb.newest_frame(), internal=True)
        self.event = event

    def stop(self):
        try:
            self.event['expensive'] = int(self.return_value)
            pending.remove(self.event)
        except Exception as exc:
            errors.append(str(exc))
        return False


class Cost(gdb.Breakpoint):
    def stop(self):
        if not selected():
            return False
        try:
            event = {'threshold': number('*(int*)($esp+4)'),
                     'entry_frequency': number('entry_exit_blocks[0].frequency'),
                     'cache': cache(), 'reload_completed': number('reload_completed'),
                     'samples': []}
            costs.append(event)
            pending.append(event)
            CostDone(event)
        except Exception as exc:
            errors.append(str(exc))
        return False


class Count(gdb.Breakpoint):
    def stop(self):
        if not selected():
            return False
        try:
            costs[-1]['samples'].append({
                'weighted_count': number('$edi'),
                'threshold_weighted': number('*(unsigned int*)($ebp-16)'),
                'block': number('((struct basic_block_def*)$esi)->index'),
                'frequency': number('((struct basic_block_def*)$esi)->frequency')})
        except Exception as exc:
            errors.append(str(exc))
        return False


assert gdb.lookup_type('long').sizeof == 4, 'unsupported compiler ABI'
start = number('(unsigned long)expensive_function_p')
# In this compiler, +91 follows ADD bb->frequency,EDI and precedes CMP limit,EDI.
# Reject another instruction sequence instead of guessing its register contract.
assert bytes(gdb.selected_inferior().read_memory(start+88, 6)).hex() == '037e503b7df0', 'unsupported cost hook'
Frame('*ix86_compute_frame_layout', internal=True)
Cost('*expensive_function_p', internal=True)
Count('*expensive_function_p+91', internal=True)
exits = []
gdb.events.exited.connect(lambda event: exits.append(getattr(event, 'exit_code', None)))
gdb.execute('run')
Path('prologue-observe.json').write_text(json.dumps({
    'frames': frames, 'costs': costs, 'errors': errors,
    'pending': len(pending), 'exits': exits}, indent=2)+'\n')
assert exits == [0] and not errors and not pending and frames and costs, 'incomplete target observation'
assert len({frame['assembler_name'] for frame in frames}) == 1, 'ambiguous target: use its assembler name'
print('Observed', len(frames), 'layouts,', len(costs), 'cost calls; no observer errors')
