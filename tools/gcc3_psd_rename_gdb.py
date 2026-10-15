"""Read-only installed-GCC rename decisions; no inferior calls or state writes."""
import hashlib
import json
from pathlib import Path
import gdb

PIN = 'b778f44bd1a5e8184ca34b11c9701d0a8d67d15406282c1a204237d03c82082d'
assert hashlib.sha256(Path(gdb.current_progspace().filename).read_bytes()).hexdigest() == PIN
events, errors, exits = [], [], []


def integer(value):
    if value.is_optimized_out:
        raise RuntimeError('required DWARF value optimized out')
    return int(value)


class Decision(gdb.Breakpoint):
    def stop(self):
        try:
            name = gdb.parse_and_eval('current_function_decl->decl.name->identifier.id.str').string()
            if name != 'process':
                return False
            frame = gdb.newest_frame()
            read = frame.read_var
            chain = read('this')
            refs, seen = [], set()
            while integer(chain):
                assert integer(chain) not in seen and len(seen) < 256
                seen.add(integer(chain))
                node = chain.dereference()
                rtl = node['loc'].dereference().dereference()
                assert str(rtl['code']) == 'REG'
                refs.append({'uid':integer(node['insn'].dereference()['u']['fld'][0]['rtint']),
                             'register':integer(rtl['u']['fld'][0]['rtuint']),
                             'mode':str(rtl['mode']), 'class':str(node['class'])})
                chain = node['next_use']
            tick = read('tick')
            unavailable = read('this_unavailable')
            events.append({'function':name, 'bb':integer(read('bb').dereference()['index']),
                           'old':integer(read('reg')), 'selected':integer(read('best_new_reg')),
                           'tick':[integer(tick[i]) for i in range(53)],
                           'unavailable':[integer(unavailable[i]) for i in range(2)],
                           'chain':refs})
        except Exception as exc:
            errors.append(str(exc))
        return False


Decision('/build/gcc-3.4.2/gcc/regrename.c:338', internal=True)
gdb.events.exited.connect(lambda event:exits.append(getattr(event,'exit_code',None)))
gdb.execute('run')
report = {'compiler_sha256':PIN, 'events':events, 'errors':errors,
          'exit_codes':exits, 'inferior_calls_or_writes':False}
Path('rename-observe.json').write_text(json.dumps(report,indent=2)+'\n')
assert not errors and exits == [0] and events, errors
print('observed %d process rename decisions; no inferior calls/writes' % len(events))
