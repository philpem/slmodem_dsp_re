#!/usr/bin/env python3
"""Compare numeric-pool consumers by exact value and decoded operand width.

Equal value sets explain storage-format differences only. Counts, operations,
guards and use order remain independent obligations; no behavior proof here.
"""
import argparse
from bisect import bisect_right
from collections import Counter, defaultdict
from fractions import Fraction
import json
import hashlib
from pathlib import Path
import subprocess
import tempfile
import sys
_saved_path = sys.path[:]
sys.path[:] = [p for p in sys.path if Path(p or '.').resolve() != Path(__file__).resolve().parent]
import dis
sys.path[:] = _saved_path
from capstone import Cs, CS_ARCH_X86, CS_MODE_32
from capstone.x86 import X86_OP_MEM
from elftools.elf.elffile import ELFFile


def value(data, width):
    bits = int.from_bytes(data[:width], 'little')
    if width in (4, 8):
        frac_bits, exp_bits, bias = (23, 8, 127) if width == 4 else (52, 11, 1023)
        sign = bits >> (width*8-1)
        exponent = (bits >> frac_bits) & ((1 << exp_bits)-1)
        fraction = bits & ((1 << frac_bits)-1)
        if exponent == (1 << exp_bits)-1:
            return 'nan:%d:%x' % (width, bits) if fraction else 'infinity:%d' % sign
        significand = fraction + ((1 << frac_bits) if exponent else 0)
        power = (exponent or 1)-bias-frac_bits
    elif width == 10:
        sign = bits >> 79
        exponent = (bits >> 64) & 0x7fff
        significand = bits & ((1 << 64)-1)
        if exponent == 0x7fff:
            return 'infinity:%d' % sign if significand == 1 << 63 else 'nan:10:%x' % bits
        # Noncanonical extended encodings are preserved, not normalised away.
        if (exponent and not significand & (1 << 63)) or (not exponent and significand & (1 << 63)):
            return 'noncanonical:10:%x' % bits
        power = (exponent or 1)-16383-63
    else:
        raise ValueError(width)
    if not significand:
        return 'zero:%d' % sign
    answer = Fraction(significand) * (Fraction(2) ** power)
    if sign:
        answer = -answer
    if max(answer.numerator.bit_length(), answer.denominator.bit_length()) > 10000:
        return '%#x/%#x' % (answer.numerator, answer.denominator)
    return '%d/%d' % (answer.numerator, answer.denominator)


