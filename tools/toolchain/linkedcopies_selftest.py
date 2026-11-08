#!/usr/bin/env python3
"""Gentoo-binutils controls for the duplicate-function diagnostic."""
import argparse
import json
from pathlib import Path
import subprocess
import tempfile

import linkedcopies


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--image', default='ghcr.io/philpem/gcc-3.4.2-gentoo2005-docker:latest')
    ap.add_argument('--work-dir', type=Path, required=True)
    args = ap.parse_args()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    passed = []
    with tempfile.TemporaryDirectory(prefix='controls-', dir=args.work_dir) as temporary:
        root = Path(temporary).resolve()
        def tool(*command):
            return subprocess.check_output(['docker', 'run', '--rm', '--platform', 'linux/386',
                                           '-v', str(root) + ':/fixture', '-w', '/fixture',
                                           args.image] + list(command), text=True)
        template = '''.section .gnu.linkonce.t.selected,"ax",@progbits
.weak selected
.type selected,@function
selected:
 mov $VALUE,%eax
 ret
.size selected,.-selected
.text
.type private_name,@function
private_name:
 ret
.size private_name,.-private_name
.data
.type NAME_data,@object
NAME_data:
 .long 7
.size NAME_data,.-NAME_data
.text
.globl NAME_reader
.type NAME_reader,@function
NAME_reader:
 mov NAME_data,%eax
 ret
.size NAME_reader,.-NAME_reader
'''
        for name, value in [('good', 1), ('bad', 2)]:
            (root / (name + '.s')).write_text(template.replace('VALUE', str(value)).replace('NAME', name))
            tool('as', '--32', '-o', name + '.o', name + '.s')
        tool('ld', '-r', '-o', 'ab.link', 'good.o', 'bad.o')
        tool('ld', '-r', '-o', 'ba.link', 'bad.o', 'good.o')
        manifest = root / 'manifest.txt'
        manifest.write_text('good.o good.s\nbad.o bad.s\n')
        ab = linkedcopies.audit(root/'good.o', manifest, root/'ab.link')
        ba = linkedcopies.audit(root/'good.o', manifest, root/'ba.link')
        lookup = lambda report, name: next(r for r in report['duplicates'] if r['symbol'] == name)
        a, b = lookup(ab, 'selected'), lookup(ba, 'selected')
        assert a['conservative_grade'] == b['conservative_grade'] == 'BYTES'
        assert a['linked']['grade'] == 'EXACT'
        assert b['linked']['grade'] == 'BYTES'
        passed += ['positive linked EXACT', 'wrong immediate rejected',
                   'reversing link order changes selected body', 'worst-copy grade unchanged']
        assert a['linked_body_matching_inputs'] == ['good.o']
        assert b['linked_body_matching_inputs'] == ['bad.o']
        passed.append('matching-input sets follow actual selected bytes')
        before = linkedcopies.byteident.body(str(root/'good.o'), 'good_reader')
        after = linkedcopies.byteident.body(str(root/'ba.link'), 'good_reader')
        assert before != after and linkedcopies.exact_body_match(before, after)
        passed.append('changed REL addend matches by canonical destination')
        (root/'alien.s').write_text(template.replace('VALUE', '3').replace('NAME', 'alien'))
        tool('as', '--32', '-o', 'alien.link', 'alien.s')
        alien = lookup(linkedcopies.audit(root/'good.o', manifest, root/'alien.link'),
                       'selected')
        assert alien['linked']['grade'] == 'BYTES'
        assert alien['linked_body_matching_inputs'] == []
        assert alien['input_supplier_proven'] is False
        passed.append('unmatched linked artifact has no matching input or supplier proof')
        private = lookup(ab, 'private_name')
        assert private['scope'] == 'local-name-collision'
        assert private['linked_records'] == 2 and private['linked']['grade'] == 'NODATA'
        passed.append('duplicate local name refused as a selected body')
        assert ab['summary']['duplicate_names'] == 2
        assert ab['summary']['duplicate_definition_records'] == 4
        passed.append('all duplicate names and records counted')
        manifest.write_text('good.o good.s\n')
        try:
            linkedcopies.audit(root/'good.o', manifest, root/'ab.link')
        except ValueError as error:
            assert 'outside manifest' in str(error)
        else:
            raise AssertionError('partial manifest accepted')
        passed.append('partial object manifest refused')
        manifest.write_text('good.o good.s\ngood.o good.s\n')
        try:
            linkedcopies.audit(root/'good.o', manifest, root/'ab.link')
        except ValueError as error:
            assert 'repeated object' in str(error)
        else:
            raise AssertionError('repeated manifest accepted')
        passed.append('repeated manifest object refused')
        result = {'passed': len(passed), 'failed': 0, 'controls': passed,
                  'image': args.image, 'assembler': tool('as', '--version').splitlines()[0],
                  'linker': tool('ld', '--version').splitlines()[0]}
        (args.work_dir/'selftest.json').write_text(json.dumps(result, indent=2)+'\n')
        print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
