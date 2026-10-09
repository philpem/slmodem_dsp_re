"""GDB Python commands: observe UID56 postreload paths, without inferior calls."""
import gdb
import os
import hashlib
from pathlib import Path

expected = '80a79e520ca77fb1efa3bbd5ac4d927d075d6c1fdc6dd908fd83b437cc47febc'
if hashlib.sha256(Path(gdb.current_progspace().filename).read_bytes()).hexdigest() != expected:
    raise RuntimeError('cc1 hash mismatch: fixed addresses are compiler-specific')

active = False
target_uid = int(os.environ.get('SDM_TRACE_UID', '56'))
watched_uids = {38, 47, 48, 53, 54, 55, target_uid}


def function_name():
    return gdb.parse_and_eval('cfun->decl->decl.name->identifier.id.str').string()


def value_summary(pointer):
    if int(pointer) == 0:
        return 'NULL'
    value = pointer.dereference()
    locations = []
    link = value['locs']
    while int(link) and len(locations) < 16:
        locations.append(rtl(link.dereference()['loc']))
        link = link.dereference()['next']
    return 'ptr=%s value=%s mode=%s locs=%s%s' % (
        pointer, value['value'], value['u']['val_rtx'].dereference()['mode'],
        locations, ' [truncated]' if int(link) else '')


def rtl(value, depth=0):
    value = value.dereference()
    code = str(value['code'])
    mode = str(value['mode'])
    if code == 'REG':
        return '%s:%s %s' % (code, mode, value['u']['fld'][0]['rtuint'])
    if depth < 3 and code in ('SET', 'ZERO_EXTEND', 'MEM', 'SUBREG'):
        children = 2 if code == 'SET' else 1
        return '%s:%s(%s)' % (code, mode, ','.join(
            rtl(value['u']['fld'][i]['rtx'], depth+1) for i in range(children)))
    return '%s:%s' % (code, mode)


class Returned(gdb.FinishBreakpoint):
    def __init__(self, name):
        super().__init__(gdb.newest_frame(), internal=True)
        self.name = name

    def stop(self):
        global active
        print('TRACE return %s = %s' % (self.name, self.return_value))
        if self.name == 'cselib_lookup':
            print('TRACE lookup value ' + value_summary(self.return_value))
        if self.name == 'simplify_set':
            active = False
        return False


class Enter(gdb.Breakpoint):
    def __init__(self, address, name):
        super().__init__('*' + address, internal=True)
        self.name = name

    def stop(self):
        global active
        if self.name == 'simplify_set':
            insn = gdb.parse_and_eval('*(rtx*)($ebp+12)')
            uid = int(insn.dereference()['u']['fld'][0]['rtint'])
            if function_name() != 'SDM_descrambler' or uid not in (38, target_uid):
                return False
            active = True
            pattern = gdb.parse_and_eval('*(rtx*)($ebp+8)')
            print('TRACE UID%d simplify_set input ' % uid + rtl(pattern))
            Returned('simplify_set')
        elif self.name == 'operands':
            insn = gdb.parse_and_eval('*(rtx*)($ebp+8)')
            uid = int(insn.dereference()['u']['fld'][0]['rtint'])
            if function_name() == 'SDM_descrambler' and uid in (38, target_uid):
                print('TRACE UID%d fallback operands' % uid)
                Returned('operands')
        elif active:
            print('TRACE cselib_lookup source ' + rtl(gdb.parse_and_eval('*(rtx*)($esp+4)')))
            Returned('cselib_lookup')
        return False


class Cost(gdb.Breakpoint):
    def __init__(self):
        super().__init__('*0x08292c36', internal=True)

    def stop(self):
        if active:
            print('TRACE candidate %s old_cost=%s new_cost=%s' % (
                rtl(gdb.parse_and_eval('$ebx').cast(gdb.lookup_type('rtx'))),
                gdb.parse_and_eval('*(int*)($ebp-24)'),
                gdb.parse_and_eval('*(int*)($ebp-28)')))
        return False


class RecordReturn(gdb.FinishBreakpoint):
    def __init__(self, uid, value, address):
        super().__init__(gdb.newest_frame(), internal=True)
        self.uid, self.value, self.address = uid, value, address

    def stop(self):
        print('TRACE record_set after UID%d value %s addr %s' % (
            self.uid, value_summary(self.value), value_summary(self.address)))
        return False


class Record(gdb.Breakpoint):
    def __init__(self):
        super().__init__('*0x080baaa4', internal=True)

    def stop(self):
        if function_name() != 'SDM_descrambler':
            return False
        frame = gdb.newest_frame()
        callers = []
        while frame:
            callers.append(frame.name())
            frame = frame.older()
        if 'reload_cse_regs_1' not in callers:
            return False
        insn = gdb.parse_and_eval('cselib_current_insn')
        uid = int(insn.dereference()['u']['fld'][0]['rtint'])
        if uid not in watched_uids:
            return False
        dest = gdb.parse_and_eval('*(rtx*)($esp+4)')
        value = gdb.parse_and_eval('*(cselib_val**)($esp+8)')
        address = gdb.parse_and_eval('*(cselib_val**)($esp+12)')
        print('TRACE record_set before UID%d dest %s value %s addr %s' % (
            uid, rtl(dest), value_summary(value), value_summary(address)))
        RecordReturn(uid, value, address)
        return False


Enter('0x08292b14', 'simplify_set')
Enter('0x08292cd8+12', 'operands')
Enter('0x080ba2e8', 'lookup')
Cost()
Record()
