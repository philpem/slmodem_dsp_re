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
from pathlib import Path
import subprocess
import tempfile
import storage_content_audit as raw
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
    return '%d/%d' % (answer.numerator, answer.denominator)


def inventory(path):
    source = raw.inventory(path)
    decoder = Cs(CS_ARCH_X86, CS_MODE_32)
    decoder.detail = True
    functions, code, ambiguous = {}, {}, set()
    with Path(path).open('rb') as stream:
        elf = ELFFile(stream)
        for symbol in elf.get_section_by_name('.symtab').iter_symbols():
            if symbol['st_info']['type'] != 'STT_FUNC' or not symbol['st_size']:
                continue
            section = elf.get_section(symbol['st_shndx'])
            start, size = symbol['st_value'], symbol['st_size']
            if symbol.name in functions:
                ambiguous.add(symbol.name)
            functions[symbol.name] = list(decoder.disasm(section.data()[start:start+size], start))
    result, unknown = defaultdict(Counter), []
    evidence = defaultdict(list)
    for row in source['pool_locations']:
        name, off = row['function'], row['relocation_offset']
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
            if instruction.mnemonic in ('fld','fadd','fmul','fsub','fsubr','fdiv','fdivr','fcom','fcomp','fild') and width in (4, 8, 10) and width <= row['width']:
                key = ('%d/1' % int.from_bytes(bytes.fromhex(row['payload'])[:width], 'little', signed=True)) if instruction.mnemonic == 'fild' else value(bytes.fromhex(row['payload']), width)
                result[name][key] += 1
                record['exact_value'] = key
                evidence[name].append(record)
                continue
        unknown.append(record)
    return result, unknown, evidence


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
        assert build('positive-zero',4,'0.0') != build('negative-zero',4,'-0.0')
        assert value(bytes.fromhex('6e12033b'),4) != value(bytes.fromhex('6f12033b'),4)
    return {'float_double_equal':True, 'double_extended_equal':True,
            'changed_value_rejected':True, 'integer_conversion_equal':True, 'signed_zero_distinct':True, 'one_ULP_distinct':True,
            'assembler':subprocess.check_output(['as','--version'],text=True).splitlines()[0],
            'scope':'assembled detector controls; not reconstruction compilations'}


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('reference', type=Path)
    ap.add_argument('candidate', type=Path)
    ap.add_argument('--json', type=Path, required=True)
    args = ap.parse_args()
    checked = controls()
    a, ua, ea = inventory(args.reference)
    b, ub, eb = inventory(args.candidate)
    shared = sorted(a.keys() & b.keys())
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
    report = {'shared_numeric_functions':len(shared),'equal_value_multisets':len(exact),
              'same_values_different_counts':sets,'different_value_sets':different,
              'reference_only_functions':sorted(a.keys()-b.keys()),'candidate_only_functions':sorted(b.keys()-a.keys()),
              'unclassified_reference':ua,'unclassified_candidate':ub,'controls':checked,
              'limits':'Only decoded x87 memory operands in numeric-pool sections; exact values, not equivalent expressions. '
                       'Equal sets/multisets do not prove operation, path, order, multiplicity, narrowing or whole-function behavior.'}
    args.json.write_text(json.dumps(report,indent=2)+'\n')
    print('%d shared numeric functions; %d equal value multisets; %d same-value/count differences; %d different value sets; %d/%d unclassified consumers' %
          (len(shared),len(exact),len(sets),len(different),len(ua),len(ub)))
    for row in different:
        print(row['function'], row['reference_only'], row['candidate_only'])


if __name__ == '__main__':
    main()
