#!/usr/bin/env python3
"""Gentoo-assembled controls for CMP16/JA/MOVZX absolute jump tables."""
import argparse
import json
import subprocess
from pathlib import Path
import byteident as b
import jumptable as j

ROOT = Path(__file__).resolve().parents[2]
IMAGE = 'ghcr.io/philpem/gcc-3.4.2-gentoo2005-docker:latest'


def source(reg='eax', bound=1):
    word = {'eax': 'ax', 'ecx': 'cx', 'edx': 'dx', 'ebx': 'bx',
            'ebp': 'bp', 'esi': 'si', 'edi': 'di'}[reg]
    saved = reg in ('ebx', 'ebp', 'esi', 'edi')
    save = 'push %' + reg + '\n' if saved else ''
    restore = 'pop %' + reg + '\n' if saved else ''
    argument = 8 if saved else 4
    entries = ','.join('.Lfirst' if i % 2 == 0 else '.Lsecond'
                       for i in range(bound + 1))
    return f'''.text
.globl sample
.type sample,@function
sample:
{save}movswl {argument}(%esp),%{reg}
cmp ${bound},%{word}
.Lguard:
ja .Ldefault
.Lextend:
movzwl %{word},%{reg}
.Ldispatch:
jmp *.Ltable(,%{reg},4)
.Lfirst:
mov $11,%eax
{restore}ret
.Lsecond:
mov $22,%eax
{restore}ret
.Ldefault:
xor %eax,%eax
{restore}ret
.size sample,.-sample
.section .rodata,"a",@progbits
.p2align 2
.Ltable:
.long {entries}
.section .note.GNU-stack,"",@progbits
'''


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--candidate', type=Path, required=True,
                    help='canonical Gentoo VPcmV34Main.cpp object, read only')
    ap.add_argument('--out', type=Path, default=ROOT / 'scratch/word-guard-controls')
    args = ap.parse_args()
    out = args.out.resolve()
    relative = out.relative_to(ROOT)
    out.mkdir(parents=True, exist_ok=True)
    base = source()
    positive = {
        'baseline': base,
        'rebased': base.replace('.text\n', '.text\n.space 16,0x90\n', 1)
                       .replace('.Ltable:\n', '.space 32\n.Ltable:\n'),
        'dirty-upper-bits': base.replace('movswl 4(%esp),%eax', 'mov $0xffff0001,%eax'),
        'ecx': source('ecx'), 'edx': source('edx'),
        'ebx': source('ebx'), 'ebp': source('ebp'),
        'esi': source('esi'), 'edi': source('edi'),
        'word-immediate': source('ecx', 128),
        'accumulator-immediate': source('eax', 128),
        'maximum-bound': source('eax', 255),
        'one-entry': source('eax', 0),
        'original-32bit-domain': base.replace('cmp $1,%ax', 'cmp $1,%eax')
                                     .replace('movzwl %ax,%eax\n', ''),
        'changed-slot': base.replace('.long .Lfirst,.Lsecond', '.long .Lsecond,.Lfirst'),
        'duplicate-slot': base.replace('.long .Lfirst,.Lsecond', '.long .Lfirst,.Lfirst'),
    }
    negative = {
        'missing-extension': base.replace('movzwl %ax,%eax\n', ''),
        'signed-extension': base.replace('movzwl %ax,%eax', 'movswl %ax,%eax'),
        'stack-word-guard': base.replace('cmp $1,%ax', 'cmp $1,%sp')
                                .replace('movzwl %ax,%eax', 'movzwl %sp,%eax'),
        'wrong-source': base.replace('movzwl %ax,%eax', 'movzwl %dx,%eax'),
        'wrong-destination': base.replace('movzwl %ax,%eax', 'movzwl %ax,%edx'),
        'wrong-index': base.replace('(,%eax,4)', '(,%ecx,4)'),
        'wrong-scale': base.replace('(,%eax,4)', '(,%eax,2)'),
        'signed-guard': base.replace('ja .Ldefault', 'jg .Ldefault'),
        'opposite-guard': base.replace('ja .Ldefault', 'jbe .Ldefault'),
        'full-width-guard': base.replace('cmp $1,%ax', 'cmp $1,%eax'),
        'byte-guard': base.replace('cmp $1,%ax', 'cmp $1,%al'),
        'bound-over-domain': source('eax', 256),
        'negative-bound': base.replace('cmp $1,%ax', 'cmp $-1,%ax'),
        'double-operand-prefix': base.replace('cmp $1,%ax', '.byte 0x66\ncmp $1,%ax'),
        'extension-prefix': base.replace('movzwl %ax,%eax', '.byte 0x66\nmovzwl %ax,%eax'),
        'flags-clobber': base.replace('.Lguard:\n', 'inc %edx\n.Lguard:\n'),
        'selector-clobber': base.replace('.Lextend:\n', 'inc %eax\n.Lextend:\n'),
        'direct-extension-entry': base.replace('.Lfirst:\n', '.Lfirst:\njmp .Lextend\n'),
        'direct-dispatch-entry': base.replace('.Lfirst:\n', '.Lfirst:\njmp .Ldispatch\n'),
        'direct-guard-entry': base.replace('.Lfirst:\n', '.Lfirst:\njmp .Lguard\n'),
        'named-extension-entry': base.replace('.Lextend:\n', '.globl bypass\nbypass:\n.Lextend:\n'),
        'external-extension-entry': base + '\n.text\nexternal: jmp .Lextend\n',
        'relocated-extension-entry': base + '\n.data\n.long .Lextend\n',
        'relocated-extension-interior': base + '\n.data\n.long .Lextend+1\n',
        'table-extension-entry': base.replace('.long .Lfirst,.Lsecond', '.long .Lextend,.Lsecond'),
        'table-dispatch-entry': base.replace('.long .Lfirst,.Lsecond', '.long .Ldispatch,.Lsecond'),
    }
    for label, text in {**positive, **negative}.items():
        (out / (label + '.s')).write_text(text)
    # Fixture construction uses the period binutils, never the host assembler.
    command = ['docker', 'run', '--rm', '--network=none',
               '-v', str(ROOT) + ':/work', '-w', '/work/' + str(relative),
               IMAGE, 'sh', '-ec',
               'as --version; for file in "$@"; do as --32 -o "${file%.s}.o" "$file"; done',
               'word-guard-controls',
               *[label + '.s' for label in {**positive, **negative}]]
    assembled = subprocess.run(command, check=True, capture_output=True, text=True)
    records = {}
    for label in {**positive, **negative}:
        path = str(out / (label + '.o'))
        try:
            proof = j.prove(path, 'sample')
        except j.Refused as error:
            proof = None
            reason = str(error)
        else:
            reason = None
        assert (proof is not None) == (label in positive), (label, reason)
        body = b.body(path, 'sample')
        grade = b.verdict(*body, *body)[0]
        assert grade == ('EXACT' if proof else 'UNRESOLVED'), (label, grade)
        records[label] = {'proved': proof is not None, 'refusal': reason,
                          'self_grade': grade, 'proof': proof}
    original = b.body(str(out / 'baseline.o'), 'sample')
    for label, expected in [('rebased', 'EXACT'), ('changed-slot', 'RELOC'),
                            ('duplicate-slot', 'RELOC')]:
        verdict = b.verdict(*original, *b.body(str(out / (label + '.o')), 'sample'))[0]
        assert verdict == expected, (label, verdict)
        records[label]['baseline_grade'] = verdict
    name = '_Z19interpretMohTimeouts'
    blob = str(ROOT / 'ref/slmodemd/dsplibs.o')
    native = j.prove(blob, name)
    candidate = j.prove(str(args.candidate), name)
    assert native['count'] == candidate['count'] == 14
    assert native['identity'] != candidate['identity']
    assert b.verdict(*b.body(blob, name), *b.body(str(args.candidate), name)) == ('SIZE', 34)
    result = {'assembler': assembled.stdout, 'command': command,
              'positive': len(positive), 'negative': len(negative),
              'records': records, 'native': native, 'candidate': candidate,
              'real_verdict': ['SIZE', 34]}
    (out / 'results.json').write_text(json.dumps(result, indent=2) + '\n')
    print('%d Gentoo-assembled positives; %d adversarial refusals; rebased EXACT; '
          'changed/duplicate slots RELOC; real two14-slot proofs retain SIZE34'
          % (len(positive), len(negative)))


if __name__ == '__main__':
    main()
