#!/usr/bin/env python3
"""Cross two witnessed V92 partial recoveries; no new spelling/profile search.

Frozen baseline: 8e216110a. Case ownership is the previously tested common-tail
control; table order restores the original 320-byte block under retained flags.
Default-path probes inspect native guards/accesses, not indeterminate values or
public reachability. Valid reset-built histories remain the period oracle.
"""
import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path
import playbook_small_patterns as d
from recovery_v92_case_owners import owned
from gcc3_value_carriers_audit import inspect
from capstone import Cs, CS_ARCH_X86, CS_MODE_32
from capstone.x86 import X86_OP_IMM, X86_OP_MEM, X86_OP_REG

PREFIX = '_ZN21V92ConvolutionEncoder'
PROCESS = PREFIX + '7processEPi'
COSET = PREFIX + '14cosetMapping4DE'
SUBSET = PREFIX + '16subsetLabelTableE'
MODES = (-2147483648, -100, -1, 3, 100, 2147483647)


def swap_tables(source):
    blocks = [re.search(r'int V92ConvolutionEncoder::' + name +
                       r'\[\d+\] = \{.*?\n\};', source, re.S)
              for name in ('cosetMapping4D', 'subsetLabelTable')]
    first, second = blocks
    assert first and second and first.end() < second.start()
    return (source[:first.start()] + second[0] +
            source[first.end():second.start()] + first[0] + source[second.end():])


def variants(path, source):
    cases = owned(source, False)
    cells = {'baseline': source, 'case-owned': cases,
             'native-tables': swap_tables(source),
             'case-owned-native-tables': swap_tables(cases)}
    assert len(set(cells.values())) == 4
    return cells


def default_path(raw, mode):
    """Bounded selector walk. Refuse unknown flags/edges; never evaluate junk."""
    decoder = Cs(CS_ARCH_X86, CS_MODE_32)
    decoder.detail = True
    insns = list(decoder.disasm(raw, 0))
    starts = []
    for n, insn in enumerate(insns):
        ops = insn.operands
        if (n and insn.mnemonic == 'cmp' and len(ops) == 2 and
                ops[0].type == X86_OP_REG and ops[1].type == X86_OP_IMM and
                ops[1].imm == 1):
            # Scheduling may interleave unrelated inline-helper operations.
            # Require the last definition, rather than the adjacent instruction.
            for prior in reversed(insns[max(0, n - 12):n]):
                if ops[0].reg not in prior.regs_access()[1]:
                    continue
                if (prior.mnemonic == 'mov' and prior.operands[0].type == X86_OP_REG
                        and prior.operands[0].reg == ops[0].reg and
                        prior.operands[1].type == X86_OP_MEM and
                        prior.operands[1].mem.disp == 0 and not prior.operands[1].mem.index
                        and prior.operands[1].mem.base):
                    starts.append((n, ops[0].reg, prior.operands[1].mem.base))
                break
    assert len(starts) == 1, 'ambiguous mode selector'
    index, selector, owner = starts[0]
    registers = {selector: mode}
    owners = {owner}
    stack_home = None
    for prior in reversed(insns[:index]):
        if owner not in prior.regs_access()[1]:
            continue
        if (prior.mnemonic == 'mov' and prior.operands[0].type == X86_OP_REG
                and prior.operands[0].reg == owner and prior.operands[1].type == X86_OP_MEM
                and prior.reg_name(prior.operands[1].mem.base) == 'esp'
                and not prior.operands[1].mem.index):
            stack_home = prior.operands[1].mem.disp
        break
    addresses = {insn.address: n for n, insn in enumerate(insns)}
    flags = None
    accesses, stores, path = [], [], []

    def value(op):
        if op.type == X86_OP_IMM:
            return op.imm
        if op.type == X86_OP_REG:
            return registers.get(op.reg)
        return None

    for _ in range(256):
        insn = insns[index]
        op = insn.operands
        mnemonic = insn.mnemonic
        path.append(insn.address)
        if mnemonic.startswith('ret'):
            return {'table_reads': accesses, 'state_stores': stores, 'path': path}
        if mnemonic in ('cmp', 'test'):
            left, right = value(op[0]), value(op[1])
            assert left is not None and right is not None, 'unknown selector flags'
            flags = (left, right) if mnemonic == 'cmp' else (left & right, 0)
        elif mnemonic.startswith('j'):
            assert len(op) == 1 and op[0].type == X86_OP_IMM
            if mnemonic == 'jmp':
                taken = True
            else:
                assert flags is not None, 'branch on unknown flags'
                a, b = flags
                conditions = {'je': a == b, 'jne': a != b, 'jle': a <= b,
                              'jg': a > b, 'jl': a < b, 'jge': a >= b}
                assert mnemonic in conditions, 'unsupported selector branch'
                taken = conditions[mnemonic]
            if taken:
                assert op[0].imm in addresses, 'edge leaves function'
                index = addresses[op[0].imm]
                continue
        elif mnemonic == 'mov':
            if op[0].type == X86_OP_REG:
                registers[op[0].reg] = value(op[1])
                if op[1].type == X86_OP_MEM and op[1].mem.base in owners:
                    disp = op[1].mem.disp
                    if disp in (8, 0x1008):
                        accesses.append([insn.address, 'output' if disp == 8 else 'nextState'])
                alias = (op[1].type == X86_OP_REG and op[1].reg in owners) or (
                    stack_home is not None and op[1].type == X86_OP_MEM and
                    insn.reg_name(op[1].mem.base) == 'esp' and
                    op[1].mem.disp == stack_home and not op[1].mem.index)
                owners.discard(op[0].reg)
                if alias:
                    owners.add(op[0].reg)
            elif op[0].type == X86_OP_MEM and op[0].mem.base in owners and op[0].mem.disp == 4:
                stores.append(insn.address)
        else:
            assert mnemonic not in ('call', 'syscall', 'int'), 'unsupported path effect'
            # Arithmetic on an uninitialised index is deliberately not evaluated.
            # A later branch depending on it must be refused, not guessed.
            for reg in insn.regs_access()[1]:
                registers[reg] = None
                owners.discard(reg)
                if insn.reg_name(reg) == 'eflags':
                    flags = None
        index += 1
        assert index < len(insns)
    raise AssertionError('selector walk did not terminate')


