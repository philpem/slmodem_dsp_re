"""Read-only local/global allocation observation of installed Gentoo cc1."""
import json
import os
import hashlib
from pathlib import Path
import gdb

TARGET = os.environ.get('GCC3_PREINIT_TARGET', 'preinitdigital')
events, active, errors, exits = [], [], [], []
PIN = '80a79e520ca77fb1efa3bbd5ac4d927d075d6c1fdc6dd908fd83b437cc47febc'
assert hashlib.sha256(Path(gdb.current_progspace().filename).read_bytes()).hexdigest() == PIN


def value(expression):
    result = gdb.parse_and_eval(expression)
    if result.is_optimized_out:
        raise RuntimeError('optimized out: ' + expression)
    return int(result)


def name():
    try:
        return gdb.parse_and_eval('current_function_decl->decl.name->identifier.id.str').string()
    except gdb.error:
        return ''


def mask(expression):
    words = [value(expression + '[%d]' % i) for i in range(2)]
    return [r for r in range(53) if words[r // 32] & (1 << (r % 32))]


class Done(gdb.FinishBreakpoint):
    def __init__(self, event):
        super().__init__(gdb.newest_frame(), internal=True)
        self.event = event

    def stop(self):
        try:
            self.event['return'] = int(self.return_value) if self.return_value is not None else None
            self.event['homes_after'] = {str(r): value('reg_renumber[%d]' % r) for r in range(79, 84)}
            active.remove(self.event)
        except Exception as exc:
            errors.append(str(exc))
        return False


class Allocation(gdb.Breakpoint):
    def stop(self):
        if name() != TARGET:
            return False
        try:
            row = {'kind': 'local_choice', 'args': {k: value(k) for k in
                ('class', 'mode', 'qtyno', 'accept_call_clobbered', 'just_try_suggested', 'born_index', 'dead_index')}}
            row['quantity'] = {k: value('qty[qtyno].' + k) for k in
                ('n_refs', 'freq', 'birth', 'death', 'size', 'n_calls_crossed', 'first_reg', 'min_class', 'alternate_class', 'phys_reg')}
            row['members'] = []
            reg = row['quantity']['first_reg']
            while reg >= 0:
                assert reg not in row['members'], 'cyclic quantity'
                row['members'].append(reg)
                reg = value('reg_next_in_qty[%d]' % reg)
            row['copy_suggestions'] = mask('qty_phys_copy_sugg[qtyno]')
            row['arithmetic_suggestions'] = mask('qty_phys_sugg[qtyno]')
            row['allocation_order'] = [value('reg_alloc_order[%d]' % i) for i in range(53)]
            events.append(row)
            active.append(row)
            Done(row)
        except Exception as exc:
            errors.append(str(exc))
        return False


class Exclusions(gdb.Breakpoint):
    def stop(self):
        if name() == TARGET and active:
            try:
                active[-1].setdefault('exclusions', []).append({
                    'used': mask('used'), 'first_used': mask('first_used')})
            except Exception as exc:
                errors.append(str(exc))
        return False


class Pass(gdb.Breakpoint):
    def stop(self):
        if name() == TARGET:
            row = {'kind': gdb.newest_frame().name(),
                   'homes_before': {str(r): value('reg_renumber[%d]' % r) for r in range(79, 84)}}
            events.append(row)
            active.append(row)
            Done(row)
        return False


class Rename(gdb.Breakpoint):
    def stop(self):
        if name() != TARGET:
            return False
        try:
            read = gdb.newest_frame().read_var
            chain = read('this')
            refs, seen = [], set()
            while int(chain):
                assert int(chain) not in seen and len(seen) < 256
                seen.add(int(chain))
                node = chain.dereference()
                rtl = node['loc'].dereference().dereference()
                assert str(rtl['code']) == 'REG'
                refs.append({'uid': int(node['insn'].dereference()['u']['fld'][0]['rtint']),
                             'register': int(rtl['u']['fld'][0]['rtuint']),
                             'mode': str(rtl['mode']), 'class': str(node['class'])})
                chain = node['next_use']
            events.append({'kind': 'rename', 'bb': int(read('bb').dereference()['index']),
                'old': int(read('reg')), 'selected': int(read('best_new_reg')),
                'tick': [int(read('tick')[i]) for i in range(53)],
                'unavailable': mask('this_unavailable'), 'chain': refs})
        except Exception as exc:
            errors.append(str(exc))
        return False


assert gdb.lookup_type('long').sizeof == 4
hook = value('(unsigned long)find_free_reg') + 376
assert bytes(gdb.selected_inferior().read_memory(hook, 4)).hex() == '31ff89f6'
Allocation('find_free_reg', internal=True)
Exclusions('/build/gcc-3.4.2/gcc/local-alloc.c:2206', internal=True)
Pass('local_alloc', internal=True)
Pass('global_alloc', internal=True)
Rename('/build/gcc-3.4.2/gcc/regrename.c:338', internal=True)
gdb.events.exited.connect(lambda event: exits.append(getattr(event, 'exit_code', None)))
gdb.execute('run')
Path('allocation-observe.json').write_text(json.dumps({
    'target': TARGET, 'events': events, 'errors': errors,
    'exit_codes': exits, 'pending': len(active)}, indent=2) + '\n')
assert exits == [0] and events and not errors and not active, (exits, len(events), errors, len(active))
print('allocation observation:', len(events), 'events')
