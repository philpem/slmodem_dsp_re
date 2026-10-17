"""Read-only native CID wrapper call/return trace; driven by cid_process_trace.py."""
import hashlib
import json
from pathlib import Path
import gdb

settings = json.loads(Path('trace-settings.json').read_text())
records, lifecycles, errors, exits = [], [], [], []
live, active, waiting = {}, {}, {}


def reg(name):
    return int(gdb.parse_and_eval('$' + name)) & 0xffffffff


def mem(address, count):
    return bytes(gdb.selected_inferior().read_memory(address, count))


def word(address):
    return int.from_bytes(mem(address, 4), 'little')


def signed(value, bits=32):
    return (value + (1 << (bits - 1))) % (1 << bits) - (1 << (bits - 1))


def text(address, limit=600):
    out = bytearray()
    if not address:
        return None
    for offset in range(limit):
        ch = mem(address + offset, 1)[0]
        if not ch:
            return out.decode('latin1')
        out.append(ch)
    raise AssertionError('unterminated string')


def receiver_state(address):
    return [signed(word(address + off)) for off in (0x260, 0x264, 0x3f8)]


class Done(gdb.FinishBreakpoint):
    def __init__(self, kind, action, row):
        super().__init__(gdb.newest_frame(), internal=True)
        self.kind, self.action, self.row = kind, action, row

    def stop(self):
        try:
            result = reg('eax')
            if self.action == 'create':
                self.row['created'] = bool(result)
                if result:
                    key = (self.kind, result)
                    assert key not in live
                    self.row['receiver'] = word(result + 4)
                    self.row['wrapper'] = result
                    live[key] = self.row
            else:
                assert self.kind not in waiting
                self.row['result'] = signed(result)
                self.row['finished'] = True
                del active[self.kind]
        except Exception as exc:
            errors.append(str(exc))
        return False


class Entry(gdb.Breakpoint):
    def __init__(self, kind, action, address):
        super().__init__('*' + hex(address), internal=True)
        self.kind, self.action = kind, action

    def stop(self):
        try:
            sp = reg('esp')
            pointer = word(sp + 4)
            if self.action == 'create':
                row = {'kind': self.kind, 'modem': pointer, 'rate': word(sp + 8),
                       'cid_val': word(sp + 12), 'created': None}
                lifecycles.append(row)
                Done(self.kind, 'create', row)
            elif self.action == 'delete':
                if pointer:
                    row = live.pop((self.kind, pointer))
                    row['deleted'] = True
            else:
                assert self.kind not in active
                owner = live[(self.kind, pointer)]
                assert owner['rate'] == 8000 and owner['cid_val'] == 1
                assert word(pointer) == owner['modem']
                row = {'kind': self.kind, 'lifecycle': lifecycles.index(owner) // 2,
                       'count': signed(word(sp + 12)), 'input': word(sp + 8),
                       'receiver': word(pointer + 4), 'modem': word(pointer),
                       'debug': word(settings[self.kind]['debug']), 'events': [],
                       'finished': False}
                assert row['receiver'] == owner['receiver']
                row['input_sha256'] = hashlib.sha256(mem(row['input'], max(0, row['count']) * 2)).hexdigest()
                records.append(row)
                active[self.kind] = row
                Done(self.kind, 'process', row)
        except Exception as exc:
            errors.append(str(exc))
        return False


class After(gdb.Breakpoint):
    def __init__(self, kind, address):
        super().__init__('*' + hex(address), internal=True)
        self.kind = kind

    def stop(self):
        try:
            row, event, aux = waiting.pop(self.kind)
            value = reg('eax')
            name = event['call']
            if name == 'progress':
                event['n_out'] = signed(int.from_bytes(mem(aux, 2), 'little'), 16)
                event['result'] = signed(value & 0xffff, 16)
                event['state_out'] = receiver_state(row['receiver'])
            elif name == 'get_strings':
                assert value == row['receiver'] + 8
                row['strings'] = value
                event['result_offset'] = 8
                event['strings_hex'] = mem(value, 600).hex()
            elif name == 'strlen':
                event['result'] = value
            else:
                # These host functions return int, although CID_process
                # ignores their results. Record the harness boundary too.
                event['result'] = signed(value)
        except Exception as exc:
            errors.append(str(exc))
        return False


class Call(gdb.Breakpoint):
    def __init__(self, kind, address, name):
        super().__init__('*' + hex(address), internal=True)
        self.kind, self.name = kind, name
        After(kind, address + 5)

    def stop(self):
        try:
            row = active[self.kind]
            assert self.kind not in waiting
            sp = reg('esp')
            args = [word(sp + 4 * i) for i in range(4)]
            event, aux = {'call': self.name}, None
            if self.name == 'progress':
                assert args[0] == row['receiver']
                assert (args[1] - row['input']) % 2 == 0
                aux = args[3]
                assert aux == sp + 0x1a, 'count pointer is not the witnessed wrapper stack slot'
                event.update(input_offset=(args[1] - row['input']) // 2,
                             what=signed(args[2]),
                             n_in=signed(int.from_bytes(mem(aux, 2), 'little'), 16),
                             state_in=receiver_state(args[0]))
            elif self.name == 'get_strings':
                assert args[0] == row['receiver']
            elif self.name == 'strlen':
                event.update(offset=args[0] - row['strings'], text=text(args[0]))
            elif self.name == 'tty':
                assert args[0] == row['modem']
                event.update(length=signed(args[2]), payload_hex=mem(args[1], args[2]).hex())
                event['offset'] = args[1] - row['strings'] if row['strings'] <= args[1] < row['strings'] + 600 else None
            else:
                event['format'] = text(args[0])
                if '%s' in event['format']:
                    event['text'] = text(args[1])
                    event['offset'] = args[1] - row['strings']
            row['events'].append(event)
            waiting[self.kind] = (row, event, aux)
        except Exception as exc:
            errors.append(str(exc))
        return False


class Loop(gdb.Breakpoint):
    def __init__(self, kind, address):
        super().__init__('*' + hex(address), internal=True)
        self.kind = kind

    def stop(self):
        try:
            row = active[self.kind]
            pointer = reg('ebx')
            index = word(reg('esp') + 0x10) if self.kind == 'original' else reg('esi')
            row['events'].append({'call': 'string_guard', 'index': index,
                                  'offset': pointer - row['strings'] if pointer else None,
                                  'null': pointer == 0,
                                  'empty': pointer != 0 and mem(pointer, 1) == b'\0'})
        except Exception as exc:
            errors.append(str(exc))
        return False


for kind, config in settings.items():
    for action in ('create', 'delete', 'process'):
        Entry(kind, action, config[action])
    for offset, name in config['sites']:
        Call(kind, config['process'] + offset, name)
    Loop(kind, config['process'] + config['loop'])
gdb.events.exited.connect(lambda event: exits.append(getattr(event, 'exit_code', None)))
gdb.execute('run')
result = {'records': records, 'lifecycles': lifecycles, 'errors': errors,
          'exit_codes': exits, 'active': list(active), 'live': list(live),
          'waiting': list(waiting)}
Path('trace-observe.json').write_text(json.dumps(result, indent=2) + '\n')
assert exits == [0] and not errors and not active and not live and not waiting, result['errors']
assert records, 'no CID_process observations'
print('observed', len(records), 'completed CID_process calls')
