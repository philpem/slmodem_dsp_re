#!/usr/bin/env python3
"""Controls for the flag-preserving index-copy corridor before a fallthrough guard.

A compiler may copy the compared selector to another register between the CMP
and the unsigned guard whose fallthrough is the single indirect dispatch:

    cmp $1,%eax
    mov %eax,%esi      ; flag-preserving copy of the compared index
    ja  .Ldefault      ; taken to the out-of-range arm
    jmp *.Ltable(,%eax,4)

The prover accepted a taken guard with an intervening flag-preserving store and
a direct adjacent comparison, but refused this fallthrough shape on the blob
itself.  This suite measures the new acceptance, the old refusal, and every way
the copy can stop being an unmodified index.  It also re-proves the two forms
that already worked (adjacent CMP/JA and the pushed-frame stack-load corridor).
"""
import argparse
import json
import subprocess
import sys
from pathlib import Path

import byteident as b
import jumptable as p

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'build/jumptable-copy-corridor-controls'
BASE_COMMIT = 'cf0a652ec'

BASE = '''.text
.globl sample
.type sample,@function
sample:
sub $8,%esp
mov %esi,(%esp)
mov %ebx,4(%esp)
mov 16(%esp),%eax
cmp $1,%eax
mov %eax,%esi
ja .Ldefault
jmp *.Ltable(,%eax,4)
.Lcase0:
xor %eax,%eax
jmp .Ldone
.Lcase1:
mov $1,%eax
jmp .Ldone
.Ldefault:
mov $9,%eax
.Ldone:
mov 4(%esp),%ebx
mov (%esp),%esi
add $8,%esp
ret
.size sample,.-sample
.section .rodata,"a",@progbits
.p2align 2
.Ltable:
.long .Lcase0,.Lcase1
'''

PUSHED_CORRIDOR = '''.text
.globl sample
.type sample,@function
sample:
push %ebx
mov 8(%esp),%ebx
mov 12(%esp),%eax
cmp $1,%eax
mov 16(%esp),%edx
ja .Ldefault
.Ldispatch:
jmp *.Ltable(,%eax,4)
.Lfirst:
mov %edx,(%ebx)
mov $1,%eax
jmp .Lexit
.Lsecond:
mov %edx,4(%ebx)
mov $2,%eax
jmp .Lexit
.Ldefault:
xor %eax,%eax
.Lexit:
pop %ebx
ret
.size sample,.-sample
.section .rodata
.p2align 2
.Ltable:
.long .Lfirst,.Lsecond
.section .note.GNU-stack,"",@progbits
'''

ENTRIES = '.long .Lcase0,.Lcase1'


def ecx_to_edx(source):
    return (source.replace('mov 16(%esp),%eax', 'mov 16(%esp),%ecx')
                  .replace('cmp $1,%eax', 'cmp $1,%ecx')
                  .replace('mov %eax,%esi', 'mov %ecx,%edx')
                  .replace('(,%eax,4)', '(,%ecx,4)'))


