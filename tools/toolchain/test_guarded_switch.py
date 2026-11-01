#!/usr/bin/env python3
"""Genuine ELF controls for exact-only anonymous switch identity.

No compiler or reference object is needed: GNU as emits ELF32 REL fixtures.
Every rejected geometry is tested on both equal-bodied sides. Optional actual
objects exercise the 12-entry RD_create witness without rebuilding source.
"""
import argparse
import hashlib
import io
import json
import subprocess
import tempfile
from pathlib import Path

from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection

import byteident as b
import fidelity
import guarded_switch as g
import linkedcopies

SOURCE = '''.text
.globl sample
.type sample,@function
sample:
push %ebx
call callee
mov 8(%esp),%eax
.Lsub:
sub $4,%eax
.Lcmp:
cmp $1,%eax
.Lguard:
jbe .Ldispatch
pop %ebx
ret
.Ldispatch:
jmp *.Ltable(,%eax,4)
.Lfirst:
mov $1,%eax
jmp .Lexit
.Lsecond:
mov $2,%eax
.Lexit:
pop %ebx
ret
.size sample,.-sample
.section .rodata,"a",@progbits
PREFIX
.Ltable:
.long .Lfirst,.Lsecond
.section .spare,"",@progbits
.space 16
'''


# Zero-based, signed 16-bit selector widened to the full EAX index before
# the unsigned bound. The default is a separate in-owner instruction block.
FALLTHROUGH_SOURCE = SOURCE.replace('mov 8(%esp),%eax\n.Lsub:\nsub $4,%eax',
                                    'movswl 8(%esp),%eax').replace(
    'jbe .Ldispatch\npop %ebx\nret\n.Ldispatch:',
    'ja .Ldefault\n.Ldispatch:').replace(
    '.Lfirst:', '.Ldefault:\npop %ebx\nret\n.Lfirst:')


