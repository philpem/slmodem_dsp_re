"""Pinned, read-only local allocation observation; loaded only inside GDB."""
import hashlib
import json
from pathlib import Path
import gdb

settings = json.loads(Path('alloc-settings.json').read_text())
events, active, errors, exits = [], [], [], []
counts = {'whole_tu_local_choices': 0, 'target_local_choices': 0,
          'whole_tu_pass_entries': 0, 'target_pass_entries': 0}


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def read(expression):
    value = gdb.parse_and_eval(expression)
    require(not value.is_optimized_out, 'optimized out: ' + expression)
    return int(value)


def name():
    value = gdb.parse_and_eval('current_function_decl->decl.assembler_name')
    require(int(value) != 0, 'missing assembler name')
    return value.dereference()['identifier']['id']['str'].string()


def mask(expression):
    words = [read(expression + '[%d]' % i) for i in range(2)]
    return [r for r in range(53) if words[r // 32] & (1 << (r % 32))]


def append(row):
    row['sequence'] = len(events)
    row['function'] = settings['function']
    events.append(row)


class Done(gdb.FinishBreakpoint):
    def __init__(self, row):
        super().__init__(gdb.newest_frame(), internal=True)
        self.row = row

    def stop(self):
        try:
            require(self.return_value is not None, 'missing local allocation return')
            result = int(self.return_value)
            self.row['return'] = result
            require(bool(self.row.get('exclusions')), 'no exclusions observed for local choice')
            append({'kind': 'local_choice_return', 'choice_sequence': self.row['sequence'],
                    'selected': result})
            active.remove(self.row)
        except Exception as exc:
            errors.append('return: ' + str(exc))
        return False

    def out_of_scope(self):
        errors.append('local allocation finish left scope')


class Choice(gdb.Breakpoint):
    def stop(self):
        counts['whole_tu_local_choices'] += 1
        try:
            if name() != settings['function']:
                return False
            counts['target_local_choices'] += 1
            row = {'kind': 'local_choice', 'args': {key: read(key) for key in
                   ('class', 'mode', 'qtyno', 'accept_call_clobbered',
                    'just_try_suggested', 'born_index', 'dead_index')}}
            row['quantity'] = {key: read('qty[qtyno].' + key) for key in
                   ('n_refs', 'freq', 'birth', 'death', 'size', 'n_calls_crossed',
                    'first_reg', 'min_class', 'alternate_class', 'phys_reg')}
            row['members'] = []
            reg = row['quantity']['first_reg']
            maximum = read('max_regno')
            while reg >= 0:
                require(reg < maximum and reg not in row['members'], 'invalid/cyclic quantity membership')
                row['members'].append(reg)
                reg = read('reg_next_in_qty[%d]' % reg)
            row['copy_suggestions'] = mask('qty_phys_copy_sugg[qtyno]')
            row['arithmetic_suggestions'] = mask('qty_phys_sugg[qtyno]')
            row['allocation_order'] = [read('reg_alloc_order[%d]' % i) for i in range(53)]
            append(row)
            active.append(row)
            Done(row)
        except Exception as exc:
            errors.append('choice: ' + str(exc))
        return False


class Exclusions(gdb.Breakpoint):
    def stop(self):
        try:
            if name() == settings['function']:
                require(bool(active), 'exclusions outside active choice')
                row = {'used': mask('used'), 'first_used': mask('first_used')}
                active[-1].setdefault('exclusions', []).append(row)
                append({'kind': 'local_exclusions', 'choice_sequence': active[-1]['sequence'], **row})
        except Exception as exc:
            errors.append('exclusions: ' + str(exc))
        return False


class Pass(gdb.Breakpoint):
    def stop(self):
        counts['whole_tu_pass_entries'] += 1
        try:
            if name() == settings['function']:
                counts['target_pass_entries'] += 1
                append({'kind': 'pass_entry', 'pass': gdb.newest_frame().name()})
        except Exception as exc:
            errors.append('pass: ' + str(exc))
        return False


try:
    compiler = Path(gdb.current_progspace().filename)
    require(hashlib.sha256(compiler.read_bytes()).hexdigest() == settings['compiler_sha256'], 'compiler hash mismatch')
    require(settings['compiler_sha256'] in {
        '80a79e520ca77fb1efa3bbd5ac4d927d075d6c1fdc6dd908fd83b437cc47febc',
        'b778f44bd1a5e8184ca34b11c9701d0a8d67d15406282c1a204237d03c82082d'}, 'unsupported compiler')
    require(gdb.lookup_type('long').sizeof == 4, 'unsupported target ABI')
    for hook in (Choice('find_free_reg', internal=True),
                 Exclusions('/build/gcc-3.4.2/gcc/local-alloc.c:2206', internal=True),
                 Pass('local_alloc', internal=True), Pass('global_alloc', internal=True)):
        require(bool(hook.locations), 'unresolved hook: ' + hook.location)
    gdb.events.exited.connect(lambda event: exits.append(getattr(event, 'exit_code', None)))
    gdb.execute('run')
except Exception as exc:
    errors.append('observer: ' + str(exc))
finally:
    if not counts['target_local_choices']:
        errors.append('target has no observed local choices')
    Path('allocation-trace.json').write_text(json.dumps({
        'function': settings['function'], 'events': events, 'denominators': counts,
        'errors': errors, 'exit_codes': exits, 'pending': len(active),
        'scope': 'local quantity choices/exclusions and pass entries; no global/reload decision model'}, indent=2) + '\n')
require(not errors and exits == [0] and not active, 'unsound/incomplete allocation trace')
print('allocation trace:', counts)
