#!/usr/bin/env python3
"""Adversarial controls for a taken JBE dispatch across a default return arm."""
import argparse
import json
import subprocess
from pathlib import Path
import byteident as b
import jumptable as p
from jumptable_fixed_abi_controls import SOURCE

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'build/jumptable-taken-guard-controls'
SOURCE = SOURCE.replace('cmp $1,%eax\nja .Ldefault\njmp *.Ltable', '''cmp $1,%eax
mov %ecx,0x100(%esi)
jbe .Ldispatch
xor %eax,%eax
jmp .Lexit
.Ldispatch:
jmp *.Ltable''')


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--candidate', type=Path, required=True)
    args = ap.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    positive = {
        'baseline': SOURCE,
        'rebased': SOURCE.replace('.Ltable:\n', '.space 32\n.Ltable:\n'),
        'swapped-targets': SOURCE.replace('.long .Lfirst,.Lsecond', '.long .Lsecond,.Lfirst'),
        'default-return': SOURCE.replace('xor %eax,%eax\njmp .Lexit\n.Ldispatch:', 'mov 8(%esp),%ebx\nmov 12(%esp),%esi\nadd $16,%esp\nret\n.Ldispatch:'),
    }
    negative = {
        'default-fallthrough': SOURCE.replace('jmp .Lexit\n.Ldispatch:', 'nop\n.Ldispatch:'),
        'default-conditional-fallthrough': SOURCE.replace('jmp .Lexit\n.Ldispatch:', 'je .Lexit\n.Ldispatch:'),
        'default-direct-bypass': SOURCE.replace('jmp .Lexit\n.Ldispatch:', 'jmp .Ldispatch\n.Ldispatch:'),
        'case-direct-bypass': SOURCE.replace('.Lfirst:', '.Lfirst:\njmp .Ldispatch'),
        'guard-entry-bypass': SOURCE.replace('.Lfirst:', '.Lfirst:\njmp .Lguard').replace('jbe .Ldispatch', '.Lguard:\njbe .Ldispatch'),
        'corridor-entry-bypass': SOURCE.replace('.Lfirst:', '.Lfirst:\njmp .Lcorridor').replace('mov %ecx,0x100(%esi)', '.Lcorridor:\nmov %ecx,0x100(%esi)'),
        'external-direct-bypass': SOURCE + '\n.text\nexternal: jmp .Ldispatch\n',
        'external-relocated-bypass': SOURCE + '\n.data\n.long .Ldispatch\n',
        'external-relocated-dispatch-interior': SOURCE + '\n.data\n.long .Ldispatch+1\n',
        'external-relocated-guard-entry': SOURCE.replace('jbe .Ldispatch', '.Lguard:\njbe .Ldispatch') + '\n.data\n.long .Lguard\n',
        'external-relocated-guard-interior': SOURCE.replace('jbe .Ldispatch', '.Lguard:\njbe .Ldispatch') + '\n.data\n.long .Lguard+1\n',
        'external-relocated-corridor-interior': SOURCE.replace('mov %ecx,0x100(%esi)', '.Lcorridor:\nmov %ecx,0x100(%esi)') + '\n.data\n.long .Lcorridor+1\n',
        'named-guard-entry': SOURCE.replace('jbe .Ldispatch', '.globl bypass\nbypass:\njbe .Ldispatch'),
        'named-dispatch-entry': SOURCE.replace('.Ldispatch:', '.globl bypass\nbypass:\n.Ldispatch:'),
        'named-corridor-entry': SOURCE.replace('mov %ecx,0x100(%esi)', '.globl bypass\nbypass:\nmov %ecx,0x100(%esi)'),
        'table-dispatch-target': SOURCE.replace('.long .Lfirst,.Lsecond', '.long .Ldispatch,.Lsecond'),
        'table-guard-target': SOURCE.replace('jbe .Ldispatch', '.Lguard:\njbe .Ldispatch').replace('.long .Lfirst,.Lsecond', '.long .Lguard,.Lsecond'),
        'table-corridor-target': SOURCE.replace('mov %ecx,0x100(%esi)', '.Lcorridor:\nmov %ecx,0x100(%esi)').replace('.long .Lfirst,.Lsecond', '.long .Lcorridor,.Lsecond'),
        'signed-guard': SOURCE.replace('jbe .Ldispatch', 'jle .Ldispatch'),
        'opposite-guard': SOURCE.replace('jbe .Ldispatch', 'ja .Ldispatch'),
        'duplicate-guard': SOURCE.replace('.Lfirst:', '.Lfirst:\njbe .Ldispatch'),
        'flags-clobber-corridor': SOURCE.replace('mov %ecx,0x100(%esi)', 'inc %ecx'),
        'selector-clobber-corridor': SOURCE.replace('mov %ecx,0x100(%esi)', 'mov 0x100(%esi),%eax'),
        'partial-store-corridor': SOURCE.replace('mov %ecx,0x100(%esi)', 'mov %cx,0x100(%esi)'),
        'stack-store-corridor': SOURCE.replace('mov %ecx,0x100(%esi)', 'mov %ecx,0x100(%esp)'),
        'indexed-store-corridor': SOURCE.replace('mov %ecx,0x100(%esi)', 'mov %ecx,0x100(%esi,%eax,1)'),
        'relocated-store-corridor': SOURCE.replace('mov %ecx,0x100(%esi)', 'mov %ecx,table(%esi)'),
        'prefixed-store-corridor': SOURCE.replace('mov %ecx,0x100(%esi)', 'addr16 mov %ecx,0x100(%si)'),
        'additional-corridor-instruction': SOURCE.replace('mov %ecx,0x100(%esi)', 'mov %ecx,0x100(%esi)\nnop'),
        'dispatch-after-release': SOURCE.replace('jmp .Lexit\n.Ldispatch:', 'jmp .Lexit\n.Ldispatch:\nadd $16,%esp'),
    }
    records = {}; objects = {}
    for label, source in {**positive, **negative}.items():
        asm = OUT / (label + '.s'); obj = OUT / (label + '.o'); asm.write_text(source)
        command = ['as', '--32', '-o', str(obj), str(asm)]
        subprocess.run(command, check=True, capture_output=True)
        objects[label] = obj
        try:
            proof = p.prove(str(obj), 'sample'); accepted = True; reason = None
        except p.Refused as error:
            proof = None; accepted = False; reason = str(error)
        assert accepted == (label in positive), (label, reason)
        own = b.body(str(obj), 'sample'); grade = b.verdict(*own, *own)[0]
        assert grade == ('EXACT' if accepted else 'UNRESOLVED'), (label, grade)
        records[label] = {'accepted': accepted, 'reason': reason, 'proof': proof, 'self_grade': grade, 'command': command}
    original = b.body(str(objects['baseline']), 'sample')
    for label, expected in [('rebased', 'EXACT'), ('swapped-targets', 'RELOC')]:
        assert b.verdict(*original, *b.body(str(objects[label]), 'sample'))[0] == expected
    name = '_ZN12V90Resampler11setBllStateE11V90BllStatej'; blob = ROOT / 'ref/slmodemd/dsplibs.o'
    a = p.prove(str(blob), name); c = p.prove(str(args.candidate), name)
    assert a['identity'] == c['identity'] and a['count'] == 16
    assert a['fixed_stack_frame']['outgoing_slots'] == [0]
    assert a['fixed_stack_frame']['direct_abi_calls'] == 16
    assert b.verdict(*b.body(str(blob), name), *b.body(str(args.candidate), name)) == ('EXACT',0)
    (OUT / 'results.json').write_text(json.dumps({'controls': records, 'real': {'original': a, 'candidate': c}, 'assembler': subprocess.check_output(['as', '--version'], text=True)}, indent=2) + '\n')
    print(f'{len(positive)} positive taken-guard controls; {len(negative)} adversarial refusals; rebased EXACT, altered targets RELOC; real 16-slot/16-call pair EXACT')


if __name__ == '__main__':
    main()
