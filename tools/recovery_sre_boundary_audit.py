#!/usr/bin/env python3
"""Audit the bounded SRE winners; trace first EAX use with explicit refusals."""
import argparse
import json
from pathlib import Path
import re

import playbook_small_patterns as d
from narrow_call_boundary_screen import bodies
from elftools.elf.elffile import ELFFile


def first_return_use(rows, call_address, budget=256, direct_edges=()):
    """Explore direct branches until first EAX use; do not infer an ABI/type."""
    positions = {int(row['address'], 16): i for i, row in enumerate(rows)}
    start = positions[int(call_address, 16)] + 1
    pending = [(start, ())]
    outcomes, refusals = [], []
    states = 0
    while pending:
        states += 1
        if states > budget:
            refusals.append('state budget before first use')
            break
        at, path = pending.pop()
        if at >= len(rows):
            refusals.append('fell outside body')
            continue
        if at in path or len(path) >= budget:
            refusals.append('cycle or path budget before first use')
            continue
        path = path + (at,)
        row = rows[at]
        mnemonic, operands = row['mnemonic'], row['operands']
        if mnemonic == 'call':
            refusals.append('intervening call clobbers return carrier')
            continue
        if mnemonic.startswith('ret'):
            refusals.append('exit with no first-use contract')
            continue
        if mnemonic.startswith('j') or mnemonic.startswith('loop'):
            if operands.startswith('*'):
                refusals.append('indirect jump/table destinations unproved')
                continue
            target = re.match(r'^([0-9a-f]+)(?:\s|$)', operands)
            destination = positions.get(int(target[1], 16)) if target else None
            if destination is None:
                refusals.append('external/unknown branch destination')
                continue
            if (row['address'], rows[destination]['address']) not in direct_edges:
                refusals.append('branch relocation/ownership not proved')
                continue
            pending.append((destination, path))
            if mnemonic != 'jmp':
                pending.append((at + 1, path))
            continue
        if mnemonic == 'cwtl':
            kind = 'signed-word-extension'
        elif mnemonic in ('movswl', 'movzwl') and operands.split(',')[0] == '%ax':
            kind = 'signed-word-extension' if mnemonic == 'movswl' else 'unsigned-word-extension'
        elif re.search(r'%(eax|ax|al|ah)\b', operands):
            refusals.append('other carrier use/write requires independent analysis')
            continue
        elif mnemonic not in ('cmp', 'cmpw', 'cmpl', 'test', 'nop', 'lea') and \
                not operands.rsplit(',', 1)[-1].startswith('%'):
            refusals.append('memory write: alias effects not modelled')
            continue
        elif mnemonic not in ('mov', 'movswl', 'movzwl', 'movsbl', 'movzbl', 'lea',
                              'xor', 'cmp', 'cmpw', 'cmpl', 'test', 'nop', 'add', 'sub'):
            refusals.append('unsupported instruction before use: ' + mnemonic)
            continue
        else:
            pending.append((at + 1, path))
            continue
        outcomes.append({'kind': kind, 'instruction': row,
                         'path': [rows[i]['address'] for i in path]})
    return {'outcomes': outcomes, 'refusals': sorted(set(refusals))}


def self_test():
    def rows(instructions):
        return [{'address': str(i), 'mnemonic': m, 'operands': o}
                for i, (m, o) in enumerate(instructions)]
    cases = [
        ('conditional paths', [('call', 'callee'), ('je', '3'), ('movzwl', '%ax,%eax'),
                               ('movzwl', '%ax,%ebx')], 2, False),
        ('loop refusal', [('call', 'callee'), ('je', '3'), ('jmp', '1'),
                          ('movzwl', '%ax,%eax')], 1, True),
        ('table refusal', [('call', 'callee'), ('jmp', '*0x0(,%edx,4)')], 0, True),
        ('alias refusal', [('call', 'callee'), ('mov', '%ecx,(%edx)'),
                           ('movzwl', '%ax,%eax')], 0, True),
        ('intervening call', [('call', 'callee'), ('call', 'other'),
                              ('movzwl', '%ax,%eax')], 0, True),
        ('signed positive', [('call', 'callee'), ('cwtl', '')], 1, False),
        ('overwrite refusal', [('call', 'callee'), ('mov', '$0,%eax'),
                               ('movzwl', '%ax,%eax')], 0, True),
    ]
    for name, instructions, count, refused in cases:
        # Synthetic labels supply independent edge ownership. Production
        # callers must prove relocation-free internal edges before granting it.
        edges = {(str(i), o) for i, (m, o) in enumerate(instructions)
                 if m.startswith('j') and not o.startswith('*')}
        result = first_return_use(rows(instructions), '0', direct_edges=edges)
        assert len(result['outcomes']) == count and bool(result['refusals']) == refused, name
    result = first_return_use(rows(cases[0][1]), '0')
    assert result['refusals'] == ['branch relocation/ownership not proved']
    result = first_return_use(rows([('call', 'callee'), ('mov', '%ecx,%edx'),
                                   ('movzwl', '%ax,%eax')]), '0', budget=1)
    assert result['refusals'] == ['state budget before first use']
    print('SRE return-use controls: 9 passed; conditional/signed positives, '
          'cycle/table/alias/call/overwrite/unproved-edge/budget refusals')


