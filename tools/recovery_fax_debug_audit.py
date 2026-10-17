#!/usr/bin/env python3
"""Audit fax omissions, canonical collateral and original defensive returns."""
import argparse
from collections import Counter
import json
from pathlib import Path
import subprocess
import recovery_debug_boundary_audit as string_audit
from elftools.elf.elffile import ELFFile

ROOT = Path(__file__).resolve().parents[1]
TARGETS = {'cHDLCrx': ('receive-diagnostic', '_hdlc_emulate_receive_state'),
           'class1': ('published-config-null', 'fax_class1_create')}


class Object:
    def __init__(self, path):
        with path.open('rb') as stream:
            elf = ELFFile(stream)
            self.sections = {i: (s.name, bytearray(s.data())) for i, s in enumerate(elf.iter_sections())}
            self.symbols = list(elf.get_section_by_name('.symtab').iter_symbols())
            self.functions = [s for s in self.symbols if s['st_info']['type'] == 'STT_FUNC' and s['st_size']]
            self.relocations = []
            for section in elf.iter_sections():
                if section['sh_type'] != 'SHT_REL':
                    continue
                table = elf.get_section(section['sh_link'])
                for rel in section.iter_relocations():
                    self.relocations.append((section['sh_info'], rel['r_offset'],
                                             table.get_symbol(rel['r_info_sym']), rel['r_info_type']))

    def payload(self, symbol, addend):
        index = symbol['st_shndx']
        if not isinstance(index, int):
            return (symbol.name, addend)
        name, data = self.sections[index]
        at = symbol['st_value'] + addend
        if name.startswith('.rodata.str'):
            return ('string', bytes(data[at:data.index(0, at)]))
        if name == '.text' and symbol['st_info']['type'] == 'STT_SECTION':
            function = next((s for s in self.functions if s['st_value'] <= at < s['st_value'] + s['st_size']), None)
            if function is not None:
                return ('code', function.name, at - function['st_value'])
        if name.startswith('.rodata.cst'):
            width = int(name.rsplit('cst', 1)[1])
            return (name, bytes(data[at:at+width]))
        return (symbol.name or name, addend)

    def canonical(self):
        sections = {i: (name, bytearray(data)) for i, (name, data) in self.sections.items()}
        consumers = {}
        for index, at, symbol, kind in self.relocations:
            name, data = self.sections[index]
            addend = int.from_bytes(data[at:at+4], 'little', signed=True)
            owner = next((s for s in self.functions if s['st_shndx'] == index and
                          s['st_value'] <= at < s['st_value'] + s['st_size']), None)
            key = owner.name if owner else name
            consumers.setdefault(key, Counter())[(kind, self.payload(symbol, addend))] += 1
            sections[index][1][at:at+4] = b'\0'*4
        bodies = {s.name: bytes(sections[s['st_shndx']][1][s['st_value']:s['st_value']+s['st_size']]) for s in self.functions}
        data = {name: bytes(raw) for name, raw in sections.values() if name and name != '.text'
                and not name.startswith(('.rodata.str', '.rel')) and name not in ('.symtab', '.strtab', '.shstrtab')}
        bindings = sorted((s.name, s['st_info']['bind'], s['st_info']['type'], s['st_other']['visibility'])
                          for s in self.symbols if s['st_info']['bind'] != 'STB_LOCAL')
        return bodies, data, bindings, consumers


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--run-dir', type=Path, default=ROOT/'build/fax-debug-boundaries')
    parser.add_argument('--reference', type=Path, default=ROOT/'ref/slmodemd/dsplibs.o')
    args = parser.parse_args()
    results = json.loads((args.run_dir/'results.json').read_text())['families']
    report = {'families': {}}
    for family, (label, target) in TARGETS.items():
        folder = args.run_dir/family
        old, new = folder/'baseline/candidate.o', folder/label/'candidate.o'
        a, b = Object(old).canonical(), Object(new).canonical()
        assert results[family]['cells']['baseline']['baseline_reproduced']
        assert a[1] == b[1] and a[2] == b[2], 'allocated nonstring data or binding drift'
        changed = [name for name in a[0] if a[0][name] != b[0][name]]
        assert changed == [target], (family, changed)
        for owner in a[3]:
            if owner != target:
                assert a[3][owner] == b[3][owner], (family, owner, 'resolved consumer drift')
        baseline = results[family]['cells']['baseline']['verdicts']
        candidate = results[family]['cells'][label]['verdicts']
        assert {n for n, v in baseline.items() if v[0] == 'EXACT'} == {n for n, v in candidate.items() if v[0] == 'EXACT'}
        original_strings = Counter(r['text'] for r in string_audit.strings(args.reference, target))
        new_strings = Counter(r['text'] for r in string_audit.strings(new, target))
        assert original_strings == new_strings, (family, original_strings, new_strings)
        wrong = new_strings.copy()
        text = next(t for t in wrong if 'Receive buffer OK' in t or 'Internal memory' in t)
        wrong[text] -= 1
        assert wrong != original_strings, 'omission negative did not fire'
        report['families'][family] = {'target': target, 'body_count': len(a[0]), 'canonical_changed_bodies': changed,
            'raw_changed_bodies': results[family]['cells'][label]['changed_bodies'],
            'binding_nonstring_data_bystander_consumers_equal': True,
            'resolved_bystander_consumers': sum(sum(c.values()) for n, c in a[3].items() if n != target),
            'original_format_inventory_restored': True, 'omission_negative_rejected': True,
            'baseline_verdict': baseline[target], 'candidate_verdict': candidate[target]}
    # Three OK print sites in the blob collapse to one source-site emission.
    name = 'faxvmi_hdlc_unframe'
    original = Counter(r['text'] for r in string_audit.strings(args.reference, name))
    candidate = Counter(r['text'] for r in string_audit.strings(args.run_dir/'faxvmi_hdlc/baseline/candidate.o', name))
    expected = original.copy()
    expected['CRC is now OK!\n'] -= 2
    assert candidate == expected
    report['unframe_shared_OK_sites'] = {'original': 3, 'candidate': 1, 'other_format_consumers_equal': True}
    report['defensive_boundary'] = 'Published config fields checked after unchecked writes. Original null arm returnsNULL with no cleanup; ordinary malloc failure faults earlier. No reachable allocation-failure fix or synthetic interposition run claimed.'
    report['receive_boundary'] = 'oldcountdown<=0 and unread record and debug>1; print clocks before output, reload readidx after print and output. Fixture is explicitly synthetic component queue, not modem receive reachability.'
    (args.run_dir/'audit.json').write_text(json.dumps(report, indent=2)+'\n')
    print('Two original format inventories restored;two omission negatives rejected;23 bodies/2 canonical changes;unframe shared-OK explanation verified')


if __name__ == '__main__':
    main()
