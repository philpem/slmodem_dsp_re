#!/usr/bin/env python3
"""Calibrate datareloc on real ELF32 controls; scratch directory is explicit."""
import argparse
from pathlib import Path
import subprocess
import sys

from datareloc import Model, compare

SOURCE = '''.file "owner.c"
.section .rodata
.local coeff
.type coeff,@object
.size coeff,4
coeff: .long 7
.globl other
.type other,@object
.size other,4
other: .long 7
.globl third
.type third,@object
.size third,4
third: .long 7
.section .rodata.str1.1,"aMS",@progbits,1
.Ltext: .asciz "one"
.section .data
.globl table
.type table,@object
.size table,16
table: .long coeff
.long other
.long .Ltext
.long 0x12345678
'''


def run(directory):
    directory.mkdir(parents=True, exist_ok=True)
    print(subprocess.check_output(['as', '--version'], text=True).splitlines()[0])
    objects = 0

    def build(name, source):
        nonlocal objects
        asm, obj = directory / (name + '.s'), directory / (name + '.o')
        asm.write_text(source)
        subprocess.run(['as', '--32', '-o', str(obj), str(asm)], check=True)
        objects += 1
        return Model(obj)

    base = build('control', SOURCE)
    checks = 0

    def check(name, source, status, reason=None):
        nonlocal checks
        candidate = build(name, source)
        report = compare(base, candidate)
        table = next(r for r in report['rows'] if r['key'][0] == 'table')
        assert table['status'] == status, (name, table)
        if reason:
            assert table['reason'] == reason, (name, table)
        checks += 1
        print(name + ': table ' + status + '; ' + str(report['counts']))
        return candidate

    check('unchanged', SOURCE, 'matched')
    check('repacked', SOURCE.replace('.local coeff', '.long 0\n.local coeff')
          .replace('.Ltext:', '.asciz "padding"\n.Ltext:'), 'matched')
    candidate = check('wrong-target', SOURCE.replace('.long other\n', '.long third\n'),
                      'different')
    a = base.objects[('table', 'GLOBAL', None)][0]
    b = candidate.objects[('table', 'GLOBAL', None)][0]
    assert base.payload(a) == candidate.payload(b), 'wrong-target control must defeat raw memcmp'
    checks += 1
    check('wrong-addend', SOURCE.replace('.long other\n', '.long other+1\n'), 'different')
    check('wrong-string', SOURCE.replace('"one"', '"two"'), 'different')
    check('wrong-local-owner', SOURCE.replace('owner.c', 'different.c'), 'different')
    check('unknown-local-owner', SOURCE.replace('.file "owner.c"\n', ''), 'unsupported')
    check('unknown-target', SOURCE.replace('.long other\n', '.long missing\n'), 'unsupported')
    check('unsupported-kind', SOURCE.replace('.long other\n', '.long other-.\n'), 'unsupported')
    check('overlapping-target', SOURCE.replace('coeff: .long 7',
          '.local alias\n.type alias,@object\n.size alias,4\nalias:\ncoeff: .long 7'),
          'unsupported')
    check('ordinary-payload', SOURCE.replace('0x12345678', '0x87654321'), 'different')
    grouped = check('COMDAT-source', SOURCE.replace('.section .data',
        '.section .data.table,"awG",@progbits,table,comdat'), 'unsupported',
        'COMDAT-or-linkonce-source')
    neighbor = next(r for r in compare(base, grouped)['rows'] if r['key'][0] == 'other')
    assert neighbor['status'] == 'matched', neighbor
    checks += 1
    check('linkonce-source', SOURCE.replace('.section .data',
        '.section .gnu.linkonce.d.table,"aw",@progbits'), 'unsupported',
        'COMDAT-or-linkonce-source')
    check('linkonce-target', SOURCE.replace('.globl other',
        '.section .gnu.linkonce.r.other,"a",@progbits\n.globl other').replace(
        '.globl third', '.section .rodata\n.globl third'), 'unsupported',
        'COMDAT-or-linkonce-target')
    check('repeated-FILE-owner', SOURCE + '\n.file "owner.c"\n', 'unsupported',
        'missing-or-repeated-LOCAL-FILE-owner')
    check('boundary-overlap', SOURCE.replace('table: .long coeff',
        '.set table, previous+1\nprevious: .long coeff') + '\n.byte 0\n',
        'unsupported', 'relocation-crosses-object-boundary')
    check('outside-named-target', SOURCE.replace('.long other\n', '.long other+4\n'),
          'unsupported', 'named-target-addend-outside-extent')
    weak_source = SOURCE.replace('.local coeff', '.globl coeff').replace(
        '.globl other', '.weak other')
    weak_a = build('weak-owner-a', weak_source)
    weak_b = build('weak-owner-b', weak_source.replace('owner.c', 'elsewhere.c'))
    weak_table = next(r for r in compare(weak_a, weak_b)['rows'] if r['key'][0] == 'table')
    assert weak_table['status'] == 'matched', weak_table
    assert weak_a.identity(next(s for s in weak_a.symbols if s.name == 'other')) == (
        'OBJECT', 'other', 'WEAK', None)
    checks += 2
    empty = build('empty-denominator', '.file "empty.c"\n.text\n.byte 0\n')
    rejected = subprocess.run([sys.executable, str(Path(__file__).with_name('datareloc.py')),
        str(empty.path), str(empty.path), '--json', str(directory / 'empty-report.json')],
        text=True, capture_output=True)
    assert rejected.returncode != 0 and 'zero reference denominator' in rejected.stderr, rejected
    checks += 1
    print('empty-denominator: rejected by CLI')
    print('datareloc controls:', checks, 'passed, 0 failed;', objects, 'assembled objects')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--scratch', required=True, type=Path)
    run(parser.parse_args().scratch)
