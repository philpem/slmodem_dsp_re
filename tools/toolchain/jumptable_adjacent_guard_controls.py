#!/usr/bin/env python3
"""Controls for a taken JBE whose CMP is immediately adjacent to the guard.

The pre-change prover assumed a taken guard always had a flag-preserving MOV
between the comparison and the branch, so it read the instruction two rows
before the guard as the comparison.  A compiler may instead rebase a switch
selector and compare it directly before the branch (`sub $4,%eax; cmp
$0xb,%eax; jbe dispatch`).  That is RD_create's shape.  This suite measures
both the new acceptance and the old refusal, with adversarial controls for
every way the adjacent form can stop being an unmodified index.
"""
import argparse
import json
import subprocess
import sys
from pathlib import Path

import byteident as b
import jumptable as p

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'build/jumptable-adjacent-guard-controls'
BASE_COMMIT = '2a9d62fc5'

BASE = '''.text
.globl sample
.type sample,@function
sample:
sub $16,%esp
mov %ebx,8(%esp)
mov %esi,12(%esp)
mov 20(%esp),%eax
sub $4,%eax
cmp $3,%eax
jbe .Ldispatch
.Ldefault:
mov $9,%ebx
jmp .Ldone
.Ldispatch:
jmp *.Ltable(,%eax,4)
.Lcase0:
mov $1,%ebx
jmp .Ldone
.Lcase1:
mov $2,%ebx
jmp .Ldone
.Lcase2:
mov $3,%ebx
jmp .Ldone
.Lcase3:
mov $4,%ebx
jmp .Ldone
.Ldone:
mov %ebx,%eax
mov 12(%esp),%esi
mov 8(%esp),%ebx
add $16,%esp
ret
.size sample,.-sample
.section .rodata,"a",@progbits
.p2align 2
.Ltable:
.long .Lcase0,.Lcase1,.Lcase2,.Lcase3
'''

ENTRIES = '.long .Lcase0,.Lcase1,.Lcase2,.Lcase3'


def with_register(source, register):
    return (source.replace('mov 20(%esp),%eax', 'mov 20(%esp),%' + register)
                  .replace('sub $4,%eax', 'sub $4,%' + register)
                  .replace('cmp $3,%eax', 'cmp $3,%' + register)
                  .replace('(,%eax,4)', '(,%' + register + ',4)'))


