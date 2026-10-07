"""GDB read-only output_40/UID118 witness, pinned installed Gentoo cc1."""
import gdb
import hashlib
from pathlib import Path

expected = '80a79e520ca77fb1efa3bbd5ac4d927d075d6c1fdc6dd908fd83b437cc47febc'
if hashlib.sha256(Path(gdb.current_progspace().filename).read_bytes()).hexdigest() != expected:
    raise RuntimeError('installed cc1 hash mismatch')

active = False


class Return(gdb.FinishBreakpoint):
    def __init__(self, label):
        super().__init__(gdb.newest_frame(), internal=True)
        self.label = label

    def stop(self):
        global active
        if self.label == 'output_40':
            print('MOVHI emitted template ' + self.return_value.string())
            active = False
        else:
            print('MOVHI attr type ' + str(self.return_value))
        return False


class Output(gdb.Breakpoint):
    def __init__(self):
        super().__init__('output_40', internal=True)

    def stop(self):
        global active
        if gdb.parse_and_eval('cfun->decl->decl.name->identifier.id.str').string() != 'SDM_descrambler':
            return False
        insn = gdb.parse_and_eval('insn')
        if int(insn.dereference()['u']['fld'][0]['rtint']) != 118:
            return False
        active = True
        pattern = insn.dereference()['u']['fld'][5]['rtx'].dereference()
        print('MOVHI UID118 pattern %s dst_mode=%s src_mode=%s alternative=%s tune=%s' % (
            pattern['code'], pattern['u']['fld'][0]['rtx'].dereference()['mode'],
            pattern['u']['fld'][1]['rtx'].dereference()['mode'],
            gdb.parse_and_eval('which_alternative'), gdb.parse_and_eval('ix86_tune')))
        print('MOVHI tune MOVX=%s PARTIAL_REG_STALL=%s HIMODE_MATH=%s' % (
            gdb.parse_and_eval('x86_movx & (1 << ix86_tune)'),
            gdb.parse_and_eval('x86_partial_reg_stall & (1 << ix86_tune)'),
            gdb.parse_and_eval('x86_himode_math & (1 << ix86_tune)')))
        Return('output_40')
        return False


class Type(gdb.Breakpoint):
    def __init__(self):
        super().__init__('get_attr_type', internal=True)

    def stop(self):
        if active:
            Return('type')
        return False


Output()
Type()