def run_fallthrough(directory, actual=None, blob=None):
    records = {}
    left = assemble(directory, 'fall-left', FALLTHROUGH_SOURCE)
    right = assemble(directory, 'fall-right', FALLTHROUGH_SOURCE, 16)
    assert g.prove(left, 'sample')['identity'] == g.prove(right, 'sample')['identity']
    assert b.verdict(*b.body(left, 'sample'), *b.body(right, 'sample')) == ('EXACT', 0)
    near = FALLTHROUGH_SOURCE.replace('ja .Ldefault',
        '.byte 0x0f,0x87\n.long .Ldefault-.-4')
    near_paths = [assemble(directory, 'near-ja-' + str(i), near, 16 * i) for i in range(2)]
    assert g.prove(near_paths[0], 'sample')['identity'] == g.prove(near_paths[1], 'sample')['identity']
    assert b.verdict(*b.body(near_paths[0], 'sample'), *b.body(near_paths[1], 'sample')) == ('EXACT', 0)
    records['near-ja-positive'] = g.prove(near_paths[0], 'sample')
    extension_positives = 0
    for mnemonic, register in (('movswl', '%dx'), ('movzbl', '%dl')):
        for mode, operand in {
            'reg':register, 'base':'(%edx)', 'disp8':'127(%edx)',
            'disp32':'0x12345(%edx)', 'sib':'(%edx,%ecx,2)',
            'absolute':'0x12345', 'sib-base5':'0x12345(,%ecx,2)',
        }.items():
            source = FALLTHROUGH_SOURCE.replace('movswl 8(%esp),%eax', mnemonic + ' ' + operand + ',%eax')
            paths = [assemble(directory, mnemonic + '-' + mode + str(i), source, 16 * i) for i in range(2)]
            assert g.prove(paths[0], 'sample')['identity'] == g.prove(paths[1], 'sample')['identity']
            assert b.verdict(*b.body(paths[0], 'sample'), *b.body(paths[1], 'sample')) == ('EXACT', 0)
            extension_positives += 1
    records['extension-addressing-positive-pairs'] = extension_positives
    # A smaller bound is a valid smaller switch, not invalid geometry. It
    # still cannot match the original machine's bytes or descriptor.
    for label, source in {
        'ordered-targets': FALLTHROUGH_SOURCE.replace('.long .Lfirst,.Lsecond', '.long .Lsecond,.Lfirst'),
        'smaller-bound': FALLTHROUGH_SOURCE.replace('cmp $1,%eax', 'cmp $0,%eax'),
    }.items():
        path = assemble(directory, 'fall-' + label, source, 16)
        if label == 'smaller-bound':
            assert g.prove(path, 'sample')['identity'][4] == 1
        grade = b.verdict(*b.body(left, 'sample'), *b.body(path, 'sample'))[0]
        assert grade != 'EXACT', (label, grade)
        records[label] = grade
    negatives = {
        'signed': FALLTHROUGH_SOURCE.replace('ja .Ldefault', 'jg .Ldefault'),
        'off-by-one': FALLTHROUGH_SOURCE.replace('cmp $1,%eax', 'cmp $2,%eax'),
        'different-comparison': FALLTHROUGH_SOURCE.replace('cmp $1,%eax', 'cmp $1,%edx'),
        'different-index': FALLTHROUGH_SOURCE.replace('(,%eax,4)', '(,%edx,4)'),
        'partial-comparison': FALLTHROUGH_SOURCE.replace('cmp $1,%eax', 'cmp $1,%ax'),
        'default-middle': FALLTHROUGH_SOURCE.replace('ja .Ldefault', 'ja .Lfirst+1'),
        'default-outside': FALLTHROUGH_SOURCE.replace('ja .Ldefault', 'ja outside') + '\n.text\noutside: ret\n',
        'default-dispatch': FALLTHROUGH_SOURCE.replace('ja .Ldefault', 'ja .Ldispatch'),
        'default-cmp': FALLTHROUGH_SOURCE.replace('ja .Ldefault', 'ja .Lcmp'),
        'target-middle': FALLTHROUGH_SOURCE.replace('.long .Lfirst,.Lsecond', '.long .Lfirst+1,.Lsecond'),
        'target-outside': FALLTHROUGH_SOURCE.replace('.long .Lfirst,.Lsecond', '.long sample-1,.Lsecond'),
        'missing-entry': FALLTHROUGH_SOURCE.replace('.long .Lfirst,.Lsecond', '.long .Lfirst,0'),
    }
    for where in ('cmp $1,%eax', 'ja .Ldefault'):
        for name, instruction in {'selector':'mov %edx,%eax', 'flags':'add $1,%edx', 'nop':'nop'}.items():
            negatives[where[:2] + '-' + name] = FALLTHROUGH_SOURCE.replace(where, where + '\n' + instruction)
    for prefix in ('0x66', '0x67', '0x2e', '0xf2', '0xf3'):
        negatives['extension-prefix-' + prefix] = FALLTHROUGH_SOURCE.replace('movswl', '.byte ' + prefix + '\nmovswl')
    for label, prefix in (('segment', '0x2e'), ('address', '0x67'), ('rep', '0xf3'), ('repeat66', '0x66')):
        negatives['word-move-prefix-' + label] = FALLTHROUGH_SOURCE.replace(
            'mov $1,%eax', '.byte 0x66,' + prefix + ',0xc7,0x43,0x02,0x01,0x00')
    negatives['word-move-C7-not-zero'] = FALLTHROUGH_SOURCE.replace(
        'mov $1,%eax', '.byte 0x66,0xc7,0xf8,0x01,0x00')
    for label in ('cmp', 'guard', 'dispatch'):
        size = {'cmp':3, 'guard':2, 'dispatch':7}[label]
        for offset in range(size):
            target = '.L%s+%d' % (label, offset)
            for name, suffix in {
                'direct':'\n.text\nexternal: jmp ' + target,
                'relocated':'\n.data\n.long ' + target,
                'named':'\n.globl alias\n.set alias,' + target,
                'xbegin':'\n.text\nexternal: ' + xbegin(target),
                'xbegin16':'\n.text\nexternal: ' + xbegin(target, word=True),
                'bnd':'\n.text\nexternal: .byte 0xf2\njmp ' + target,
            }.items():
                negatives['%s-%s-%d' % (name, label, offset)] = FALLTHROUGH_SOURCE + suffix + '\n'
    for label, source in negatives.items():
        reasons = []
        for i in range(2):
            path = assemble(directory, 'fall-' + label + str(i), source, 16 * i)
            try:
                g.prove(path, 'sample')
            except g.Refused as error:
                reasons.append(str(error))
            else:
                raise AssertionError('fallthrough geometry accepted: ' + label)
            assert not any(t[0] == g.TAG for _, t in (b.body(path, 'sample')[1] or {}).values()), label
        records[label] = reasons
    if actual:
        proofs = [g.prove(path, 'TxNextStateV29') for path in (blob, actual)]
        assert proofs[0]['identity'] == proofs[1]['identity']
        assert proofs[0]['identity'][-1] == (106, 153, 263, 310, 441, 486, 527)
        assert b.verdict(*b.body(blob, 'TxNextStateV29'), *b.body(actual, 'TxNextStateV29')) == ('EXACT', 0)
        for side, path, proof in zip(('native', 'candidate'), (blob, actual), proofs):
            for mode in ('swap', 'interior'):
                def action(raw, elf):
                    off = elf.get_section_by_name(proof['table_section'])['sh_offset'] + proof['table_offset']
                    if mode == 'swap':
                        raw[off:off+4], raw[off+4:off+8] = raw[off+4:off+8], raw[off:off+4]
                    else:
                        raw[off:off+4] = (int.from_bytes(raw[off:off+4], 'little') + 1).to_bytes(4, 'little')
                changed = mutate(directory, 'v29-' + side + '-' + mode, path, action)
                grade = b.verdict(*b.body(blob, 'TxNextStateV29'), *b.body(changed, 'TxNextStateV29'))[0]
                assert grade != 'EXACT', (side, mode, grade)
                records['actual-' + side + '-' + mode] = grade
        records['actual-proof'] = proofs
    (directory / 'fallthrough-results.json').write_text(json.dumps(records, indent=2) + '\n')
    print('fallthrough: 16 positive ELF pairs (short/near JA +14 extension addressing); 2 non-EXACT changed controls; %d two-sided geometry negatives; actual V29=%s' % (len(negatives), bool(actual)))


