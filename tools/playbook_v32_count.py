#!/usr/bin/env python3
"""Four bounded V32 count-memory/array-root controls on the complete period TU."""
import argparse
import json
import re
import playbook_small_patterns as driver


def variants(path, source):
    start, end, fn = driver.function(source, 'RxHdxSequenceE')
    count = '\tn = DemodDataV32(modem, in, out, *count);\n\t*count = n;\n\tDescrambleDataV32(modem, (short *)out, n);'
    rate = '\t\tregs = hdx->regs;\n\t\tregs[V32HDX_REG_RATE_SEQ] = (short)GetSequence(modem);'
    assert fn.count(count) == fn.count(rate) == fn.count('\tunsigned short n;\n') == 1
    cells = {}
    for label, direct_count, direct_array in [('baseline', False, False),
                                            ('direct-array', False, True),
                                            ('count-memory', True, False),
                                            ('both', True, True)]:
        body = fn
        if direct_count:
            body = body.replace('\tunsigned short n;\n', '')
            body = body.replace(count, '\t*count = DemodDataV32(modem, in, out, *count);\n\tDescrambleDataV32(modem, (short *)out, *count);')
            assert not re.search(r'\bn\b', body)
        if direct_array:
            body = body.replace(rate, '\t\thdx->regs[V32HDX_REG_RATE_SEQ] = (short)GetSequence(modem);')
        cells[label] = source[:start] + body + source[end:]
    assert len(cells) == len(set(cells.values())) == 4
    return cells


def write_analysis():
    import hashlib
    import json
    import subprocess
    root = driver.ROOT
    output = root / 'build' / driver.OUT_NAME
    result = json.loads((output / 'results.json').read_text())
    cells = result['families']['V32rxhdx']['cells']
    expected = {'baseline': (True, False), 'direct-array': (False, False),
                'count-memory': (True, True), 'both': (False, True)}
    assert set(cells) == set(expected)
    analysis = {'domain': result['domain'], 'revision': result['revision'],
                'control_count': 4, 'controls_passed': 0, 'cells': {}}
    for name, wanted in expected.items():
        obj = output / 'V32rxhdx' / name / 'candidate.o'
        cell = cells[name]
        assert hashlib.sha256(obj.read_bytes()).hexdigest() == cell['object_hash']
        dis = subprocess.check_output(['python3', str(root / 'tools/dis.py'),
                                       str(obj), 'RxHdxSequenceE'], text=True,
                                      stderr=subprocess.DEVNULL)
        region = dis[dis.index('R_386_PC32 DemodDataV32'):].split('R_386_PC32 DescrambleDataV32')[0]
        actual = (bool(re.search(r'add\s+\$0x3c', dis)),
                  region.index('mov    %ax,') < region.index('movzwl %ax,'))
        assert actual == wanted, 'known graph control failed: ' + name
        analysis['controls_passed'] += 1
        analysis['cells'][name] = {'rate_store_rebased': actual[0],
                                   'count_store_before_widen': actual[1],
                                   'verdict': cell['verdicts']['RxHdxSequenceE'],
                                   'functions': len(cell['functions']),
                                   'globals': len(cell['globals']),
                                   'changed_bodies': cell.get('changed_bodies', []),
                                   'gains': cell.get('gains', []),
                                   'losses': cell.get('losses', [])}
        (obj.parent / 'RxHdxSequenceE.dis').write_text(dis)
        rtl = (obj.parent / 'V32rxhdx.c.01.rtl').read_text()
        rtl = rtl.split(';; Function RxHdxSequenceE')[1].split(';; Function')[0]
        (obj.parent / 'RxHdxSequenceE-initial.rtl').write_text(rtl)
    (output / 'analysis.json').write_text(json.dumps(analysis, indent=2) + '\n')
    print('graph controls:', analysis['controls_passed'], '/', analysis['control_count'])


if __name__ == '__main__':
    driver.REV = '5c9f7b61'
    driver.OUT_NAME = 'playbook-v32-count'
    driver.SOURCE_PATHS = ('src/pump/v32/V32rxhdx.c',)
    driver.variants = variants
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--domain', required=True)
    parser.add_argument('--analysis-only', action='store_true')
    args = parser.parse_args()
    if args.analysis_only:
        saved = driver.ROOT / 'build' / driver.OUT_NAME / 'results.json'
        assert json.loads(saved.read_text())['domain'] == args.domain
    else:
        driver.main()
    write_analysis()