def inventory(path):
    # Extract numeric relocations independently of pool entry width. A legal
    # FLDS at +12 in a 16-byte entry reads four bytes, not another 16 bytes.
    decoder = Cs(CS_ARCH_X86, CS_MODE_32)
    decoder.detail = True
    functions, code, ambiguous, owners = {}, {}, set(), defaultdict(list)
    locations = []
    with Path(path).open('rb') as stream:
        elf = ELFFile(stream)
        assert elf.elfclass == 32 and elf.little_endian and elf['e_machine'] == 'EM_386' and elf['e_type'] == 'ET_REL', 'requires little-endian i386 ELF32 ET_REL'
        assert not any(section['sh_type'] == 'SHT_RELA' for section in elf.iter_sections()), 'SHT_RELA is unsupported; numeric audit requires SHT_REL'
        symbols = elf.get_section_by_name('.symtab')
        for symbol in symbols.iter_symbols():
            if symbol['st_info']['type'] != 'STT_FUNC' or not symbol['st_size']:
                continue
            index = symbol['st_shndx']
            if not isinstance(index, int):
                continue
            section = elf.get_section(index)
            start, size = symbol['st_value'], symbol['st_size']
            if symbol.name in functions:
                ambiguous.add(symbol.name)
            functions[symbol.name] = list(decoder.disasm(section.data()[start:start+size], start))
            owners[index].append((start, start+size, symbol.name))
        for section in elf.iter_sections():
            if section['sh_type'] != 'SHT_REL':
                continue
            origin = elf.get_section(section['sh_info'])
            if not origin['sh_flags'] & 4:
                continue
            data = origin.data()
            targets = elf.get_section(section['sh_link'])
            for relocation in section.iter_relocations():
                target = targets.get_symbol(relocation['r_info_sym'])
                if not isinstance(target['st_shndx'], int):
                    continue
                pool = elf.get_section(target['st_shndx'])
                if not pool.name.startswith('.rodata.cst'):
                    continue
                off = relocation['r_offset']
                matches = [name for lo, hi, name in owners[section['sh_info']] if lo <= off < hi]
                row = {'function': matches[0] if len(matches) == 1 else None,
                       'relocation_offset': off, 'pool_section': pool.name,
                       'width': pool['sh_entsize'] or int(pool.name.rsplit('cst', 1)[1])}
                if len(matches) != 1:
                    row['reason'] = 'missing or ambiguous function owner'
                elif relocation['r_info_type'] != 1 or off+4 > len(data):
                    row['reason'] = 'unsupported or out-of-bounds relocation'
                else:
                    addend = int.from_bytes(data[off:off+4], 'little', signed=True)
                    start = target['st_value'] + addend
                    row['pool_offset'] = start
                    if not 0 <= start < pool['sh_size']:
                        row['reason'] = 'target outside numeric pool'
                    else:
                        row['payload'] = pool.data()[start:start+max(row['width'], 10)].hex()
                locations.append(row)
    result, unknown = defaultdict(Counter), []
    evidence = defaultdict(list)
    for row in locations:
        name, off = row['function'], row['relocation_offset']
        if 'reason' in row:
            unknown.append(row)
            continue
        if name in ambiguous:
            unknown.append(dict(row, reason='duplicate function name'))
            continue
        if name not in code:
            instructions = functions[name]
            code[name] = ([i.address for i in instructions], instructions)
        starts, instructions = code[name]
        at = bisect_right(starts, off)-1
        instruction = instructions[at] if at >= 0 else None
        record = dict(row)
        if instruction is not None and instruction.address <= off < instruction.address+instruction.size:
            memory = [op for op in instruction.operands if op.type == X86_OP_MEM]
            width = memory[0].size if len(memory) == 1 else 0
            record.update(instruction=instruction.mnemonic+' '+instruction.op_str, operand_width=width)
            if instruction.mnemonic in ('fld','fadd','fmul','fsub','fsubr','fdiv','fdivr','fcom','fcomp','fild') and width in (4, 8, 10):
                payload = bytes.fromhex(row['payload'])
                if len(payload) < width:
                    unknown.append(dict(record, reason='decoded operand crosses pool boundary'))
                    continue
                if instruction.mnemonic == 'fild':
                    integer = int.from_bytes(payload[:width], 'little', signed=True)
                    key = '%d/1' % integer if integer else 'zero:0'
                else:
                    key = value(payload, width)
                result[name][key] += 1
                record['exact_value'] = key
                evidence[name].append(record)
                continue
        unknown.append(dict(record, reason='unclassified instruction or operand'))
    classified = sum(sum(c.values()) for c in result.values())
    assert classified + len(unknown) == len(locations)
    denominator = {'pool_relocations': len(locations), 'classified': classified, 'unclassified': len(unknown)}
    return result, unknown, evidence, denominator