def assemble(directory, label, source, prefix=0):
    asm = directory / (label + '.s')
    obj = directory / (label + '.o')
    asm.write_text(source.replace('PREFIX', '.space %d' % prefix))
    subprocess.run(['as', '--32', '-o', str(obj), str(asm)], check=True,
                   capture_output=True)
    return str(obj)


def mutate(directory, label, path, action):
    raw = bytearray(Path(path).read_bytes())
    elf = ELFFile(io.BytesIO(raw))
    action(raw, elf)
    obj = directory / (label + '.o')
    obj.write_bytes(raw)
    return str(obj)


def reloc_field(raw, elf, section, index, value):
    rs = next(s for s in elf.iter_sections() if isinstance(s, RelocationSection)
              and elf.get_section(s['sh_info']).name == section)
    off = rs['sh_offset'] + index * rs['sh_entsize']
    raw[off:off + 4] = value.to_bytes(4, 'little')


def section_flags(raw, elf, section, flags):
    section_field(raw, elf, section, 8, flags)


def section_field(raw, elf, section, field, value):
    i = next(i for i, s in enumerate(elf.iter_sections()) if s.name == section)
    off = elf['e_shoff'] + i * elf['e_shentsize'] + field
    raw[off:off + 4] = value.to_bytes(4, 'little')


def xbegin(target, word=False):
    # Encode directly: assembler support for TSX and operand-size prefix
    # spellings varies. The relative field starts at dot and ends 2/4 bytes
    # later, so the expression preserves the exact original instruction.
    if word:
        return '.byte 0x66,0xc7,0xf8\n.word ' + target + '-.-2'
    return '.byte 0xc7,0xf8\n.long ' + target + '-.-4'


