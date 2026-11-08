#!/usr/bin/env python3
"""SETcc controls for the FIXED-SAVE stack-frame table prover.

`prove_pushed_abi_frame` already admits the exact unprefixed i386 register
form of SETcc and refuses memory, prefixed and non-zero-extending encodings.
`prove_fixed_stack_frame` is the same proof for a `sub $N,%esp; mov %reg,K
(%esp)` prologue, and it refused every SETcc.  This script exercises the port
through the fixed-frame fixture so the admission is proved where it is used,
not only where it was first written.

Positive controls: every condition x byte register, inserted in a dispatch
destination.  Negative controls: the forms the ABI prover already refuses.
The unmodified fixture is also re-proved, so a regression that rejects the
baseline is caught here."""

import runpy
import subprocess
from pathlib import Path

import json

import jumptable as p

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'build/jumptable-fixed-setcc-controls'
fixed = runpy.run_path(str(ROOT / 'tools/toolchain/jumptable_fixed_abi_controls.py'))
SOURCE = fixed['SOURCE']

CONDITIONS = ('o', 'no', 'b', 'ae', 'e', 'ne', 'be', 'a',
              's', 'ns', 'p', 'np', 'l', 'ge', 'le', 'g')
REGISTERS = ('al', 'cl', 'dl', 'bl', 'ah', 'ch', 'dh', 'bh')


def assemble(label, source):
    OUT.mkdir(parents=True, exist_ok=True)
    asm = OUT / (label + '.s')
    obj = OUT / (label + '.o')
    asm.write_text(source)
    command = ['as', '--32', '-o', str(obj), str(asm)]
    subprocess.run(command, check=True, capture_output=True)
    return obj, command


def main():
    records = {}

    obj, _ = assemble('baseline', SOURCE)
    proof = p.prove(str(obj), 'sample')
    records['baseline'] = 'accepted'
    assert proof['identity'] and proof['count'], proof

    for condition in CONDITIONS:
        for register in REGISTERS:
            label = 'setcc-' + condition + '-' + register
            source = SOURCE.replace(
                '.Lfirst:', '.Lfirst:\nset' + condition + ' %' + register)
            obj, _ = assemble(label, source)
            p.prove(str(obj), 'sample')
            records[label] = 'accepted'

    negative = {
        'saved-slot': 'sete 8(%esp)',
        'outgoing-slot': 'sete (%esp)',
        'ordinary-memory': 'sete (%ebx)',
        'indexed-stack': 'sete (%esp,%eax,1)',
        'operand-prefix': '.byte 0x66,0x0f,0x94,0xc0',
        'address-prefix': '.byte 0x67,0x0f,0x94,0xc0',
        'nonzero-extension': '.byte 0x0f,0x94,0xc8',
    }
    for label, instruction in negative.items():
        source = SOURCE.replace('.Lfirst:', '.Lfirst:\n' + instruction)
        obj, _ = assemble('negative-' + label, source)
        try:
            p.prove(str(obj), 'sample')
        except p.Refused as error:
            records[label] = str(error)
        else:
            raise AssertionError('negative accepted: ' + label)

    (OUT / 'results.json').write_text(json.dumps(records, indent=2) + '\n')
    print('%d positive SETcc encodings accepted; %d adversarial forms refused; '
          'baseline re-proved'
          % (len(CONDITIONS) * len(REGISTERS), len(negative)))


if __name__ == '__main__':
    main()
