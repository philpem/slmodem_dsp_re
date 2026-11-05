"""Read-only GCC 3 scratch observer, loaded by gcc3_search_capture.py in GDB.

The entry ABI and candidate line are supported only for the executable hashes
checked by the caller. No expression invokes an inferior function or writes
inferior memory. GDB's own software breakpoints are the only instrumentation.
"""
import json
from pathlib import Path

import gdb

settings = json.loads(Path('observer-settings.json').read_text())
events, active, errors, exits = [], [], [], []


def read(expression):
    value = gdb.parse_and_eval(expression)
    if value.is_optimized_out:
        raise RuntimeError('optimized out: ' + expression)
    return int(value)


def bits(expression):
    words = [read(expression + '[%d]' % i) for i in range(2)]
    return [r for r in range(53) if words[r // 32] & (1 << (r % 32))]


def cursor():
    return read('*(int*)' + hex(settings['cursor_address']))


def identifier(field):
    value = gdb.parse_and_eval('current_function_decl->decl.' + field)
    if not int(value):
        raise RuntimeError('missing function ' + field)
    return value.dereference()['identifier']['id']['str'].string()


class Done(gdb.FinishBreakpoint):
    def __init__(self, event):
        super().__init__(gdb.newest_frame(), internal=True)
        self.event = event

    def stop(self):
        try:
            result = self.return_value
            if result is None:
                raise RuntimeError('missing scratch return value')
            self.event['selected'] = (int(result.dereference()['u']['fld'][0]['rtint'])
                                      if int(result) else None)
            self.event['cursor_after'] = cursor()
            active.remove(self.event)
        except Exception as exc:
            errors.append('return: ' + str(exc))
        return False

    def out_of_scope(self):
        errors.append('scratch search finish breakpoint left scope')


class Entry(gdb.Breakpoint):
    def stop(self):
        try:
            if active:
                raise RuntimeError('nested scratch search')
            stack = read('$esp')
            arg = lambda offset: read('*(unsigned int*)' + hex(stack + offset))
            signed = lambda value: value - (1 << 32) if value & (1 << 31) else value
            reg_set = arg(20)
            event = {
                'function': identifier('assembler_name'),
                'function_simple_name': identifier('name'),
                'cursor_before': cursor(), 'from': signed(arg(4)),
                'to': signed(arg(8)),
                'mode': str(gdb.Value(arg(16)).cast(gdb.lookup_type('enum machine_mode'))),
                'constraint': gdb.Value(arg(12)).cast(gdb.lookup_type('char').pointer()).string(),
                'allocation_order': [read('reg_alloc_order[%d]' % r) for r in range(53)],
                'excluded': bits('(*(HARD_REG_SET*)' + hex(reg_set) + ')'),
                'candidates': [],
            }
            if not (0 <= event['from'] <= event['to'] < 5 and event['constraint']):
                raise RuntimeError('invalid scratch entry arguments')
            current = read('peep2_current')
            event['insn_uid'] = read('peep2_insn_data[%d].insn->u.fld[0].rtint' %
                                     ((event['from'] + current) % 5))
            events.append(event)
            active.append(event)
            Done(event)
        except Exception as exc:
            errors.append('entry: ' + str(exc))
        return False


class Candidate(gdb.Breakpoint):
    def stop(self):
        try:
            if not active:
                raise RuntimeError('candidate without scratch entry')
            event = active[-1]
            reg, raw, cls = read('regno'), read('raw_regno'), read('class')
            item = {
                'register': reg, 'order_index': raw, 'live': bits('live'),
                'fixed': read('fixed_regs[%d]' % reg),
                'class_ok': bool(read('reg_class_contents[%d][%d]' % (cls, reg // 32)) &
                                 (1 << (reg % 32))),
                'call_used': read('call_used_regs[%d]' % reg),
                'ever_live': read('regs_ever_live[%d]' % reg),
                'reload_completed': read('reload_completed'),
                'frame_pointer_needed': read('frame_pointer_needed'),
            }
            if 'eligibility' not in event:
                event['eligibility'] = {
                    'class': cls, 'live': item['live'],
                    'fixed': [read('fixed_regs[%d]' % r) for r in range(53)],
                    'class_members': bits('reg_class_contents[%d]' % cls),
                    'call_used': [read('call_used_regs[%d]' % r) for r in range(53)],
                    'ever_live': [read('regs_ever_live[%d]' % r) for r in range(53)],
                    'frame_pointer_needed': item['frame_pointer_needed'],
                    'reload_completed': item['reload_completed'],
                }
            event['candidates'].append(item)
        except Exception as exc:
            errors.append('candidate: ' + str(exc))
        return False


try:
    Entry('*peep2_find_free_register', internal=True)
    candidate = Candidate('/build/gcc-3.4.2/gcc/recog.c:2984', internal=True)
    if not candidate.locations:
        raise RuntimeError('candidate source hook did not resolve')
    gdb.events.exited.connect(lambda event: exits.append(getattr(event, 'exit_code', None)))
    gdb.execute('run')
except Exception as exc:
    errors.append('observer: ' + str(exc))
finally:
    report = {'events': events, 'errors': errors, 'exit_codes': exits, 'pending': len(active)}
    Path('scratch-observe.json').write_text(json.dumps(report, indent=2) + '\n')

if errors or exits != [0] or active or not events:
    raise RuntimeError('incomplete scratch observation; inspect scratch-observe.json')
print('observed %d scratch searches; no inferior calls/writes' % len(events))
