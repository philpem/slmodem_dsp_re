#!/usr/bin/env python3
"""Validate saved full-TU controls and fire on TxHdxTRN's GCSE transition."""
import json
import re
import subprocess
import playbook_txhdxtrn as fold

driver = fold.driver
root = driver.ROOT


def function_dump(path):
    return path.read_text().split(';; Function TxHdxTRN', 1)[1].split(';; Function', 1)[0]


def disassembly(path):
    return subprocess.check_output(
        ['python3', str(root / 'tools/dis.py'), str(path), 'TxHdxTRN'], text=True,
        stderr=subprocess.DEVNULL)


def main():
    prior = root / 'build/playbook-txhdxtrn/V32TXHDX'
    directory = root / 'build/txhdxtrn-gcse/V32TXHDX'
    results = json.loads((directory.parent / 'results.json').read_text())
    cells = results['families']['V32TXHDX']['cells']
    assert len(cells) == 6
    for label in ('baseline', 'unsigned-word'):
        assert (directory / label / 'candidate.o').read_bytes() == (prior / label / 'candidate.o').read_bytes()
        assert (directory / (label + '-nolm') / 'candidate.o').read_bytes() == (directory / label / 'candidate.o').read_bytes()
    assert cells['baseline']['verdicts']['TxHdxTRN'] == ['BYTES', 45]
    assert cells['unsigned-word']['verdicts']['TxHdxTRN'] == ['BYTES', 44]
    for label, cell in cells.items():
        assert len(cell['functions']) == len(cell['globals']) == 9
        assert not cell.get('gains', [])
        expected = ['TxHdxFinishFrame', 'V32TxHdxModem'] if label.endswith('-nogcse') else []
        assert cell.get('losses', []) == expected
    before = function_dump(prior / 'baseline/V32TXHDX.c.06.cse')
    after = function_dump(prior / 'baseline/V32TXHDX.c.08.gcse')
    assert re.search(r'\(insn 48 .*?\(set \(reg:SI 79 .*?\(mem/s:SI', before, re.S)
    assert 'PRE: redundant insn 48 (expression 4) in bb 3, reaching reg is 102' in after
    assert 'LOCAL COPY-PROP: Replacing reg 79 in insn 49 with reg 102' in after
    rmw = r'\bsub\s+%\w+,0x78\(%\w+\)'
    assert not re.search(rmw, disassembly(directory / 'baseline/candidate.o'))
    assert re.search(rmw, disassembly(driver.b.BLOB))
    for label in ('baseline-nogcse', 'unsigned-word-nogcse'):
        assert re.search(rmw, disassembly(directory / label / 'candidate.o'))
    assert 'movswl (%esi,%edx,2),%eax' in disassembly(prior / 'baseline/candidate.o')
    assert 'movzwl (%esi,%edx,2),%eax' in disassembly(prior / 'unsigned-word/candidate.o')
    print('raw source controls: 2/2; unchanged load-motion controls: 2/2; inventories: 6/6 (9 functions/globals each)')
    print('known GCSE transition: 1/1; memory-RMW exposure: 2/2; load-extension controls: 2/2')
    print('exact gains: 0/6; no-GCSE exact losses: 2 per source; no candidate adopted')


if __name__ == '__main__':
    main()