def legacy_prove():
    """Write the pre-change prover from the branch point beside the fixtures.

    A real pre-change artifact is stronger than a predicate reproducing it:
    the control can then say the OLD program refused the new positive, not
    that a replica written today would have.  It refuses loudly if the base
    commit is unavailable, so a clean run always carries the measurement.
    """
    source = subprocess.run(['git', 'show', BASE_COMMIT + ':tools/toolchain/jumptable.py'],
                            cwd=str(ROOT), check=True, capture_output=True,
                            text=True).stdout
    assert "cmp_row, corridor = rows[guard_index - 2:guard_index]" in source, \
        'base prover no longer carries the pre-change guard selection'
    path = OUT / 'jumptable_prechange.py'
    path.write_text(source)
    return path


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--candidate', type=Path,
                    default=ROOT / 'build/tc_out/src_service_rd.c.o',
                    help='period object holding RD_create, read only')
    args = ap.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)

    positive = {
        'baseline-rebased-index': BASE,
        'no-index-rebase': BASE.replace('sub $4,%eax\n', ''),
        'register-ecx': with_register(BASE, 'ecx'),
        'register-edx': with_register(BASE, 'edx'),
        'table-rebased': BASE.replace('.Ltable:\n', '.space 32\n.Ltable:\n'),
        'swapped-targets': BASE.replace(ENTRIES,
                                        '.long .Lcase1,.Lcase0,.Lcase2,.Lcase3'),
        'one-entry': BASE.replace('cmp $3,%eax', 'cmp $0,%eax')
                          .replace(ENTRIES, '.long .Lcase0'),
        'maximum-bound': (BASE.replace('cmp $3,%eax', 'cmp $0xff,%eax')
                              .replace(ENTRIES, '.long ' + ','.join(
                                  ['.Lcase0'] * 256))),
        'with-corridor': BASE.replace('jbe .Ldispatch',
                                      'mov %esi,0x100(%ebx)\njbe .Ldispatch'),
    }
    negative = {
        'signed-guard': BASE.replace('jbe .Ldispatch', 'jle .Ldispatch'),
        'opposite-guard': BASE.replace('jbe .Ldispatch', 'ja .Ldispatch'),
        'wrong-extent': BASE.replace('cmp $3,%eax', 'cmp $4,%eax'),
        'truncated-table': BASE.replace(ENTRIES,
                                        '.long .Lcase0,.Lcase1,.Lcase2,0'),
        'ambiguous-guard': BASE.replace('.Ldefault:\n', '.Ldefault:\njbe .Ldispatch\n'),
        'clobbered-index-corridor': BASE.replace('jbe .Ldispatch',
                                                 'inc %eax\njbe .Ldispatch'),
        'guard-target-not-dispatch': BASE.replace('jbe .Ldispatch', 'jbe .Lreload')
                                          .replace('.Ldispatch:\n',
                                                   '.Ldispatch:\n.Lreload:\nsub $0,%eax\n'),
        'guard-after-dispatch': BASE.replace('cmp $3,%eax\njbe .Ldispatch\n', '')
                                    .replace('.Lcase3:\n',
                                             '.Lcase3:\n.Llate:\ncmp $3,%eax\njbe .Ldispatch\n'),
        'non-register-index': BASE.replace('cmp $3,%eax', 'cmp $3,(%eax)'),
        'second-indirect': BASE.replace('.Lcase0:\n', '.Lcase0:\njmp *%ebx\n'),
        'wrong-dispatch-register': BASE.replace('(,%eax,4)', '(,%ecx,4)'),
        'wrong-scale': BASE.replace('(,%eax,4)', '(,%eax,2)'),
        'named-guard-entry': BASE.replace('jbe .Ldispatch',
                                          '.globl bypass\nbypass:\njbe .Ldispatch'),
        'table-guard-target': BASE.replace('jbe .Ldispatch', '.Lguard:\njbe .Ldispatch')
                                  .replace(ENTRIES, '.long .Lguard,.Lcase1,.Lcase2,.Lcase3'),
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

    baseline = b.body(str(objects['baseline-rebased-index']), 'sample')
    for label, expected in [('table-rebased', 'EXACT'),
                            ('swapped-targets', 'RELOC')]:
        verdict = b.verdict(*baseline, *b.body(str(objects[label]), 'sample'))[0]
        assert verdict == expected, (label, verdict)
        records[label]['baseline_grade'] = verdict

    # The acceptance must be a measured change: the prover committed before
    # this change refuses every new positive with the guard message.
    legacy = legacy_prove()
    prechange = {}
    for label in ('baseline-rebased-index', 'no-index-rebase',
                  'register-ecx', 'with-corridor'):
        result = subprocess.run([sys.executable, str(legacy),
                                 str(objects[label]), 'sample'],
                                capture_output=True, text=True)
        prechange[label] = {'returncode': result.returncode,
                            'stderr': result.stderr.strip()}
        if label in ('baseline-rebased-index', 'no-index-rebase'):
            assert result.returncode == 1, (label, result.stdout, result.stderr)
            assert result.stderr.strip() == \
                'UNPROVED: unsigned immediate/register guard', (label, result.stderr)
        if label == 'with-corridor':
            assert result.returncode == 0, (label, result.stderr)

    # The real function. The guard form is now accepted; record exactly where
    # the proof continues to refuse rather than claim a grade it does not have.
    real = {}
    for label, path in [('blob', ROOT / 'ref/slmodemd/dsplibs.o'),
                        ('candidate', args.candidate)]:
        pre = subprocess.run([sys.executable, str(legacy), str(path), 'RD_create'],
                             capture_output=True, text=True)
        try:
            proof = p.prove(str(path), 'RD_create')
            post = {'accepted': True, 'reason': None}
        except p.Refused as error:
            proof = None
            post = {'accepted': False, 'reason': str(error)}
        real[label] = {'artifact': str(path),
                       'prechange_returncode': pre.returncode,
                       'prechange_refusal': pre.stderr.strip(),
                       'postchange': post, 'proof': proof}
        assert pre.returncode == 1 and \
            pre.stderr.strip() == 'UNPROVED: unsigned immediate/register guard'
        assert not post['accepted']
    assert real['blob']['postchange']['reason'] == \
        real['candidate']['postchange']['reason']

    (OUT / 'results.json').write_text(json.dumps({
        'assembler': subprocess.check_output(['as', '--version'], text=True),
        'base_commit': BASE_COMMIT,
        'positive': len(positive), 'negative': len(negative),
        'records': records, 'prechange': prechange, 'real': real,
        'rd_create_residual': real['blob']['postchange']['reason'],
    }, indent=2) + '\n')
    print(f'{len(positive)} positive adjacent-guard controls; '
          f'{len(negative)} adversarial refusals; pre-change prover refuses '
          'both new positives with "unsigned immediate/register guard"; '
          'rebased EXACT, swapped targets RELOC; '
          'RD_create guard accepted, residual "'
          + real['blob']['postchange']['reason'] + '" on blob and candidate')


if __name__ == '__main__':
    main()