def data(path):
    from elftools.elf.elffile import ELFFile
    with Path(path).open('rb') as stream:
        return ELFFile(stream).get_section_by_name('.data').data()


def audit():
    root = d.ROOT / 'build' / d.OUT_NAME
    result = json.loads((root / 'results.json').read_text())
    cells = result['families']['V92ConvEncoder']['cells']
    assert set(cells) == {'baseline', 'case-owned', 'native-tables', 'case-owned-native-tables'}
    folder = root / 'V92ConvEncoder'
    base = inspect(folder / 'baseline/candidate.o')
    assert cells['baseline']['baseline_reproduced']
    original_data = data(d.b.BLOB)[0x6ac0:0x6c00]
    assert len(original_data) == 320
    original_paths = [default_path(d.b.body(d.b.BLOB, PROCESS)[0], mode) for mode in MODES]
    assert all(not p['table_reads'] and len(p['state_stores']) == 1 for p in original_paths)
    report = {'original_default_paths': original_paths, 'modes': MODES, 'cells': {}}
    for label, cell in cells.items():
        path = folder / label / 'candidate.o'
        obj = inspect(path)
        case, native = label.startswith('case-owned'), 'native-tables' in label
        records = {name: value[:] for name, value in base['records'].items()}
        allocated = dict(base['allocated'])
        if native:
            records[COSET][-2], records[SUBSET][-2] = 0, 256
            allocated['.data'] = original_data.hex()
            assert data(path) == original_data
        assert obj['records'] == records
        assert obj['allocated'] == allocated
        for key in ('nobits', 'relocations'):
            assert obj[key] == base[key]
        assert len(cell['functions']) == len(cell['verdicts']) == 8
        assert cell.get('changed_bodies', []) == ([PROCESS] if case else [])
        assert not cell.get('gains', []) and not cell.get('losses', [])
        assert cell['verdicts'][PROCESS] == ['SIZE', 218 if case else 263]
        paths = [default_path(d.b.body(str(path), PROCESS)[0], mode) for mode in MODES]
        assert all(len(p['state_stores']) == 1 for p in paths)
        assert all(len(p['table_reads']) == (0 if case else 2) for p in paths)
        report['cells'][label] = {'paths': paths, 'verdict': cell['verdicts'][PROCESS],
                                 'gains': [], 'losses': [], 'native_data': native}
    selected = bytearray(d.b.body(str(folder / 'case-owned-native-tables/candidate.o'), PROCESS)[0])
    decoder = Cs(CS_ARCH_X86, CS_MODE_32)
    branch = next(insn for insn in decoder.disasm(selected, 0) if insn.mnemonic == 'je')
    assert selected[branch.address] == 0x74
    selected[branch.address] = 0x75
    assert len(default_path(bytes(selected), 3)['table_reads']) == 2, 'changed guard escaped detection'
    report['changed_guard_control_detected'] = True
    (root / 'audit.json').write_text(json.dumps(report, indent=2) + '\n')
    print('4 cells / 32 body verdicts; 6 original and 24 candidate default paths; '
          'baseline missing-guard and altered native branch controls fire; '
          'native320-byte data block matches; no exact gains/losses')


