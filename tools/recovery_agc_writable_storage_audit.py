#!/usr/bin/env python3
"""Audit coefficient permissions, bytes, pointer consumers and full table TUs."""
import argparse
import hashlib
import json
from pathlib import Path
import playbook_small_patterns as driver
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection

NAMES = {'AGC_DEF_ALPHA', 'AGC_DEF_BETA'}


def objects(path):
    with path.open('rb') as stream:
        elf = ELFFile(stream)
        tab = elf.get_section_by_name('.symtab')
        relocs = {}
        for section in elf.iter_sections():
            if not isinstance(section, RelocationSection):
                continue
            data = elf.get_section(section['sh_info']).data()
            for rel in section.iter_relocations():
                sym = tab.get_symbol(rel['r_info_sym'])
                target = sym.name or elf.get_section(sym['st_shndx']).name
                kind = {1: 'R_386_32', 2: 'R_386_PC32'}[rel['r_info_type']]
                at = rel['r_offset']
                identity = driver.b.relocation_target(kind, target, data[at:at+4],
                                                      driver.b.section_symbols(str(path)))
                relocs[section['sh_info'], at] = (kind, identity)
        result, owner = {}, ''
        for sym in tab.iter_symbols():
            if sym['st_info']['type'] == 'STT_FILE':
                owner = sym.name
            if sym['st_info']['type'] != 'STT_OBJECT' or not isinstance(sym['st_shndx'], int):
                continue
            sec = elf.get_section(sym['st_shndx'])
            lo, size = sym['st_value'], sym['st_size']
            data = bytearray(sec.data()[lo:lo+size])
            refs = {}
            for (ndx, at), target in relocs.items():
                if ndx == sym['st_shndx'] and lo <= at < lo + size:
                    assert at + 4 <= lo + size
                    refs[at-lo] = target
                    data[at-lo:at-lo+4] = b'\0' * 4
            key = (owner if sym['st_info']['bind'] == 'STB_LOCAL' else '', sym.name)
            assert key not in result, key
            result[key] = {'data': data.hex(), 'relocations': refs,
                           'bind': sym['st_info']['bind'], 'visibility': sym['st_other']['visibility'],
                           'size': size, 'section': sec.name, 'flags': sec['sh_flags']}
        return result


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--run-dir', type=Path, default=driver.ROOT / 'build/recovery-agc-writable-storage')
    root = ap.parse_args().run_dir
    families = json.loads((root / 'results.json').read_text())['families']
    original = objects(Path(driver.b.BLOB))
    report, total_objects, total_refs = {}, 0, 0
    for family, description in families.items():
        folder = root / family
        cells = description['cells']
        paths = {label: folder / label / 'candidate.o' for label in cells}
        inventories = {label: objects(path) for label, path in paths.items()}
        base, candidate = inventories['baseline'], inventories['writable-original']
        assert base.keys() == candidate.keys()
        moved = []
        for key, old in base.items():
            new = candidate[key]
            for field in ('data', 'relocations', 'bind', 'visibility', 'size'):
                assert old[field] == new[field], (family, key, field)
            if key[1] in NAMES:
                assert old['section'] == '.rodata' and not old['flags'] & 1
                assert new['section'] == '.data' and new['flags'] & 1
                reference = original[key]
                for field in ('data', 'size', 'bind', 'visibility', 'section', 'flags'):
                    assert reference[field] == new[field], (key, field)
                moved.append(key[1])
            else:
                assert old['section'] == new['section'] and old['flags'] == new['flags']
        assert set(moved) == NAMES
        assert cells['baseline']['baseline_reproduced']
        assert cells['baseline']['globals'] == cells['writable-original']['globals']
        assert cells['baseline']['functions'] == cells['writable-original']['functions'] == []
        for label, path in paths.items():
            assert hashlib.sha256(path.read_bytes()).hexdigest() == cells[label]['object_hash']
        refs = sum(len(row['relocations']) for row in candidate.values())
        total_objects += len(candidate)
        total_refs += refs
        report[family] = {'objects': len(candidate), 'canonical_pointer_relocations': refs,
                          'moved_to_original_writable_storage': moved,
                          'original_FILE_provenance': family + '.c', 'functions': 0,
                          'object_hash': cells['writable-original']['object_hash']}
    assert len(report) == 2
    output = {'full_TU_cells': 4, 'functions': 0, 'named_objects_per_pair': total_objects,
              'canonical_pointer_relocations_per_pair': total_refs, 'families': report,
              'controls': 'two raw-identical baselines fail original writable-permission witness; both candidates pass',
              'limits': 'restores observed storage permissions, not proof of a unique original declaration or runtime write'}
    (root / 'audit.json').write_text(json.dumps(output, indent=2) + '\n')
    print('4 full data-only TUs: %d objects and %d pointer relocations unchanged per pair' % (total_objects, total_refs))
    print('4/4 original coefficient objects: bytes/sizes/local bindings and writable storage match; readonly controls rejected')


if __name__ == '__main__':
    main()
