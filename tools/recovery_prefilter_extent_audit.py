#!/usr/bin/env python3
"""Check table extent, natural zero padding, and all PreFilter table contents."""
import argparse
import json
from pathlib import Path
import playbook_small_patterns as d
from elftools.elf.elffile import ELFFile
from recovery_agc_writable_storage_audit import objects

TYPE2 = '_ZN12V90PreFilter13refLoopsType2E'
TYPE1 = '_ZN12V90PreFilter13refLoopsType1E'

def gap(path):
    with Path(path).open('rb') as stream:
        elf = ELFFile(stream)
        syms = {s.name: s for s in elf.get_section_by_name('.symtab').iter_symbols()}
        a, b = syms[TYPE2], syms[TYPE1]
        assert a['st_shndx'] == b['st_shndx']
        data = elf.get_section(a['st_shndx']).data()
        lo, hi = a['st_value'] + a['st_size'], b['st_value']
        return a['st_size'], data[lo:hi]

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--run-dir', type=Path, default=d.ROOT/'build/recovery-prefilter-extent')
    root = p.parse_args().run_dir
    cells = json.loads((root/'results.json').read_text())['families']['V90PreFilterRefLoops']['cells']
    paths = {n: root/'V90PreFilterRefLoops'/n/'candidate.o' for n in cells}
    original = objects(Path(d.b.BLOB))
    base, new = objects(paths['baseline']), objects(paths['original-extent'])
    assert base.keys() == new.keys() and len(new) == 7
    refs = 0
    for key, candidate in new.items():
        prior, reference = base[key], original[key]
        for field in ('data', 'size', 'bind', 'visibility', 'section', 'flags', 'relocations'):
            assert candidate[field] == reference[field], (key, field)
            if key[1] != TYPE2:
                assert candidate[field] == prior[field], (key, field)
        refs += len(candidate['relocations'])
    assert gap(paths['baseline'])[0] == 34*68
    assert gap(paths['original-extent']) == gap(d.b.BLOB) == (33*68, bytes(28))
    assert base['', TYPE2]['data'] == new['', TYPE2]['data'] + bytes(68).hex()
    assert cells['baseline']['baseline_reproduced']
    assert all(not c['functions'] for c in cells.values())
    report = {'full_TUs': 2, 'named_objects': 7, 'canonical_relocations': refs,
              'original_rows': 33, 'natural_zero_gap': 28,
              'baseline_wrong_extent_rejected': True,
              'all_named_contents_sizes_bindings_permissions_targets_match_original': True,
              'scope': 'Period compiler natural padding preserves original first-byte terminator read; no padding declaration or function gain.'}
    (root/'audit.json').write_text(json.dumps(report, indent=2)+'\n')
    print('2 full TUs: 7 objects and %d pointer targets match original; 33 rows + 28 natural zero bytes; 34-row control rejected' % refs)

if __name__ == '__main__':
    main()
