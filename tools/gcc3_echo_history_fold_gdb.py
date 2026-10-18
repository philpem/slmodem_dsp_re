"""Observe installed cc1plus expression construction, without inferior writes."""
import hashlib
import json
import os
from pathlib import Path
import gdb

TARGET = os.environ.get('GCC3_HISTORY_TARGET', 'historyAlloc')
FIRST = int(os.environ.get('GCC3_HISTORY_FIRST_LINE', '222'))
LAST = FIRST + 1
events, bindings, errors, pending, exits = [], [], [], [], []
counts = {}


def value(expression):
    return gdb.parse_and_eval(expression)


def node(pointer, depth=0):
    if pointer is None or not int(pointer):
        return None
    n = pointer.dereference()
    code = str(n['common']['code'])
    result = {'code': code, 'pointer': int(pointer)}
    kind = n['common']['type']
    if int(kind) and str(kind.dereference()['common']['code']) in ('INTEGER_TYPE', 'ENUMERAL_TYPE'):
        result['type'] = {'precision': int(kind.dereference()['type']['precision']),
                          'unsigned': int(kind.dereference()['common']['unsigned_flag'])}
    result['side_effects'] = int(n['common']['side_effects_flag'])
    result['volatile'] = int(n['common']['volatile_flag'])
    if code in ('VAR_DECL', 'PARM_DECL', 'FIELD_DECL', 'FUNCTION_DECL'):
        name = n['decl']['name']
        result['name'] = name.dereference()['identifier']['id']['str'].string() if int(name) else None
    elif code == 'INTEGER_CST':
        result['low'] = int(n['int_cst']['int_cst']['low'])
        result['high'] = int(n['int_cst']['int_cst']['high'])
    elif depth < 12:
        arities = {'PLUS_EXPR': 2, 'MINUS_EXPR': 2, 'MULT_EXPR': 2,
                   'TRUNC_DIV_EXPR': 2, 'MODIFY_EXPR': 2, 'COMPONENT_REF': 2,
                   'NOP_EXPR': 1, 'CONVERT_EXPR': 1, 'NON_LVALUE_EXPR': 1,
                   'INDIRECT_REF': 1, 'ADDR_EXPR': 1, 'SAVE_EXPR': 1}
        if code in arities:
            result['operands'] = [node(n['exp']['operands'][i], depth + 1) for i in range(arities[code])]
    return result


def frames():
    frame, result = gdb.newest_frame(), []
    while frame and len(result) < 12:
        result.append(frame.name())
        frame = frame.older()
    return result


def relevant(tree):
    return bool(tree and (tree.get('name') in ('span', 'blockLen', 'extra') or
                         any(relevant(operand) for operand in tree.get('operands', []))))


class Done(gdb.FinishBreakpoint):
    def __init__(self, event):
        super().__init__(gdb.newest_frame(), internal=True)
        self.event = event

    def stop(self):
        try:
            self.event['returned'] = node(self.return_value)
            pending.remove(self.event)
        except Exception as exc:
            errors.append(str(exc))
        return False


class Hook(gdb.Breakpoint):
    def __init__(self, name):
        super().__init__('*' + name, internal=True)
        self.hook = name
        self.condition = 'input_location.line >= %d && input_location.line <= %d' % (FIRST, LAST)

    def stop(self):
        try:
            filename = value('input_location.file').string()
            if not filename.endswith('V92EchoCanceller.cpp'):
                return False
            counts[self.hook] = counts.get(self.hook, 0) + 1
            callers = frames()
            if not any(name and name.startswith('cp_parser') for name in callers):
                return False
            event = {'hook': self.hook, 'file': filename,
                     'line': int(value('input_location.line')), 'callers': callers}
            if self.hook == 'fold':
                event['input'] = node(value('*(tree*)($esp+4)'))
                if not relevant(event['input']):
                    return False
            elif self.hook == 'build_modify_expr':
                left = node(value('*(tree*)($esp+4)'))
                if left['code'] != 'COMPONENT_REF' or left['operands'][1].get('name') != TARGET:
                    return False
                event['left'] = left
                event['right'] = node(value('*(tree*)($esp+12)'))
                bindings.append(event)
            else:
                event['code'] = str(value('*(enum tree_code*)($esp+4)'))
                event['operands'] = [node(value('*(tree*)($esp+%d)' % offset)) for offset in (8, 12)]
                if not any(relevant(operand) for operand in event['operands']):
                    return False
            events.append(event)
            pending.append(event)
            Done(event)
        except Exception as exc:
            errors.append(str(exc))
        return False


assert hashlib.sha256(Path(gdb.current_progspace().filename).read_bytes()).hexdigest() == 'b778f44bd1a5e8184ca34b11c9701d0a8d67d15406282c1a204237d03c82082d'
assert gdb.lookup_type('long').sizeof == 4
assert int(value('(unsigned long)fold')) == 0x081518b0
assert int(value('(unsigned long)build_binary_op')) == 0x08097ce0
assert int(value('(unsigned long)build_modify_expr')) == 0x0809b54c
Hook('build_binary_op')
Hook('fold')
Hook('build_modify_expr')
gdb.events.exited.connect(lambda event: exits.append(getattr(event, 'exit_code', None)))
gdb.execute('run')
report = {'target': TARGET, 'line_window': [FIRST, LAST], 'events': events if bindings else [],
          'assignment_bindings': bindings, 'parser_candidate_events': len(events),
          'hook_counts': counts, 'errors': errors, 'exit_codes': exits, 'pending': len(pending)}
Path('fold-observe.json').write_text(json.dumps(report, indent=2) + '\n')
assert exits == [0] and not errors and not pending and events and bindings, 'incomplete target fold observation'
print('observed', len(events), 'expression construction/fold events')
