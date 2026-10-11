"""Read-only allocation boundaries for unchanged Detect_1s in Gentoo cc1."""
import gdb
import json
from pathlib import Path

events = []
errors = []


def value(expression):
    try:
        v = gdb.parse_and_eval(expression)
        if v.is_optimized_out:
            return {'unavailable': 'optimized out'}
        return int(v)
    except gdb.error as exc:
        return {'unavailable': str(exc)}


def target():
    return gdb.parse_and_eval('current_function_decl->decl.name->identifier.id.str').string() == 'Detect_1s'


def rtx(pointer, depth=0):
    if int(pointer) == 0:
        return None
    assert depth < 12, 'unexpected deep reload operand'
    node = pointer.dereference()
    code = int(node['code'])
    name = gdb.parse_and_eval('rtx_name[%d]' % code).string()
    mode = str(node['mode'])
    fields = []
    for index, kind in enumerate(gdb.parse_and_eval('rtx_format[%d]' % code).string()):
        field = node['u']['fld'][index]
        if kind == 'e':
            fields.append(rtx(field['rtx'], depth + 1))
        elif kind == 'i':
            fields.append(int(field['rtint']))
        elif kind == 'w':
            fields.append(int(node['u']['hwint'][index]))
        elif kind == 's':
            fields.append(field['rtstr'].string() if int(field['rtstr']) else None)
        elif kind == '0':
            fields.append('<annotation>')
        else:
            raise RuntimeError('unsupported reload operand %s format %s' % (name, kind))
    return [name, mode, fields]


def reloads():
    count = value('n_reloads')
    assert isinstance(count, int) and 0 <= count < 100
    rows = []
    for i in range(count):
        item = gdb.parse_and_eval('rld[%d]' % i)
        rows.append({'in': rtx(item['in']), 'out': rtx(item['out']),
                     'selected': rtx(item['reg_rtx']),
                     'properties': {k: int(item[k]) for k in ('class', 'inmode', 'outmode',
                                    'mode', 'nregs', 'regno', 'opnum', 'optional', 'when_needed')}})
    return rows


class ChoiceDone(gdb.FinishBreakpoint):
    def __init__(self, event):
        self.event = event
        super().__init__(gdb.newest_frame(), internal=True)

    def stop(self):
        try:
            self.event['after'] = reloads()
            self.event['cursor_after'] = value("'reload1.c'::last_spill_reg")
        except Exception as exc:
            errors.append(str(exc))
        return False


class Choice(gdb.Breakpoint):
    def stop(self):
        try:
            if target():
                # True i386 cdecl entry, independent of optimized DWARF parameters.
                chain = gdb.parse_and_eval('*(struct insn_chain**)($esp+4)')
                event = {'stage': 'reload-choice', 'uid': int(chain['insn']['u']['fld'][0]['rtint']),
                         'cursor_before': value("'reload1.c'::last_spill_reg"),
                         'spill_regs': [value('spill_regs[%d]' % i) for i in range(value('n_spills'))],
                         'before': reloads()}
                events.append(event)
                ChoiceDone(event)
        except Exception as exc:
            errors.append(str(exc))
        return False


def snapshot(stage):
    count = value('max_regno')
    assert isinstance(count, int) and 53 < count < 10000
    event = {'stage': stage, 'mapping': {str(n): value('reg_renumber[%d]' % n) for n in range(53, count)},
             'order': [value('reg_alloc_order[%d]' % n) for n in range(53)],
             'ever_live': [value('regs_ever_live[%d]' % n) for n in range(53)],
             'fixed': [value('fixed_regs[%d]' % n) for n in range(53)],
             'state': {n: value(n) for n in ('frame_pointer_needed', 'reload_completed',
                       'preferred_stack_boundary', 'current_function_preferred_stack_boundary',
                       'preferred_incoming_stack_boundary', "'reload1.c'::last_spill_reg")}}
    assert all(isinstance(v, int) for v in event['mapping'].values())
    events.append(event)


class ReloadDone(gdb.FinishBreakpoint):
    def stop(self):
        snapshot('reload-return')
        return False


class Boundary(gdb.Breakpoint):
    def __init__(self, function, stage):
        self.stage = stage
        super().__init__('*' + function, internal=True)

    def stop(self):
        try:
            if target():
                snapshot(self.stage)
                if self.stage == 'reload-entry':
                    ReloadDone(gdb.newest_frame(), internal=True)
        except Exception as exc:
            errors.append(str(exc))
        return False


Boundary('global_alloc', 'global-entry')
Boundary('reload', 'reload-entry')
Choice('*choose_reload_regs', internal=True)
exits = []
gdb.events.exited.connect(lambda e: exits.append(getattr(e, 'exit_code', None)))
gdb.execute('run')
Path('observe.json').write_text(json.dumps({'events': events, 'errors': errors, 'exits': exits}, indent=2) + '\n')
assert not errors and exits == [0]
assert [e['stage'] for e in events if e['stage'] != 'reload-choice'] == ['global-entry', 'reload-entry', 'reload-return']
assert any(e['stage'] == 'reload-choice' for e in events)
print('3 allocation/reload boundary snapshots; no inferior calls or writes')
