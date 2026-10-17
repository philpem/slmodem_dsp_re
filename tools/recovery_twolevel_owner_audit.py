#!/usr/bin/env python3
"""Audit postcall owner recovery, full-TU collateral and resolved relocation payloads."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

_saved = sys.path[:]
sys.path[:] = [p for p in sys.path if Path(p or '.').resolve() != Path(__file__).resolve().parent]
import dis
from elftools.elf.elffile import ELFFile
sys.path[:] = _saved
ROOT = Path(__file__).resolve().parents[1]
TARGET = '_ZN20V90Phase3Demodulator13twoLevelDemodEfRi'


def metadata(path):
    with path.open('rb') as stream:
        elf = ELFFile(stream)
        symbols = elf.get_section_by_name('.symtab')
        functions = [s for s in symbols.iter_symbols() if s['st_info']['type'] == 'STT_FUNC' and s['st_size']]
        exports = [(s.name, s['st_info']['bind'], s['st_info']['type'], s['st_other']['visibility'],
                    0 if s.name == TARGET else s['st_size']) for s in symbols.iter_symbols()
                   if s['st_info']['bind'] != 'STB_LOCAL']
        data = {s.name: hashlib.sha256(s.data()).hexdigest() for s in elf.iter_sections()
                if s.name not in ['', '.text', '.symtab', '.strtab', '.shstrtab', '.rel.text']}
        consumers = Counter()
        for section in elf.iter_sections():
            if section['sh_type'] != 'SHT_REL':
                continue
            names = elf.get_section(section['sh_link'])
            source = elf.get_section(section['sh_info']).data()
            for relocation in section.iter_relocations():
                at = relocation['r_offset']
                owner = next((s.name for s in functions if s['st_shndx'] == section['sh_info'] and
                              s['st_value'] <= at < s['st_value'] + s['st_size']), section.name)
                symbol = names.get_symbol(relocation['r_info_sym'])
                addend = int.from_bytes(source[at:at+4], 'little', signed=True)
                destination = symbol['st_shndx']
                payload = ''
                if isinstance(destination, int):
                    target = elf.get_section(destination)
                    if target.name.startswith(('.rodata', '.data')):
                        offset = symbol['st_value'] + addend
                        payload = target.data()[offset:offset+8].hex()
                consumers[(owner, symbol.name, relocation['r_info_type'], addend, payload)] += 1
        return sorted(exports), data, consumers


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--run-dir', type=Path, default=ROOT/'build/twolevel-owner')
    args = ap.parse_args()
    family = args.run_dir/'V90Phase3Demodulator'
    baseline = family/'baseline/candidate.o'
    candidate = family/'postcall-member-owner/candidate.o'
    a, b = metadata(baseline), metadata(candidate)
    assert a == b, 'export/data/resolved-consumer drift'
    cells = json.loads((args.run_dir/'results.json').read_text())['families']['V90Phase3Demodulator']['cells']
    assert cells['baseline']['baseline_reproduced']
    assert cells['postcall-member-owner']['changed_bodies'] == [TARGET]
    assert not cells['postcall-member-owner']['gains'] and not cells['postcall-member-owner']['losses']
    original = subprocess.check_output(['python3', str(ROOT/'tools/dis.py'), str(ROOT/'ref/slmodemd/dsplibs.o'), TARGET], text=True)
    text = subprocess.check_output(['python3', str(ROOT/'tools/dis.py'), str(candidate), TARGET], text=True)
    (args.run_dir/'original.dis').write_text(original)
    def reloads(s):
        return bool(re.search(r'9dd:.*mov\s+\(%esi\),%eax', s)) and bool(re.search(r'9f5:.*mov\s+\(%esi\),%ecx', s))
    assert reloads(text)
    assert not reloads(text.replace('(%esi),%eax', '(%ebx),%eax')), 'owner negative did not fire'
    assert all(address + ':' in original for address in ['2165d', '21675', '215e3', '215ff', '21613', '2162f'])
    final = args.run_dir/'final-source/candidate.o'
    assert final.read_bytes() == candidate.read_bytes(), 'final comments changed raw candidate'
    report = {'target': TARGET, 'original_bytes': 220, 'baseline_verdict': cells['baseline']['verdicts'][TARGET],
              'candidate_verdict': cells['postcall-member-owner']['verdicts'][TARGET],
              'candidate_sha256': hashlib.sha256(candidate.read_bytes()).hexdigest(),
              'shared_bodies': len(cells['baseline']['functions']), 'changed_bodies': [TARGET],
              'exports_data_and_resolved_consumers_equal': True, 'relocation_consumers': sum(a[2].values()),
              'two_original_owner_reloads_restored': True, 'wrong_owner_negative_rejected': True,
              'boundary': 'isAltRbs is read-only; no functional bug or alias reachability claimed. Valid producer framePosition0..5 and code<128; scalar bit reference is caller-owned storage.',
              'remaining': 'Register allocation differs throughout: original thisEBX/levelESI, candidate thisESI/levelEBX; positive arm level-copy and negative arm temporary differ. Source-use graph restored, not exact source spelling.'}
    (args.run_dir/'audit.json').write_text(json.dumps(report, indent=2)+'\n')
    print('Owner reloads2/2; wrong-owner negative rejected; %d bodies/1 changed; %d resolved consumers' % (report['shared_bodies'], report['relocation_consumers']))


if __name__ == '__main__':
    main()
