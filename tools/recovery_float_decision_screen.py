#!/usr/bin/env python3
"""Nominate local x87 narrowing differences before floating decisions.

This is a bounded basic-block diagnostic, not whole-function equivalence or
input reachability. Reuse the numeric audit's Capstone/ELF loader conventions.
Only explicit ESP-relative spill homes are followed. Joins, calls, unknown
aliases and unsupported x87 operations stop provenance. Compare ordinals are
manual-review aids: they are never asserted to be corresponding expressions.
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
import numeric_consumer_audit  # Preload stdlib dis before ELF imports.
from capstone import Cs, CS_ARCH_X86, CS_MODE_32, CS_AC_WRITE
from capstone.x86 import X86_OP_IMM, X86_OP_MEM, X86_OP_REG
from elftools.elf.elffile import ELFFile

ROOT = Path(__file__).resolve().parents[1]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load(path):
    decoder = Cs(CS_ARCH_X86, CS_MODE_32)
    decoder.detail = True
    with path.open('rb') as stream:
        elf = ELFFile(stream)
        assert elf.elfclass == 32 and elf['e_machine'] == 'EM_386' and elf.little_endian
        symbols = elf.get_section_by_name('.symtab')
        functions = {}
        for sym in symbols.iter_symbols():
            if sym['st_info']['type'] != 'STT_FUNC' or not sym['st_size']:
                continue
            assert sym.name not in functions, ('ambiguous function', sym.name)
            start, size = sym['st_value'], sym['st_size']
            raw = elf.get_section(sym['st_shndx']).data()[start:start+size]
            rows = list(decoder.disasm(raw, start))
            assert sum(i.size for i in rows) == size, ('undecoded function', sym.name)
            functions[sym.name] = rows
    return functions


def slot(inst, op):
    if op.type == X86_OP_MEM and inst.reg_name(op.mem.base) == 'esp' and not op.mem.index:
        return op.mem.disp, op.size
    return None


def st(inst, op, default=0):
    if op.type != X86_OP_REG:
        return default
    name = inst.reg_name(op.reg)
    return int(name[3:-1]) if name.startswith('st(') else default


def blocks(rows):
    """Split at every direct branch destination and transfer boundary."""
    addresses = {i.address for i in rows}
    leaders = {rows[0].address}
    edges, unknown = {}, []
    for i in rows:
        nxt = i.address+i.size
        if i.mnemonic.startswith('j') or i.mnemonic.startswith('loop'):
            if i.operands[0].type == X86_OP_IMM and i.operands[0].imm in addresses:
                edges[i.address] = [i.operands[0].imm]
                leaders.add(i.operands[0].imm)
            else:
                edges[i.address] = []
                unknown.append(i.address)
            if i.mnemonic != 'jmp' and nxt in addresses:
                edges[i.address].append(nxt)
            if nxt in addresses:
                leaders.add(nxt)
        elif i.mnemonic in ('ret', 'call') and nxt in addresses:
            leaders.add(nxt)
    result, current = [], []
    for i in rows:
        if i.address in leaders and current:
            result.append(current)
            current = []
        current.append(i)
    if current:
        result.append(current)
    return result, edges, unknown


def trace(rows):
    base = rows[0].address
    pieces, edges, unknown_edges = blocks(rows)
    decisions, barriers = [], Counter()
    # Tokens are (last arithmetic instruction, narrowing store, width, reload).
    # Unknown operands never acquire an arithmetic origin from a raw copy.
    for piece in pieces:
        fp = [None]*8
        memory, registers = {}, {}
        for index, inst in enumerate(piece):
            name, ops = inst.mnemonic, inst.operands
            at = inst.address-base

            def get(n):
                return fp[n] if n < len(fp) else None

            def pop():
                if fp:
                    fp.pop(0)

            def write_memory(home, token):
                for key in list(memory):
                    if max(key[0], home[0]) < min(key[0]+key[1], home[0]+home[1]):
                        del memory[key]
                memory[home] = token

            def invalidate_register(register):
                groups = [('eax', 'ax', 'al', 'ah'), ('ebx', 'bx', 'bl', 'bh'),
                          ('ecx', 'cx', 'cl', 'ch'), ('edx', 'dx', 'dl', 'dh'),
                          ('esi', 'si'), ('edi', 'di'), ('ebp', 'bp'), ('esp', 'sp')]
                aliases = next((g for g in groups if register in g), (register,))
                for alias in aliases:
                    registers.pop(alias, None)

            if name in ('fld', 'fild', 'fldz', 'fld1', 'fldpi', 'fldl2e', 'fldl2t', 'fldlg2', 'fldln2'):
                token = None
                if name == 'fld':
                    if ops[0].type == X86_OP_REG:
                        token = get(st(inst, ops[0]))
                    elif slot(inst, ops[0]) in memory:
                        token = memory[slot(inst, ops[0])]
                        if token:
                            token = token[:3]+(at,)
                fp.insert(0, token)
            elif name == 'fxch':
                # Capstone exposes both ST(0) and ST(i), although op_str
                # prints only ST(i). The final operand is the exchange peer.
                n = st(inst, ops[-1], 1) if ops else 1
                if n < len(fp):
                    fp[0], fp[n] = fp[n], fp[0]
            elif name in ('fst', 'fstp'):
                if ops[0].type == X86_OP_REG:
                    n = st(inst, ops[0])
                    if n < len(fp):
                        fp[n] = get(0)
                else:
                    home = slot(inst, ops[0])
                    if home and home[1] in (4, 8):
                        token = get(0)
                        write_memory(home, (token[0], at, home[1], None) if token else None)
                    else:
                        memory.clear()
                        barriers['nonstack_store_alias'] += 1
                if name == 'fstp':
                    pop()
            elif name in ('fabs', 'fchs'):
                # Exact sign-bit operations preserve the input's precision.
                # A directly loaded SF coefficient cannot gain excess bits
                # from FABS alone. Keep an earlier arithmetic origin if any.
                pass
            elif name.startswith(('fadd', 'fsub', 'fmul', 'fdiv')) or name in ('fsqrt', 'frndint'):
                n = st(inst, ops[0]) if ops and ops[0].type == X86_OP_REG else 0
                if name.endswith('p') and not ops:
                    n = 1
                if n < len(fp):
                    fp[n] = (at, None, None, None)
                if name.endswith('p'):
                    pop()
            elif name.startswith(('fcom', 'fucom')) or name == 'ftst':
                values = [get(0)]
                if name.endswith('pp'):
                    values.append(get(1))
                elif ops and ops[-1].type == X86_OP_REG:
                    values.append(get(st(inst, ops[-1])))
                elif ops and slot(inst, ops[0]) in memory:
                    token = memory[slot(inst, ops[0])]
                    values.append(token[:3]+(at,) if token else None)
                branch = None
                for following in piece[index+1:index+8]:
                    if following.mnemonic.startswith('j') and following.mnemonic != 'jmp':
                        branch = following.address-base
                        break
                    allowed = following.mnemonic in ('fnstsw', 'fstsw', 'sahf', 'wait', 'nop')
                    allowed |= following.mnemonic == 'fstp' and following.operands[0].type == X86_OP_REG
                    allowed |= following.mnemonic in ('test', 'and') and any(
                        o.type == X86_OP_REG and following.reg_name(o.reg) in ('ah', 'ax', 'eax')
                        for o in following.operands)
                    if not allowed:
                        break
                if branch is not None:
                    values = [v for v in values if v is not None]
                    decisions.append({'compare': at, 'branch': branch,
                                      'instruction': name+' '+inst.op_str,
                                      'arithmetic_values': [dict(producer=v[0], store=v[1],
                                          width=v[2], reload=v[3]) for v in values],
                                      'narrowed': any(v[1] is not None and v[3] is not None for v in values)})
                if name.endswith('pp'):
                    pop(); pop()
                elif name.endswith('p') or name.endswith('ip'):
                    pop()
            elif name == 'mov':
                token = None
                if ops[1].type == X86_OP_REG:
                    token = registers.get(inst.reg_name(ops[1].reg))
                elif slot(inst, ops[1]) in memory:
                    token = memory[slot(inst, ops[1])]
                if ops[0].size != ops[1].size:
                    token = None
                if ops[0].type == X86_OP_REG:
                    register = inst.reg_name(ops[0].reg)
                    invalidate_register(register)
                    registers[register] = token
                    if register in ('esp', 'sp'):
                        memory.clear()
                else:
                    home = slot(inst, ops[0])
                    if home:
                        write_memory(home, token)
                    else:
                        memory.clear()
                        barriers['unknown_integer_store_alias'] += 1
            elif name == 'call' or (name.startswith('f') and name not in ('fnstsw', 'fstsw', 'fnstcw', 'fldcw', 'fnclex')):
                fp, memory, registers = [None]*8, {}, {}
                barriers['call_or_unsupported_x87_'+name] += 1
            else:
                _, writes = inst.regs_access()
                for register in writes:
                    invalidate_register(inst.reg_name(register))
                if any(inst.reg_name(r) == 'esp' for r in writes):
                    memory.clear()
                if any(o.type == X86_OP_MEM and o.access & CS_AC_WRITE for o in ops):
                    memory.clear()
                    barriers['other_store_alias'] += 1
            fp = fp[:8]
    return {'decisions': decisions, 'blocks': len(pieces), 'barriers': dict(barriers),
            'edges': {str(k-base): [v-base for v in vs] for k, vs in edges.items()},
            'unknown_edges': [a-base for a in unknown_edges]}


def controls(tone, reference):
    with tempfile.TemporaryDirectory(prefix='float-decisions-') as temp:
        root = Path(temp)
        start = 'flds 4(%esp)\nfadds 8(%esp)\n'
        decision = 'fcomps 12(%esp)\nfnstsw %ax\nsahf\nja 1f\n1: ret\n'
        forms = {
            'narrow': start+'fstps 16(%esp)\nflds 16(%esp)\n'+decision,
            'omitted': start+decision,
            'f64': start+'fstpl 16(%esp)\nfldl 16(%esp)\n'+decision,
            'dead': start+'fsts 16(%esp)\n'+decision,
            'copy': 'flds 4(%esp)\nfstps 16(%esp)\nflds 16(%esp)\n'+decision,
            'alias': start+'fstps 16(%esp)\nmovl %eax,(%edi)\nflds 16(%esp)\n'+decision,
            'wrong-width': start+'fstps 16(%esp)\nfldl 16(%esp)\n'+decision,
            'bitcopy': start+'fstps 16(%esp)\nmovl 16(%esp),%edx\nmovl %edx,20(%esp)\nflds 20(%esp)\n'+decision,
            'join-refusal': start+'fstps 16(%esp)\ntestl %eax,%eax\nje 2f\n2: flds 16(%esp)\n'+decision,
            'post-decision-store': start+decision.replace('1: ret', '1: fstps 16(%esp)\nret'),
            'exchange': start+'fld1\nfxch %st(1)\nfstps 16(%esp)\nflds 16(%esp)\n'+decision,
            'exact-fabs': 'flds 4(%esp)\nfabs\nfstps 16(%esp)\nflds 16(%esp)\n'+decision,
            'exact-fchs': 'flds 4(%esp)\nfchs\nfstps 16(%esp)\nflds 16(%esp)\n'+decision,
            'arithmetic-fabs': start+'fabs\nfstps 16(%esp)\nflds 16(%esp)\n'+decision,
            'overlap': start+'fstps 16(%esp)\nfldz\nfstps 18(%esp)\nflds 16(%esp)\n'+decision,
            'partial-register': start+'fstps 16(%esp)\nmovl 16(%esp),%edx\nmovw %ax,%dx\nmovl %edx,20(%esp)\nflds 20(%esp)\n'+decision,
            'duplicate-finals': start+decision.replace('1: ret', '1: fsts 16(%esp)\nfstps 20(%esp)\nret'),
            'memory-compare': start+'fstps 16(%esp)\nfld1\nfcomps 16(%esp)\nfnstsw %ax\nsahf\nja 1f\n1: ret\n',
            'stack-rebase': start+'fstps 16(%esp)\nmovl %ebp,%esp\nflds 16(%esp)\n'+decision,
        }
        results = {}
        for label, body in forms.items():
            source, obj = root/(label+'.s'), root/(label+'.o')
            source.write_text('.text\n.type probe,@function\nprobe:\n'+body+'.size probe,.-probe\n')
            subprocess.run(['as', '--32', '-o', str(obj), str(source)], check=True)
            results[label] = sum(d['narrowed'] for d in trace(load(obj)['probe'])['decisions'])
        assert results == {label: int(label in ('narrow', 'f64', 'bitcopy', 'exchange', 'arithmetic-fabs', 'memory-compare')) for label in forms}, results
    results['tone'] = {}
    for label in ('baseline', 'explicit-float-cast', 'double-literal'):
        obj = tone/label/'candidate.o'
        result = trace(load(obj)['TONE_generate'])
        results['tone'][label] = {'sha256': sha(obj), 'trace': result}
    old, cast, repaired = [results['tone'][label]['trace']['decisions'] for label in results['tone']]
    assert [d['narrowed'] for d in old] == [False, False, True]
    assert old == cast
    assert [d['narrowed'] for d in repaired] == [True, False, True]
    original = trace(load(reference)['TONE_generate'])
    assert [d['narrowed'] for d in original['decisions']] == [True, False, True]
    results['original_tone'] = original
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--objects', type=Path, required=True)
    parser.add_argument('--reference', type=Path, required=True)
    parser.add_argument('--census', type=Path, required=True)
    parser.add_argument('--tone-controls', type=Path, required=True)
    parser.add_argument('--max-bytes', type=int, default=8192)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    result = {'controls': controls(args.tone_controls, args.reference), 'counts': {}, 'nominations': [],
              'traces': [],
              'unaligned': [], 'objects': {}, 'config': (args.objects/'.build-config').read_text(),
              'revision': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
              'reference_sha256': sha(args.reference), 'census_sha256': sha(args.census),
              'assembler_controls_version': subprocess.check_output(['as', '--version'], text=True).splitlines()[0],
              'max_original_bytes': args.max_bytes,
              'limits': __doc__}
    exact = set(json.loads(args.census.read_text())['exact_symbols'])
    original = load(args.reference)
    original_traces = {}
    counts = Counter({key: 0 for key in ('objects', 'emitted_bodies', 'without_reference',
        'exact_names_excluded', 'above_size_bound', 'eligible_nonexact_copies',
        'no_original_float_compare', 'traced_copies', 'original_decisions',
        'retained_decisions', 'original_narrowed_decisions', 'retained_narrowed_decisions',
        'unequal_decision_counts', 'nominated_copies', 'no_local_narrowing_deficit')})
    for obj in sorted(args.objects.glob('*.o')):
        counts['objects'] += 1
        result['objects'][obj.name] = sha(obj)
        for name, rows in load(obj).items():
            counts['emitted_bodies'] += 1
            if name not in original:
                counts['without_reference'] += 1
                continue
            if name in exact:
                counts['exact_names_excluded'] += 1
                continue
            ref = original[name]
            if sum(i.size for i in ref) > args.max_bytes:
                counts['above_size_bound'] += 1
                continue
            counts['eligible_nonexact_copies'] += 1
            if not any(i.mnemonic.startswith(('fcom', 'fucom')) or i.mnemonic == 'ftst' for i in ref):
                counts['no_original_float_compare'] += 1
                continue
            if name not in original_traces:
                original_traces[name] = trace(ref)
            a, b = original_traces[name], trace(rows)
            counts['traced_copies'] += 1
            counts['original_decisions'] += len(a['decisions'])
            counts['retained_decisions'] += len(b['decisions'])
            counts['original_narrowed_decisions'] += sum(d['narrowed'] for d in a['decisions'])
            counts['retained_narrowed_decisions'] += sum(d['narrowed'] for d in b['decisions'])
            row = {'symbol': name, 'object': obj.name, 'original': a, 'retained': b,
                   'correspondence': 'unproven; inspect original/retained arithmetic operands and complete CFG'}
            result['traces'].append(row)
            if len(a['decisions']) != len(b['decisions']):
                counts['unequal_decision_counts'] += 1
                result['unaligned'].append(row)
            elif sum(d['narrowed'] for d in a['decisions']) > sum(d['narrowed'] for d in b['decisions']):
                counts['nominated_copies'] += 1
                result['nominations'].append(row)
            else:
                counts['no_local_narrowing_deficit'] += 1
    assert counts['objects'] and counts['traced_copies'] and counts['original_narrowed_decisions']
    result['counts'] = dict(counts)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps(dict(counts), sort_keys=True))
    print('Controls: 19 assembled cases; original bug and cast-only positive, repaired tone negative')
    for row in result['nominations']:
        print(row['object'], row['symbol'], [d['compare'] for d in row['original']['decisions'] if d['narrowed']])


if __name__ == '__main__':
    main()