def production_control():
    root = d.ROOT / 'build' / d.OUT_NAME
    result = json.loads((root / 'results.json').read_text())
    label = 'case-owned-native-tables'
    out = root / 'V92ConvEncoder/production-control'
    out.mkdir(exist_ok=True)
    (out / 'V92ConvEncoder.cpp').write_bytes((d.ROOT / d.SOURCE_PATHS[0]).read_bytes())
    command = [s.replace('/V92ConvEncoder/' + label, '/V92ConvEncoder/production-control')
               for s in result['families']['V92ConvEncoder']['cells'][label]['command']]
    with (out / 'compile.log').open('w') as log:
        subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True)
    assert (out / 'candidate.o').read_bytes() == (root / 'V92ConvEncoder' / label / 'candidate.o').read_bytes()
    print('Production full object raw-matches combined selected control')


def sensitivity():
    """Recheck moved mutation meaning with period objects and isolated outputs."""
    root = d.ROOT / 'build' / d.OUT_NAME
    results = json.loads((root / 'results.json').read_text())
    template = results['families']['V92ConvEncoder']['cells']['case-owned-native-tables']['command']
    source = (d.ROOT / d.SOURCE_PATHS[0]).read_text()
    rows = [row for row in json.loads((d.ROOT / 'test/mutations/v92convenc.json').read_text())
            if row['label'].startswith('process ')]
    assert len(rows) == 3
    report = []
    for index, row in enumerate(rows):
        assert source.count(row['find']) == 1
        label = 'mutation-' + str(index)
        folder = root / 'V92ConvEncoder' / label
        folder.mkdir(exist_ok=True)
        (folder / 'V92ConvEncoder.cpp').write_text(source.replace(row['find'], row['replace']))
        command = [s.replace('/V92ConvEncoder/case-owned-native-tables', '/V92ConvEncoder/' + label)
                   for s in template]
        with (folder / 'compile.log').open('w') as log:
            subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True)
        if 'equivalent' in row:
            assert (folder / 'candidate.o').read_bytes() == (root / 'V92ConvEncoder/production-control/candidate.o').read_bytes()
            report.append({'label': row['label'], 'equivalent_raw_object': True, 'command': command})
            continue
        out = root / ('period-' + label)
        out.mkdir(exist_ok=True)
        period = d.ROOT / 'build/period'
        stable = list(period.glob('src_*.o')) + list(period.glob('test_harness_*.o'))
        assert len(list(period.glob('src_*.o'))) == 300
        stable.append(period / 'test_unit_t_v92convmapper.o')
        for path in stable:
            shutil.copy2(path, out / path.name)
        obj = out / 'src_pump_v90_V92ConvEncoder.o'
        shutil.copy2(folder / 'candidate.o', obj)
        run = ['make', 'period', 'T=t_v92convmapper', 'J=4']
        with (out / 'gate.log').open('w') as log:
            status = subprocess.run(run, cwd=d.ROOT,
                env=dict(os.environ, PERIOD_OUT=str(out.relative_to(d.ROOT))),
                stdout=log, stderr=subprocess.STDOUT).returncode
        assert status != 0 and 'FAIL' in (out / 't_v92convmapper.run.log').read_text()
        assert obj.read_bytes() == (folder / 'candidate.o').read_bytes(), 'mutant overwritten'
        report.append({'label': row['label'], 'period_exit': status,
                       'caught': True, 'compile_command': command, 'run_command': run})
    (root / 'period-sensitivity.json').write_text(json.dumps(report, indent=2) + '\n')
    print('2/2 relocated process mutants caught by period fixture; equivalent index full object raw-identical')


if __name__ == '__main__':
    d.REV = '8e216110a0d31fec024e50b28af4b80d91f0f881'
    d.OUT_NAME = 'v92-boundary-adoption'
    d.SOURCE_PATHS = ('src/pump/v90/V92ConvEncoder.cpp',)
    d.DUMP_FLAGS = ('-da', '-v', '-save-temps')
    d.CELL_FLAGS = lambda path, label: ()
    d.variants = variants
    if sys.argv[1:] == ['--audit']:
        audit()
    elif sys.argv[1:] == ['--production-control']:
        production_control()
    elif sys.argv[1:] == ['--sensitivity']:
        sensitivity()
    else:
        d.main()
        audit()