def run(directory, actual=None, blob=None, actual_v29=None):
    run_fallthrough(directory, actual_v29, blob)
    records = {}
    left = assemble(directory, 'left', SOURCE)
    right = assemble(directory, 'right', SOURCE, 16)
    a, c = b.body(left, 'sample'), b.body(right, 'sample')
    assert b.verdict(*a, *c) == ('EXACT', 0)
    assert g.prove(left, 'sample')['identity'] == g.prove(right, 'sample')['identity']
    assert linkedcopies.grade(left, right, 'sample')['grade'] == 'EXACT'
    assert linkedcopies.exact_body_match(a, c)
    assert b.insns(left, 'sample') and b.insns(right, 'sample')
    assert b.alpha_why(b.insns(left, 'sample'), b.insns(right, 'sample')) is not None
    assert not b.alpha_equal(b.insns(left, 'sample'), b.insns(right, 'sample'))
    records['rebased-positive'] = {'grade': 'EXACT', 'proof': g.prove(right, 'sample')}
    # A CI digest pins the same descriptor body, rather than a print-only grade.
    import sys
    sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'ci'))
    import period_metrics
    assert period_metrics.body_digest(*a) == b.body_digest(*a)
    digest_dir = directory / 'digest-input'
    digest_dir.mkdir(exist_ok=True)
    (digest_dir / 'single.o').write_bytes(Path(left).read_bytes())
    assert period_metrics.function_digest(digest_dir, 'sample') == b.body_digest(*a)
    for label, source, expected in [
        ('ordinary-byte', SOURCE.replace('mov $2,%eax', 'mov $3,%eax'), 'BYTES'),
        ('callee-target', SOURCE.replace('call callee', 'call other'), 'RELOC'),
        ('valid-wrong-arm', SOURCE.replace('.long .Lfirst,.Lsecond', '.long .Lsecond,.Lfirst'), 'RELOC'),
    ]:
        path = assemble(directory, label, source, 16)
        grade = b.verdict(*a, *b.body(path, 'sample'))[0]
        assert grade == expected, (label, grade)
        records[label] = {'grade': grade}
    negatives = {
        'extra-table-reference': SOURCE + '.data\n.long .Ltable\n',
        'zero-sized-table-alias': SOURCE.replace('.Ltable:', '.globl alias\n.type alias,@object\nalias:\n.Ltable:'),
        'notype-table-alias': SOURCE.replace('.Ltable:', '.globl alias\nalias:\n.Ltable:'),
        'table-overlap-alias': SOURCE.replace('PREFIX', 'PREFIX\n.globl alias\n.type alias,@object\nalias:\n.space 4\n.size alias,12'),
        'missing-entry': SOURCE.replace('.long .Lfirst,.Lsecond', '.long .Lfirst,0'),
        'interior-target': SOURCE.replace('.long .Lfirst,.Lsecond', '.long .Lfirst+1,.Lsecond'),
        'signed-guard': SOURCE.replace('jbe .Ldispatch', 'jle .Ldispatch'),
        'index-write': SOURCE.replace('cmp $1,%eax', 'cmp $1,%eax\nmov %edx,%eax'),
        'flags-write': SOURCE.replace('cmp $1,%eax', 'cmp $1,%eax\ninc %edx'),
        'partial-index': SOURCE.replace('cmp $1,%eax', 'cmp $1,%ax'),
        'bypassed-guard': SOURCE.replace('.Lfirst:\n', '.Lfirst:\njmp .Ldispatch\n'),
        'table-guard-entry': SOURCE.replace('.long .Lfirst,.Lsecond', '.long .Lguard,.Lsecond'),
    }
    # Copy each unsupported transfer into both ELF sides. Equal function
    # bytes must not turn a missing CFG edge into a destination identity.
    # Explicit bytes also preserve these controls on assemblers that reject
    # MPX/CET prefix syntax (including BND in newer Ubuntu binutils).
    for label, transfer in {
        'notrack-indirect-jump': '.byte 0x3e,0xff,0xe2',
        'notrack-indirect-call': '.byte 0x3e,0xff,0xd2',
        'bnd-indirect-jump': '.byte 0xf2,0xff,0xe2',
        'bnd-indirect-call': '.byte 0xf2,0xff,0xd2',
        'segment-indirect-jump': '.byte 0x2e,0xff,0xe2',
        'bnd-return': '.byte 0xf2,0xc3',
        'rep-return': '.byte 0xf3,0xc3',
        'notrack-return': '.byte 0x3e,0xc3',
        'xbegin-dispatch': xbegin('.Ldispatch'),
        'xbegin16-dispatch': xbegin('.Ldispatch', word=True),
        'xabort': '.byte 0xc6,0xf8,0x01',
        'xend': '.byte 0x0f,0x01,0xd5',
        'jcxz-dispatch': 'jcxz .Ldispatch',
        'loop-dispatch': 'loop .Ldispatch',
        'indirect-jump': 'jmp *%edx',
        'indirect-call': 'call *%edx',
    }.items():
        negatives[label] = SOURCE.replace('.Lsub:', transfer + '\n.Lsub:')
    # Literal far offsets intentionally carry no relocation. The section
    # scanner must refuse them without assuming any segment semantics.
    for label, encoding in {
        'external-literal-far-jump': '.byte 0xea\n.long 20\n.word 0x23',
        'external-literal-far-call': '.byte 0x9a\n.long 20\n.word 0x23',
        'external-literal-far-jump16': '.byte 0x66,0xea\n.word 20,0x23',
        'external-literal-far-call16': '.byte 0x66,0x9a\n.word 20,0x23',
    }.items():
        negatives[label] = SOURCE + '\n.text\nexternal: ' + encoding + '\n'
    # Every byte of every guard/index/dispatch instruction is protected.
    for label in ('sub', 'cmp', 'guard', 'dispatch'):
        size = {'sub': 3, 'cmp': 3, 'guard': 2, 'dispatch': 7}[label]
        for offset in range(size):
            target = '.L%s+%d' % (label, offset)
            negatives['direct-%s-%d' % (label, offset)] = SOURCE + '\n.text\nexternal: jmp ' + target + '\n'
            negatives['relocated-%s-%d' % (label, offset)] = SOURCE + '\n.data\n.long ' + target + '\n'
            negatives['named-%s-%d' % (label, offset)] = SOURCE + '\n.globl alias\n.set alias,' + target + '\n'
            negatives['external-xbegin-%s-%d' % (label, offset)] = SOURCE + '\n.text\nexternal: ' + xbegin(target) + '\n'
            negatives['external-xbegin16-%s-%d' % (label, offset)] = SOURCE + '\n.text\nexternal: ' + xbegin(target, word=True) + '\n'
            negatives['external-bnd-jump-%s-%d' % (label, offset)] = SOURCE + '\n.text\nexternal: .byte 0xf2\njmp ' + target + '\n'
            negatives['external-segment-jump-%s-%d' % (label, offset)] = SOURCE + '\n.text\nexternal: .byte 0x3e\njmp ' + target + '\n'
    for label, source in negatives.items():
        paths = [assemble(directory, label + '-' + str(i), source, 16 * i)
                 for i in range(2)]
        reasons = []
        for path in paths:
            try:
                g.prove(path, 'sample')
            except g.Refused as error:
                reasons.append(str(error))
            else:
                raise AssertionError('geometry accepted: ' + label)
        for path in paths:
            _, relocs = b.body(path, 'sample')
            assert not any(t[0] == g.TAG for _, t in (relocs or {}).values()), label
        records[label] = {'two_sided_refusals': reasons}
    binary_actions = {
        'writable': lambda raw, elf: section_flags(raw, elf, '.rodata', 3),
        'merge': lambda raw, elf: section_flags(raw, elf, '.rodata', 18),
        'overlapping-entry': lambda raw, elf: reloc_field(raw, elf, '.rodata', 1, 2),
        'duplicate-entry': lambda raw, elf: reloc_field(raw, elf, '.rodata', 1, 0),
        'misplaced-dispatch': lambda raw, elf: reloc_field(raw, elf, '.text', 1, g.prove(left, 'sample')['relocation_offset'] - 1),
        'unsupported-relocation': lambda raw, elf: unsupported_reloc(raw, elf),
        'wrong-machine': lambda raw, elf: raw.__setitem__(slice(18, 20), (62).to_bytes(2, 'little')),
        'wrong-file-type': lambda raw, elf: raw.__setitem__(slice(16, 18), (2).to_bytes(2, 'little')),
        'duplicate-table-section-name': lambda raw, elf: section_field(raw, elf, '.spare', 0, elf.get_section_by_name('.rodata')['sh_name']),
        'multiple-symbol-tables': lambda raw, elf: second_symtab(raw, elf),
    }
    for label, action in binary_actions.items():
        reasons = []
        for i, path in enumerate((left, right)):
            changed = mutate(directory, label + '-' + str(i), path, action)
            try:
                g.prove(changed, 'sample')
            except g.Refused as error:
                reasons.append(str(error))
            else:
                raise AssertionError('binary geometry accepted: ' + label)
        records[label] = {'two_sided_refusals': reasons}
    # Lacking ELF context, equal unresolved relocations remain unresolved.
    assert b.verdict(b'\0' * 4, {0: ('R_386_32', ('section', '.rodata', 0))},
                     b'\0' * 4, {0: ('R_386_32', ('section', '.rodata', 0))}) == ('UNRESOLVED', 1)
    if actual:
        original = g.prove(blob, 'RD_create')
        candidate = g.prove(actual, 'RD_create')
        assert original['identity'] == candidate['identity']
        assert b.verdict(*b.body(blob, 'RD_create'), *b.body(actual, 'RD_create')) == ('EXACT', 0)
        rows = [b.insns(p, 'RD_create') for p in (blob, actual)]
        assert list(map(len, rows)) == [77, 77]
        assert fidelity.measure(*rows, b._padding)['longest_run'] == 77
        assert not b.alpha_equal(*rows)
        assert linkedcopies.grade(blob, actual, 'RD_create')['grade'] == 'EXACT'
        records['actual-RD'] = {'reference': original, 'candidate': candidate,
                                'instructions': list(map(len, rows)),
                                'sha256': [hashlib.sha256(Path(p).read_bytes()).hexdigest()
                                           for p in (blob, actual)]}
        for side, path in [('reference', blob), ('candidate', actual)]:
            for mode in ('swap', 'interior'):
                changed = mutate(directory, 'actual-' + side + '-' + mode, path,
                                 lambda raw, elf: mutate_table(raw, elf, g.prove(path, 'RD_create'), mode))
                if mode == 'swap':
                    proof = g.prove(changed, 'RD_create')
                    assert proof['identity'] != original['identity']
                    assert b.verdict(*b.body(blob, 'RD_create'), *b.body(changed, 'RD_create'))[0] == 'RELOC'
                    result = 'RELOC: ordered valid-arm mismatch'
                else:
                    try:
                        g.prove(changed, 'RD_create')
                    except g.Refused as error:
                        result = str(error)
                        assert result == 'table target boundary/owner'
                    else:
                        raise AssertionError('actual interior target accepted')
                records['actual-' + side + '-' + mode] = result
    records['tools'] = {tool: subprocess.check_output([tool, '--version'], text=True).splitlines()[0]
                        for tool in ('as', 'objdump')}
    (directory / 'results.json').write_text(json.dumps(records, indent=2) + '\n')
    print('%d positive ELF pairs; 3 changed-body/ordered-target negatives; %d two-sided geometry negatives; exact-only REG refusal; CI digest/linked-copy/contextless controls passed' %
          (1 + bool(actual), len(negatives) + len(binary_actions)))
    if actual:
        print('4 actual ELF negatives: 2 valid-arm swaps RELOC; 2 interior targets refused')


