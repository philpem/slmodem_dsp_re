#!/usr/bin/env python3
"""Check the two-cell countdown-copy diagnostic, including its losing control."""
import argparse
import hashlib
import json
import re
import subprocess
from pathlib import Path
import playbook_small_patterns as driver
from gcc3_reload_trace import instructions
from gcc3_stage_divergence import chunks
from gcc3_value_carriers_audit import inspect

TARGET = '_ZN19GenericToneDetector7processEPfj'
HEADER = 'int GenericToneDetector::process(float*, unsigned int)'


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--run-dir', type=Path,
                    default=driver.ROOT / 'build/recovery-generic-tone-bbro')
    ap.add_argument('--gentoo-cc1plus', type=Path,
                    help='optional extracted period compiler for read-only GDB symbol/rule evidence')
    args = ap.parse_args()
    root = args.run_dir
    cells = json.loads((root / 'results.json').read_text())['families']['GenericToneDetector']['cells']
    assert set(cells) == {'baseline', 'no-reorder-blocks'}
    base = root / 'GenericToneDetector/baseline/candidate.o'
    metadata = inspect(base)
    result = {}
    streams = {}
    for label, cell in cells.items():
        folder = root / 'GenericToneDetector' / label
        obj = folder / 'candidate.o'
        assert hashlib.sha256(obj.read_bytes()).hexdigest() == cell['object_hash']
        current = inspect(obj)
        for key in ('records', 'allocated', 'nobits', 'relocations'):
            assert current[key] == metadata[key], (label, key)
        verdicts = {name: list(driver.b.verdict(*driver.b.body(driver.b.BLOB, name),
                                              *driver.b.body(str(obj), name)))
                    for name in cell['verdicts']}
        assert verdicts == cell['verdicts'] and len(verdicts) == 7
        assert cell['diagnostic_flags'] == ([] if label == 'baseline' else ['-fno-reorder-blocks'])
        assert verdicts[TARGET] == ['SIZE', 3 if label == 'baseline' else 27]
        stages = {}
        for stage in ('27.flow2', '31.bbro', '34.stack'):
            text, = chunks(folder / ('GenericToneDetector.cpp.' + stage), HEADER)
            streams[label, stage] = text
            decrements = [uid for uid, pattern in instructions(text).items()
                          if 'plus:SI' in str(pattern) and "'-1'" in str(pattern) and "'k'" in str(pattern)]
            assert decrements == ([373, 453] if label == 'baseline' and stage != '27.flow2' else [373])
            stages[stage] = decrements
        result[label] = {'countdown_uids': stages, 'verdicts': verdicts,
                         'changed_bodies': cell.get('changed_bodies', []),
                         'exact_losses': cell.get('losses', [])}
    def normalize(text):
        text = re.sub(r'<function_decl 0x[0-9a-f]+ ', '<function_decl POINTER ', text)
        return re.sub(r'(?m)^(\(code_label\S* \d+ \d+ \d+ \d+) \d+ ', r'\1 LABEL ', text)
    assert normalize(streams['baseline', '27.flow2']) == normalize(streams['no-reorder-blocks', '27.flow2'])
    assert len(result['no-reorder-blocks']['changed_bodies']) == 6
    assert len(result['no-reorder-blocks']['exact_losses']) == 5
    trace = streams['baseline', '31.bbro']
    witnesses = ['Block 20 can\'t be copied because its size = 7.',
                 'Connection: 19 20 21', 'Fallthru edge 19->20 redirected to 22',
                 'Duplicated bb 20 (created bb 22)',
                 '3 [7570] 20 [10000] 2 [10000]',
                 'Basic block 22:', 'freq 2430, maybe hot.']
    assert all(w in trace for w in witnesses)
    assert 'Duplicated bb' not in streams['no-reorder-blocks', '31.bbro']
    report = {'body_verdicts': 14, 'cells': result, 'trace_witnesses': witnesses,
              'scope': 'actual Gentoo compiler diagnostics; original compiler RTL unavailable',
              'conclusion': 'pass attribution only; no supported source change or flag adoption',
              'next_discriminator': 'independent original branch/loop-boundary evidence before source reopening'}
    if args.gentoo_cc1plus:
        compiler = args.gentoo_cc1plus.resolve()
        command = ['gdb', '-q', '-batch', str(compiler),
                   '-ex', 'info line copy_bb_p', '-ex', 'disassemble copy_bb_p',
                   '-ex', 'disassemble connect_traces']
        debug = subprocess.check_output(command, text=True, stderr=subprocess.STDOUT)
        assert 'Line 1002 of "/build/gcc-3.4.2/gcc/bb-reorder.c"' in debug
        assert re.search(r'<\+220>:\s+shl\s+\$0x3,%edi', debug)
        # Actual connect_traces begins with the signed divide-by-ten
        # reciprocal multiply/shift, independently of the fallback C macro.
        connect = debug.split('Dump of assembler code for function connect_traces:')[1]
        assert re.search(r'<\+15>:\s+mov\s+\$0x66666667,%ecx', connect)
        assert re.search(r'<\+22>:\s+imul\s+%ecx', connect)
        assert re.search(r'<\+29>:\s+sar\s+\$0x2,%edx', connect)
        assert '<copy_bb_p>' in debug and '<copy_bb>' in debug
        (root / 'gentoo-compiler-debug.log').write_text(debug)
        report['compiler_debug'] = {'command': command,
                                    'sha256': hashlib.sha256(compiler.read_bytes()).hexdigest(),
                                    'verified_rules': ['entry-frequency divided by ten', 'hot copy size cap multiplied by eight', 'connect_traces calls copy_bb_p then copy_bb'],
                                    'scope': 'actual Gentoo DWARF/disassembly; no original RTL or complete compiler-source equivalence claim'}
    (root / 'audit.json').write_text(json.dumps(report, indent=2) + '\n')
    print('2 full TUs / 14 verdicts: baseline reproduced, flow2 identical except declaration addresses/label numbers')
    print('bbro duplicates UID373 as453; diagnostic suppresses copy but loses5 exact symbols, block SIZE3->SIZE27')
    print('all bindings/data/BSS/nontext relocations equal; no source or profile adoption')


if __name__ == '__main__':
    main()
