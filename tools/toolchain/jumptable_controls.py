#!/usr/bin/env python3
"""Real assembled ELF controls for the separate bounded jump-table proof."""
import json
import re
import subprocess
from pathlib import Path
import jumptable as proof


ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'build/jumptable-controls'


def fixture(label, **options):
    register = options.get('register', 'ecx')
    scale = options.get('scale', 4)
    count = options.get('count', 4)
    guard = options.get('guard', 'ja')
    entries = options.get('entries', ['case0', 'case1', 'case2', 'case3', 'case4'])
    table = '\n'.join('.long ' + entry for entry in entries)
    if options.get('missing'):
        table = table.replace('.long case2', '.long 0')
    if options.get('wrong_kind'):
        table = table.replace('.long case2', '.long case2 - .')
    suffix = ''
    if options.get('duplicate'):
        suffix += '.reloc table+8,R_386_32,case2\n'
    if options.get('overlap'):
        suffix += '.reloc table+9,R_386_32,case2\n'
    if options.get('alias'):
        suffix += '.type alias,@function\n.set alias,dispatch\n.size alias,1\n'
    if options.get('zero_alias'):
        suffix += '.type alias,@function\n.set alias,dispatch\n'
    if options.get('named_overlap'):
        suffix += '.type named,@object\n.set named,table\n.size named,20\n'
    if options.get('duplicate_section'):
        suffix += '.section .extra,"a",@progbits\n.long 0\n'
    bypass = options.get('prefix', 'jmp dispatch\n' if options.get('bypass') else '')
    extra = 'jmp *%eax\n' if options.get('indirect') else ''
    middle = 'inc %ecx\n' if options.get('clobber') else ''
    flags = 'aw' if options.get('writable') else 'a'
    source = f'''.text
.space {options.get('code_padding', 0)},0x90
.globl sample
.type sample,@function
sample:
{bypass}cmp ${count},%ecx
{guard} default
{middle}dispatch:
jmp *table(,%{register},{scale})
case0: mov $1,%eax
ret
case1: mov $2,%eax
ret
case2: mov $3,%eax
ret
case3: mov $4,%eax
ret
case4: mov $5,%eax
ret
{extra}default: ret
end:
.size sample,end-sample
.type other,@function
other: {options.get('other_body', 'ret')}
.size other,.-other
.section .rodata,"{flags}",@progbits
.space {options.get('table_padding', 0)},0
table:
{table}
{suffix}
'''
    source = re.sub(r'\b(dispatch|case[0-4]|default|end|table)\b', r'.L\1', source)
    assembly, obj = OUT / (label + '.s'), OUT / (label + '.o')
    assembly.write_text(source)
    command = ['as', '--32', '-o', str(obj), str(assembly)]
    subprocess.run(command, check=True, capture_output=True)
    if options.get('duplicate_section'):
        subprocess.run(['objcopy', '--rename-section', '.extra=.rodata', str(obj)],
                       check=True, capture_output=True)
    return obj


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    version = subprocess.check_output(['as', '--version'], text=True).splitlines()[0]
    baseline_path = fixture('baseline')
    baseline = proof.prove(baseline_path, 'sample')
    results = {}
    for label, options in {
        'rebased': {'code_padding': 16, 'table_padding': 32},
        'table-later': {'table_padding': 64},
        'duplicate-destinations': {'entries': ['case0', 'case1', 'case1', 'case3', 'case4']},
        'changed-slot': {'entries': ['case0', 'case1', 'case3', 'case3', 'case4']},
        'slot-permutation': {'entries': ['case1', 'case0', 'case2', 'case3', 'case4']},
    }.items():
        record = proof.prove(fixture(label, **options), 'sample')
        same = record['identity'] == baseline['identity']
        assert same == (label in ('rebased', 'table-later'))
        if label == 'duplicate-destinations':
            again = proof.prove(fixture('duplicate-destinations-rebased', code_padding=16,
                                       table_padding=32, **options), 'sample')
            assert again['identity'] == record['identity']
        results[label] = {'proved': True, 'same_identity': same}
    negatives = {
        'guard-count': {'count': 5},
        'wrong-index': {'register': 'edx'},
        'wrong-scale': {'scale': 2},
        'signed-guard': {'guard': 'jg'},
        'missing-guard': {'guard': 'jmp'},
        'guard-bypass': {'bypass': True},
        'index-clobber': {'clobber': True},
        'instruction-interior': {'entries': ['case0+1', 'case1', 'case2', 'case3', 'case4']},
        'owner-end': {'entries': ['end', 'case1', 'case2', 'case3', 'case4']},
        'other-owner': {'entries': ['other', 'case1', 'case2', 'case3', 'case4']},
        'alias-owner': {'alias': True},
        'writable-table': {'writable': True},
        'truncated-table': {'entries': ['case0', 'case1', 'case2', 'case3']},
        'missing-entry-relocation': {'missing': True},
        'duplicate-entry-relocation': {'duplicate': True},
        'overlapping-entry-relocation': {'overlap': True},
        'wrong-entry-relocation': {'wrong_kind': True},
        'misaligned-base': {'table_padding': 1},
        'outside-section': {'entries': ['case0+0x1000', 'case1', 'case2', 'case3', 'case4']},
        'table-bypass': {'entries': ['dispatch', 'case1', 'case2', 'case3', 'case4']},
        'additional-indirect': {'indirect': True},
        'named-table-overlap': {'named_overlap': True},
        'external-direct-bypass': {'other_body': 'jmp dispatch'},
        'external-absolute-bypass': {'other_body': 'mov $dispatch,%eax\nret'},
        'ambiguous-table-section': {'duplicate_section': True},
        'oversized-bound': {'count': 256},
        'owner-loop-bypass': {'prefix': 'loop dispatch\n'},
        'external-loop-bypass': {'other_body': 'loop dispatch\nret'},
        'external-interior-bypass': {'other_body': 'jmp dispatch+1'},
        'zero-sized-alias': {'zero_alias': True},
        'owner-far-jump': {'prefix': 'ljmp *(%eax)\n'},
        'owner-far-call': {'prefix': 'lcall *(%eax)\n'},
        'external-cmp-interior': {'other_body': 'jmp sample+1'},
        'prefixed-indirect-jump': {'prefix': '.byte 0xf2\njmp *%eax\n'},
        'address-size-indirect': {'prefix': '.byte 0x67\njmp *%eax\n'},
        'return-address-store': {'prefix': 'mov %eax,(%esp)\nret\n'},
        'stack-pointer-write': {'prefix': 'mov %eax,%esp\nret\n'},
    }
    for label, options in negatives.items():
        try:
            proof.prove(fixture(label, **options), 'sample')
        except proof.Refused as error:
            results[label] = {'proved': False, 'reason': str(error)}
            print(label + ': REFUSED ' + str(error))
        else:
            raise AssertionError('negative control accepted: ' + label)
    blob = proof.prove(ROOT / 'ref/slmodemd/dsplibs.o', 'MakeTxData')
    candidate = proof.prove(ROOT / 'build/tc_out/src_pump_v22_v22prc.c.o', 'MakeTxData')
    assert blob['identity'] == candidate['identity']
    (OUT / 'results.json').write_text(json.dumps({
        'assembler': version, 'controls': results, 'reference': blob,
        'candidate': candidate, 'strict_comparator_changed': False, 'synthetic_objects': 7 + len(negatives),
        'accepted_objects': 7, 'refused_objects': len(negatives)}, indent=2) + '\n')
    print(f'{7 + len(negatives)} synthetic ELF objects: 7 accepted '
          f'(3 equal pairs, 2 distinct identities), {len(negatives)} refused; '
          '2 real objects/1 equal table identity; strict grade unchanged')


if __name__ == '__main__':
    main()