def mutate_table(raw, elf, proof, mode):
    off = elf.get_section_by_name(proof['table_section'])['sh_offset'] + proof['table_offset']
    if mode == 'swap':
        other = off + 9 * 4
        raw[off:off + 4], raw[other:other + 4] = raw[other:other + 4], raw[off:off + 4]
    else:
        raw[off:off + 4] = (int.from_bytes(raw[off:off + 4], 'little') + 1).to_bytes(4, 'little')


def unsupported_reloc(raw, elf):
    rs = elf.get_section_by_name('.rel.rodata')
    off = rs['sh_offset'] + 4
    info = int.from_bytes(raw[off:off + 4], 'little')
    raw[off:off + 4] = ((info & ~255) | 3).to_bytes(4, 'little')


def second_symtab(raw, elf):
    section_field(raw, elf, '.spare', 4, 2)
    section_field(raw, elf, '.spare', 24, elf.get_section_by_name('.symtab')['sh_link'])
    section_field(raw, elf, '.spare', 36, 16)


if __name__ == '__main__':
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--out', type=Path)
    ap.add_argument('--actual', type=str)
    ap.add_argument('--actual-v29', type=str)
    ap.add_argument('--blob', default=b.BLOB)
    args = ap.parse_args()
    if args.out:
        args.out.mkdir(parents=True, exist_ok=True)
        run(args.out, args.actual, args.blob, args.actual_v29)
    else:
        with tempfile.TemporaryDirectory(prefix='guarded-switch-') as tmp:
            run(Path(tmp), args.actual, args.blob, args.actual_v29)