def legacy_prove():
    """Materialise the prover from the branch point so the refusal is measured."""
    source = subprocess.run(['git', 'show', BASE_COMMIT + ':tools/toolchain/jumptable.py'],
                            cwd=str(ROOT), check=True, capture_output=True,
                            text=True).stdout
    assert "require(pushed_abi and index >= 3, 'missing adjacent guard')" in source, \
        'base prover no longer carries the pre-change fallthrough refusal'
    path = OUT / 'jumptable_prechange.py'
    path.write_text(source)
    return path


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--candidate', type=Path,
                    default=ROOT / 'build/tc_out/src_v8_v8.c.o',
                    help='period object holding v8_process, read only')
    args = ap.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)

    positive = {
        'eax-to-esi': BASE,
        'eax-to-edx': BASE.replace('mov %eax,%esi', 'mov %eax,%edx'),
        'ecx-to-edx': ecx_to_edx(BASE),
        'dispatch-on-copy': BASE.replace('(,%eax,4)', '(,%esi,4)'),
        'adjacent-no-copy': BASE.replace('mov %eax,%esi\n', ''),
        'pushed-stack-corridor': PUSHED_CORRIDOR,
        'table-rebased': BASE.replace('.Ltable:\n', '.space 32\n.Ltable:\n'),
        'swapped-targets': BASE.replace(ENTRIES, '.long .Lcase1,.Lcase0'),
    }
    negative = {
        'copy-from-different-register': BASE.replace('mov %eax,%esi', 'mov %ebx,%esi'),
        'copy-clobbers-compared-register': BASE.replace('mov %eax,%esi', 'mov %eax,%eax'),
        'flags-writing-corridor': BASE.replace('mov %eax,%esi', 'inc %esi'),
        'index-writing-corridor': BASE.replace('mov %eax,%esi', 'lea 0x1(%eax),%eax'),
        'dispatch-on-wrong-register': BASE.replace('(,%eax,4)', '(,%edi,4)'),
        'dispatch-wrong-scale': BASE.replace('(,%eax,4)', '(,%eax,2)'),
        'extra-corridor-instruction': BASE.replace('mov %eax,%esi', 'mov %eax,%esi\nnop'),
        'second-corridor-instruction': BASE.replace('mov %eax,%esi',
                                                    'mov %eax,%esi\nmov %esi,%edi'),
        'opposite-guard': BASE.replace('ja .Ldefault', 'jb .Ldefault'),
        'signed-guard': BASE.replace('ja .Ldefault', 'jl .Ldefault'),
        'truncated-extent': BASE.replace('cmp $1,%eax', 'cmp $2,%eax'),
        'second-indirect': BASE.replace('.Lcase0:\n', '.Lcase0:\njmp *%ebx\n'),
        'named-corridor-entry': BASE.replace('mov %eax,%esi',
                                             '.globl bypass\nbypass:\nmov %eax,%esi'),
        'guard-bypass-entry': BASE.replace('.Lcase0:\n', '.Lcase0:\njmp .Lguard\n')
                                  .replace('ja .Ldefault', '.Lguard:\nja .Ldefault'),
    }

    records = {}
    objects = {}
    for label, source in {**positive, **negative}.items():
        asm, obj = OUT / (label + '.s'), OUT / (label + '.o')
        asm.write_text(source)
        command = ['as', '--32', '-o', str(obj), str(asm)]
        subprocess.run(command, check=True, capture_output=True)
        objects[label] = obj
        try:
            proof = p.prove(str(obj), 'sample')
            accepted, reason = True, None
        except p.Refused as error:
            proof, accepted, reason = None, False, str(error)
        assert accepted == (label in positive), (label, reason)
        own = b.body(str(obj), 'sample')
        grade = b.verdict(*own, *own)[0]
        assert grade == ('EXACT' if accepted else 'UNRESOLVED'), (label, grade)
        records[label] = {'accepted': accepted, 'reason': reason,
                          'proof': proof, 'self_grade': grade,
                          'command': command}

    baseline = b.body(str(objects['eax-to-esi']), 'sample')
    for label, expected in [('table-rebased', 'EXACT'),
                            ('swapped-targets', 'RELOC')]:
        verdict = b.verdict(*baseline, *b.body(str(objects[label]), 'sample'))[0]
        assert verdict == expected, (label, verdict)
        records[label]['baseline_grade'] = verdict

    # The acceptance must be a measured change.  The prover at the branch point
    # refuses every copy-corridor positive with the incumbent message and still
    # proves the adjacent and pushed-corridor forms.
    legacy = legacy_prove()
    prechange = {}
    new_positives = ('eax-to-esi', 'eax-to-edx', 'ecx-to-edx', 'dispatch-on-copy')
    old_positives = ('adjacent-no-copy', 'pushed-stack-corridor')
    for label in new_positives + old_positives:
        result = subprocess.run([sys.executable, str(legacy),
                                 str(objects[label]), 'sample'],
                                capture_output=True, text=True)
        prechange[label] = {'returncode': result.returncode,
                            'stderr': result.stderr.strip()}
        if label in new_positives:
            assert result.returncode == 1, (label, result.stdout, result.stderr)
            assert result.stderr.strip() == 'UNPROVED: missing adjacent guard', \
                (label, result.stderr)
        else:
            assert result.returncode == 0, (label, result.stderr)

    # The real function.  Both objects must prove with the same ordered targets;
    # the grade recorded below is measured, not assumed, because the prover is
    # only one of the conditions grade 0/1 applies.
    blob = ROOT / 'ref/slmodemd/dsplibs.o'
    real = {}
    for label, path in (('blob', blob), ('candidate', args.candidate)):
        proof = p.prove(str(path), 'v8_process')
        real[label] = {'artifact': str(path), 'proof': proof,
                       'grade': b.verdict(*b.body(str(path), 'v8_process'),
                                          *b.body(str(path), 'v8_process'))[0]}
    assert real['blob']['proof']['identity'] == real['candidate']['proof']['identity']
    assert real['blob']['proof']['count'] == 18
    grade, differing = b.verdict(*b.body(str(blob), 'v8_process'),
                                 *b.body(str(args.candidate), 'v8_process'))
    real['pair_grade'] = [grade, differing]
    real['pair_alpha'] = b.alpha_why(b.insns(str(blob), 'v8_process'),
                                     b.insns(str(args.candidate), 'v8_process'))

    (OUT / 'results.json').write_text(json.dumps({
        'assembler': subprocess.check_output(['as', '--version'], text=True),
        'base_commit': BASE_COMMIT,
        'positive': len(positive), 'negative': len(negative),
        'records': records, 'prechange': prechange, 'real': real,
    }, indent=2) + '\n')
    print(f'{len(positive)} copy-corridor positives; {len(negative)} adversarial '
          f'refusals; pre-change prover refuses the {len(new_positives)} new '
          'positives with "missing adjacent guard" and still proves the '
          f'{len(old_positives)} existing forms; rebased EXACT, swapped targets '
          f'RELOC; v8_process proves on blob and candidate ({real["blob"]["proof"]["count"]} '
          f'slots, equal identity), pair grade {grade} ({differing} bytes)')

if __name__ == '__main__':
    main()