def nontext(path):
    with Path(path).open('rb') as stream:
        elf = ELFFile(stream)
        sections = [(s.name, s['sh_type'], s['sh_flags'], s['sh_size'],
                     s['sh_addralign'], s['sh_entsize'],
                     '' if s['sh_type'] == 'SHT_NOBITS' else s.data().hex())
                    for s in elf.iter_sections()
                    if (s['sh_flags'] & 2 and not s['sh_flags'] & 4) or
                    s.name == '.note.GNU-stack']
        symbols = []
        objects = []
        for s in elf.get_section_by_name('.symtab').iter_symbols():
            owner = s['st_shndx']
            owner = elf.get_section(owner).name if isinstance(owner, int) else owner
            symbols.append((s.name, s['st_info']['type'], s['st_info']['bind'],
                            s['st_other']['visibility'], owner))
            if s['st_info']['type'] == 'STT_OBJECT':
                objects.append((symbols[-1], s['st_value'], s['st_size']))
        return sections, symbols, objects


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--self-test', action='store_true')
    ap.add_argument('--output', type=Path)
    args = ap.parse_args()
    self_test()
    if args.self_test:
        return
    if args.output is None:
        ap.error('--output is required')
    root = d.ROOT / 'build/recovery-sre-read-boundaries'
    original = bodies(d.b.BLOB)
    report = []
    for family, name in [('V17r_int', 'DemodDataV17'), ('V27r_int', 'DemodDataV27'),
                         ('V29r_int', 'DemodDataV29')]:
        paths = {cell: root/family/cell/'candidate.o'
                 for cell in ('baseline', 'int-joint', 'int-read-boundary')}
        baseline = paths['baseline']
        names = sorted(d.b.sizes(str(baseline)))
        for cell, path in paths.items():
            assert nontext(path) == nontext(baseline), (family, cell, 'nontext/data/bindings')
            for symbol in names:
                a, ar = d.b.body(str(baseline), symbol)
                b, br = d.b.body(str(path), symbol)
                assert [r for off, r in sorted(ar.items())] == [r for off, r in sorted(br.items())], \
                    (family, cell, symbol, 'relocation target sequence')
                if symbol != name:
                    assert (a, ar) == (b, br), (family, cell, symbol, 'bystander')
            if cell == 'int-read-boundary':
                assert d.b.verdict(*d.b.body(d.b.BLOB, name),
                                   *d.b.body(str(path), name))[0] == 'EXACT'
            rows = bodies(path)[name]
            call = next(row for row in rows if row.get('target') == 'FPM_MRF_filter')
            use = first_return_use(rows, call['address'])
            expected = 'signed-word-extension' if cell == 'baseline' else 'unsigned-word-extension'
            assert not use['refusals'] and {v['kind'] for v in use['outcomes']} == {expected}
            report.append({'family': family, 'cell': cell, 'return_use': use,
                           'nontext_data_bindings_equal': True,
                           'shared_functions': len(set(names) & set(d.b.sizes(d.b.BLOB)))})
        rows = original[name]
        call = next(row for row in rows if row.get('target') == 'FPM_MRF_filter')
        use = first_return_use(rows, call['address'])
        assert not use['refusals'] and {v['kind'] for v in use['outcomes']} == {'unsigned-word-extension'}
        report.append({'family': family, 'cell': 'original', 'return_use': use})
    args.output.write_text(json.dumps({'controls': 9, 'audit': report}, indent=2) + '\n')
    print('SRE audit: 9 full-TU cells, 3 original return traces, metadata/data/target '
          'sequences/bystanders unchanged; 3 source winners EXACT')


if __name__ == '__main__':
    main()
