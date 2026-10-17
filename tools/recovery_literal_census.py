#!/usr/bin/env python3
"""Triage missing literal consumers, including bodies with equal debug-call counts.

Only .rodata.str relocations are inventoried. Same-TU direct call/tail closure
can explain moved consumers, but cannot establish guards or argument timing.
"""
import argparse
from collections import defaultdict
import hashlib
import json
from pathlib import Path
import tempfile
import recovery_large_debug_audit as calls
from elftools.elf.elffile import ELFFile

ROOT = Path(__file__).resolve().parents[1]


def inventory(path):
    result = defaultdict(set)
    locations = []
    with path.open('rb') as stream:
        elf = ELFFile(stream)
        table = elf.get_section_by_name('.symtab')
        owners = defaultdict(list)
        for symbol in table.iter_symbols():
            if symbol['st_info']['type'] == 'STT_FUNC' and isinstance(symbol['st_shndx'], int):
                owners[symbol['st_shndx']].append(symbol)
                result[symbol.name]
        for section in elf.iter_sections():
            if section['sh_type'] != 'SHT_REL' or section['sh_info'] not in owners:
                continue
            code = elf.get_section(section['sh_info']).data()
            symbols = elf.get_section(section['sh_link'])
            for reloc in section.iter_relocations():
                if reloc['r_info_type'] != 1:  # R_386_32 absolute literal address
                    continue
                target = symbols.get_symbol(reloc['r_info_sym'])
                if not isinstance(target['st_shndx'], int):
                    continue
                data = elf.get_section(target['st_shndx'])
                if not data.name.startswith('.rodata.str'):
                    continue
                off = reloc['r_offset']
                start = target['st_value'] + int.from_bytes(code[off:off+4], 'little', signed=True)
                raw = data.data()
                assert 0 <= start < len(raw), (path, off, start)
                end = raw.index(b'\0', start)
                literal = raw[start:end].decode('ascii')
                for owner in owners[section['sh_info']]:
                    if owner['st_value'] <= off < owner['st_value'] + owner['st_size']:
                        result[owner.name].add(literal)
                        locations.append((owner.name, literal, data['sh_offset'] + start))
    return dict(result), locations


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--objects', type=Path, default=ROOT/'build/tc_repro')
    ap.add_argument('--reference', type=Path, default=ROOT/'ref/slmodemd/dsplibs.o')
    ap.add_argument('--json-out', type=Path, required=True)
    args = ap.parse_args()
    reference, _ = inventory(args.reference)
    objects = sorted(args.objects.glob('*.o'))
    assert objects, 'empty object denominator'
    rows, shared, with_literals, explained, controls = [], 0, 0, 0, 0
    literal_consumers = 0
    for path in objects:
        candidate, locations = inventory(path)
        edges = None
        for name in sorted(candidate.keys() & reference.keys()):
            shared += 1
            if not reference[name]:
                continue
            with_literals += 1
            literal_consumers += len(reference[name])
            missing = reference[name] - candidate[name]
            if not missing:
                continue
            if edges is None:
                edges = calls.graph(path, True)
            reached = calls.closure(edges, name)
            expanded = set().union(*(candidate.get(n, set()) for n in reached))
            remaining = reference[name] - expanded
            if not remaining:
                explained += 1
                continue
            rows.append({'object': path.name, 'symbol': name,
                         'direct_missing': sorted(missing), 'closure_missing': sorted(remaining),
                         'same_TU_bodies': len(reached)})
        # Real ELF negative: a same-length string mutation keeps relocation
        # and call counts fixed while changing the consumed literal inventory.
        if not controls and locations:
            name, literal, offset = next((r for r in locations if r[1]), (None, None, None))
            if name is not None:
                raw = bytearray(path.read_bytes())
                raw[offset] ^= 0x20
                with tempfile.TemporaryDirectory() as temp:
                    wrong = Path(temp)/'wrong.o'
                    wrong.write_bytes(raw)
                    mutated, _ = inventory(wrong)
                assert candidate[name] != mutated[name]
                assert len(raw) == path.stat().st_size
                controls += 1
    assert controls == 1, 'negative control did not execute'
    report = {'reference_sha256': hashlib.sha256(args.reference.read_bytes()).hexdigest(),
              'objects': len(objects), 'shared_bodies': shared, 'bodies_with_original_literals': with_literals,
              'original_unique_consumers': literal_consumers, 'explained_by_same_TU_closure': explained,
              'nominations': len(rows), 'real_ELF_same_length_negative_controls': controls, 'rows': rows,
              'limits': 'Literal inventory only; no guard, argument, multiplicity or reachability proof. '
                        'Closure includes only direct same-TU calls/tails. Missing literals nominate review, not source edits.'}
    args.json_out.write_text(json.dumps(report, indent=2)+'\n')
    print('%d objects; %d shared bodies; %d with original literals; %d unique consumers; '
          '%d closure explanations; %d nominations; %d real ELF negative control' %
          (len(objects), shared, with_literals, literal_consumers, explained, len(rows), controls))
    for row in rows:
        print(row['object'], row['symbol'], repr(row['closure_missing']))


if __name__ == '__main__':
    main()
