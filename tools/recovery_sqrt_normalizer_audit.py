#!/usr/bin/env python3
"""Verify the complete sqrt TU and the discriminating normalization controls."""
import argparse
import hashlib
import json
from pathlib import Path
import playbook_small_patterns as d
from elftools.elf.elffile import ELFFile

TARGETS = {'FPM_sqrt', 'FPM_sqrt_dp'}

def inventory(path):
    with path.open('rb') as stream:
        elf = ELFFile(stream)
        symbols = list(elf.get_section_by_name('.symtab').iter_symbols())
        bodies = {s.name: elf.get_section(s['st_shndx']).data()[s['st_value']:s['st_value']+s['st_size']]
                  for s in symbols if s['st_info']['type'] == 'STT_FUNC' and s['st_size']}
        exports = sorted((s.name, s['st_info']['bind'], s['st_info']['type'], s['st_other']['visibility'],
                          0 if s.name in TARGETS else s['st_size'])
                         for s in symbols if s['st_info']['bind'] != 'STB_LOCAL')
        data = {s.name: s.data().hex() for s in elf.iter_sections()
                if s.name not in ('', '.text', '.rel.text', '.symtab', '.strtab', '.shstrtab')}
        relocs = []
        for section in elf.iter_sections():
            if section['sh_type'] != 'SHT_REL': continue
            syms = elf.get_section(section['sh_link'])
            for r in section.iter_relocations():
                owner = next(s.name for s in symbols if s['st_info']['type']=='STT_FUNC' and
                             s['st_shndx']==section['sh_info'] and s['st_value']<=r['r_offset']<s['st_value']+s['st_size'])
                target=syms.get_symbol(r['r_info_sym'])
                target_name=target.name
                if target['st_info']['type']=='STT_SECTION':
                    target_name=elf.get_section(target['st_shndx']).name
                    assert target_name in ('.rodata', '.rodata.cst4', '.rodata.cst8')
                assert target_name
                raw=elf.get_section(section['sh_info']).data()
                addend=int.from_bytes(raw[r['r_offset']:r['r_offset']+4], 'little', signed=True)
                relocs.append((owner,target_name,r['r_info_type'],addend))
        return bodies,exports,data,sorted(relocs)

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--run-dir',type=Path,default=Path('build/recovery-sqrt-normalizer'))
    ap.add_argument('--production-object',type=Path)
    a=ap.parse_args()
    cells=json.loads((a.run_dir/'results.json').read_text())['families']['fpm_sqrt']['cells']
    name='pointed-quotient-output-order'
    assert cells['baseline']['baseline_reproduced']
    for label,cell in cells.items():
        assert not cell.get('losses')
        if label!=name: assert not cell.get('gains'), label
    assert set(cells[name]['gains'])==TARGETS
    assert all(cells[name]['verdicts'][s][0]=='EXACT' for s in TARGETS)
    family=a.run_dir/'fpm_sqrt'
    base=inventory(family/'baseline/candidate.o')
    candidate=family/name/'candidate.o'
    new=inventory(candidate)
    assert base[1:3]==new[1:3], 'data or export drift'
    assert set(base[3])-set(new[3]) == {('FPM_sqrt','FPM_sqrt_table',1,-128)}
    assert set(new[3])-set(base[3]) == {('FPM_sqrt','FPM_sqrt_table',1,0)}
    # Explicit HI index subtraction replaces the old relocation-folded -128.
    # The reference-aware byte verdict above checks the recovered destination.
    assert base[0].keys()==new[0].keys()
    changed={s for s in base[0] if base[0][s]!=new[0][s]}
    assert changed==TARGETS
    assert [len(new[0][s]) for s in sorted(TARGETS)]==[133,143]
    if a.production_object: assert a.production_object.read_bytes()==candidate.read_bytes()
    report={'cells':len(cells),'reference_verdicts':len(cells)*2,'emitted_bodies':len(new[0]),
            'changed':sorted(changed),'gains':sorted(TARGETS),'losses':[],
            'relocation_consumers':len(new[3]),'exports_and_data_equal':True,
            'candidate_sha256':hashlib.sha256(candidate.read_bytes()).hexdigest(),
            'negative_controls':'local homes, bit parity, and swapped homes all reject exactness',
            'production_raw_identical':bool(a.production_object)}
    (a.run_dir/'audit.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report))

if __name__=='__main__': main()
