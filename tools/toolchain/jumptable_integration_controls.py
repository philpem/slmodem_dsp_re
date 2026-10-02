#!/usr/bin/env python3
"""Exercise bounded table identity through byteident's actual body/verdict."""
import json
from pathlib import Path
import byteident as b
import jumptable_controls as controls


def main():
    controls.main()
    root = controls.OUT
    metadata = json.loads((root / 'results.json').read_text())
    baseline = b.body(str(root / 'baseline.o'), 'sample')
    results = {}
    for label, record in metadata['controls'].items():
        candidate = b.body(str(root / (label + '.o')), 'sample')
        grade = b.verdict(*baseline, *candidate)[0]
        if record['proved']:
            assert grade == ('EXACT' if record['same_identity'] else 'RELOC'), (label, grade)
        else:
            assert grade != 'EXACT', (label, grade)
            assert not any(target[0] == 'jump-table' for _, target in candidate[1].values()), label
        results[label] = grade
        print(label + ': ' + grade)
    duplicate = b.body(str(root / 'duplicate-destinations.o'), 'sample')
    rebased_duplicate = b.body(str(root / 'duplicate-destinations-rebased.o'), 'sample')
    assert b.verdict(*duplicate, *rebased_duplicate)[0] == 'EXACT'
    # The same table address loaded as ordinary data has no bounded dispatch
    # evidence. Even two identical objects must remain UNRESOLVED.
    assembly = root / 'ordinary-consumer.s'
    assembly.write_text('''.text
.globl ordinary
.type ordinary,@function
ordinary: mov $.Ltable,%eax
ret
.size ordinary,.-ordinary
.section .rodata,"a",@progbits
.Ltable: .long 1,2,3,4,5
''')
    import subprocess
    ordinary_path = root / 'ordinary-consumer.o'
    subprocess.run(['as', '--32', '-o', str(ordinary_path), str(assembly)], check=True)
    ordinary = b.body(str(ordinary_path), 'ordinary')
    assert b.verdict(*ordinary, *ordinary)[0] == 'UNRESOLVED'
    reference = b.body(str(controls.ROOT / 'ref/slmodemd/dsplibs.o'), 'MakeTxData')
    candidate = b.body(str(controls.ROOT / 'build/tc_out/src_pump_v22_v22prc.c.o'), 'MakeTxData')
    assert b.verdict(*reference, *candidate)[0] == 'EXACT'
    metadata['integration'] = results
    metadata['ordinary_consumer'] = 'UNRESOLVED'
    metadata['MakeTxData'] = 'EXACT'
    metadata['strict_comparator_changed'] = True
    (root / 'integration-results.json').write_text(json.dumps(metadata, indent=2) + '\n')
    print(f"{metadata['synthetic_objects'] + 1} synthetic ELF objects: "
          '3 equal-pair EXACT checks, 3 distinct-pair RELOC '
          f"checks, {metadata['refused_objects']} refusals never EXACT, "
          '1 ordinary-consumer UNRESOLVED; '
          '2 real objects/MakeTxData EXACT')


if __name__ == '__main__':
    main()