def controls():
    with tempfile.TemporaryDirectory(prefix='numeric-consumer-') as temp:
        root = Path(temp)
        def build(label, width, literal):
            source, obj = root/(label+'.s'), root/(label+'.o')
            directive, opcode, padding = {4:('.float', 'flds', ''), 8:('.double', 'fldl', ''),
                                         10:('.tfloat', 'fldt', '.zero 6\n'), -4:('.long', 'fildl', '')}[width]
            entry = 16 if width == 10 else abs(width)
            source.write_text('.text\n.globl consume\n.type consume,@function\nconsume:\n '+opcode+
                              ' .LC0\n ret\n.size consume,.-consume\n.section .rodata.cst%d,"aM",@progbits,%d\n.LC0:\n%s %s\n%s' %
                              (entry,entry,directive,literal,padding))
            subprocess.run(['as','--32',str(source),'-o',str(obj)], check=True)
            return inventory(obj)[0]['consume']
        a, b, c = build('float',4,'1.5'), build('double',8,'1.5'), build('extended',10,'1.5')
        assert a == b == c
        assert a != build('changed',4,'1.5001')
        assert build('integer',-4,'1') == build('one',4,'1.0')
        assert build('integer-zero',-4,'0') == build('zero',4,'0.0')
        assert build('positive-zero',4,'0.0') != build('negative-zero',4,'-0.0')
        assert value(bytes.fromhex('6e12033b'),4) != value(bytes.fromhex('6f12033b'),4)
        assert value(bytes.fromhex('0000000000000080fe7f'),10).startswith('0x')
        assert value(bytes.fromhex('01000000000000000000'),10).startswith('0x1/')
        source, obj = root/'interior.s', root/'interior.o'
        source.write_text('.text\n.globl consume\n.type consume,@function\nconsume:\n flds .LC0+12\n ret\n.size consume,.-consume\n.section .rodata.cst16,"aM",@progbits,16\n.LC0:\n.float 0,0,0,1.5\n')
        subprocess.run(['as','--32',str(source),'-o',str(obj)], check=True)
        interior = inventory(obj)
        assert interior[0]['consume'] == a
        assert interior[3] == {'pool_relocations':1,'classified':1,'unclassified':0}
        source.write_text(source.read_text().replace('.LC0+12', '.LC0+14'))
        subprocess.run(['as','--32',str(source),'-o',str(obj)], check=True)
        invalid = inventory(obj)
        assert invalid[3] == {'pool_relocations':1,'classified':0,'unclassified':1}
        subprocess.run(['as','--64',str(source),'-o',str(obj)], check=True)
        try:
            inventory(obj)
        except AssertionError as error:
            assert 'i386 ELF32 ET_REL' in str(error)
        else:
            raise AssertionError('unsupported x86_64 input accepted')
    return {'float_double_equal':True, 'double_extended_equal':True,
            'changed_value_rejected':True, 'integer_conversion_equal':True, 'signed_zero_distinct':True, 'one_ULP_distinct':True,
            'large_extended_finite_serialized':True, 'extended_subnormal_serialized':True,
            'integer_zero_equal':True, 'interior_pool_load_counted':True, 'cross_boundary_load_unclassified':True,
            'unsupported_ELF_rejected':True,
            'assembler':subprocess.check_output(['as','--version'],text=True).splitlines()[0],
            'scope':'assembled i386 operand tests plus raw extended/one-ULP controls; not reconstruction compilations'}


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('reference', type=Path)
    ap.add_argument('candidate', type=Path)
    ap.add_argument('--json', type=Path, required=True)
    args = ap.parse_args()
    checked = controls()
    a, ua, ea, da = inventory(args.reference)
    b, ub, eb, db = inventory(args.candidate)
    shared = sorted(a.keys() & b.keys())
    assert shared, "no shared numeric consumers: comparison is unmeasured"
    exact, sets, different = [], [], []
    for name in shared:
        if a[name] == b[name]:
            exact.append(name)
        elif a[name].keys() == b[name].keys():
            sets.append({'function':name,'reference_counts':dict(a[name]),'candidate_counts':dict(b[name])})
        else:
            different.append({'function':name,'reference_only':sorted(a[name].keys()-b[name].keys()),
                              'candidate_only':sorted(b[name].keys()-a[name].keys()),
                              'reference_consumers':ea[name], 'candidate_consumers':eb[name]})
    report = {'inputs': {label: {'path': str(path.resolve()), 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()} for label, path in [('reference', args.reference), ('candidate', args.candidate)]},
              'shared_numeric_functions':len(shared),'equal_value_multisets':len(exact),
              'same_values_different_counts':sets,'different_value_sets':different,
              'reference_only_functions':sorted(a.keys()-b.keys()),'candidate_only_functions':sorted(b.keys()-a.keys()),
              'unclassified_reference':ua,'unclassified_candidate':ub,'controls':checked,
              'reference_consumer_denominator':da,'candidate_consumer_denominator':db,
              'limits':'Only little-endian i386 ELF32 ET_REL with SHT_REL relocations; decoded x87 memory operands in numeric-pool sections; exact values, not equivalent expressions. '
                       'Equal sets/multisets do not prove operation, path, order, multiplicity, narrowing or whole-function behavior.'}
    args.json.write_text(json.dumps(report,indent=2)+'\n')
    print('%d shared numeric functions; %d equal value multisets; %d same-value/count differences; %d different value sets; %d/%d unclassified consumers' %
          (len(shared),len(exact),len(sets),len(different),len(ua),len(ub)))
    for row in different:
        print(row['function'], row['reference_only'], row['candidate_only'])


if __name__ == '__main__':
    main()
