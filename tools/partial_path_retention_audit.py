#!/usr/bin/env python3
"""Audit current V32/V8 path controls, closed pole cross and integration."""
import argparse
import hashlib
import json
from pathlib import Path
import playbook_small_patterns as d
from helper_caller_audit import inspect, allocated
from gcc3_adid_first_study_audit import masked
from gcc3_stage_divergence import chunks, sets
from gcc3_reload_trace import instructions

STAGES = ('01.rtl', '24.lreg', '25.greg', '27.flow2', '28.peephole2', '30.rnreg', '35.mach')

def publications(folder, stage):
    selected = chunks(folder/('V8Interface.c.'+stage), 'V8Process')
    assert len(selected) == 1
    result = []
    for uid, pattern in instructions(selected[0]).items():
        for assignment in sets(pattern):
            dest = assignment[1]
            if isinstance(dest, list) and dest[0].startswith('mem') and 'tx_ring_base' in repr(dest):
                result.append(uid)
    return result

def read_order(folder, stage):
    selected = chunks(folder/('V8Interface.c.'+stage), 'V8Process')
    assert len(selected) == 1
    result = []
    for uid, pattern in instructions(selected[0]).items():
        for assignment in sets(pattern):
            dest, src = assignment[1:3]
            if not isinstance(dest, list) or not dest[0].startswith('reg'):
                continue
            text = repr(src)
            if 'mem' in text and 'pole_state' in text:
                result.append('pole')
            if 'mem:HI' in text and "'in'" in text:
                result.append('input')
    assert sorted(result) == ['input', 'pole'], (stage, result)
    return result

def audit_family(root, family, symbol, count):
    ledger = json.loads((root/'results.json').read_text())['families'][family]
    cells = ledger['cells']
    assert len(cells) == count and cells['baseline']['baseline_reproduced']
    basepath = root/family/'baseline/candidate.o'
    base, data = inspect(basepath), allocated(basepath)
    report = {}
    for label, cell in cells.items():
        path = root/family/label/'candidate.o'
        assert hashlib.sha256(path.read_bytes()).hexdigest() == cell['object_hash']
        assert hashlib.sha256((path.parent/Path(ledger['source_path']).name).read_bytes()).hexdigest() == cell['source_hash']
        assert inspect(path) == base and allocated(path) == data
        changed = [n for n in cell['functions'] if masked(path, n) != masked(basepath, n)]
        assert set(changed) <= {symbol}
        assert not cell.get('gains') and not cell.get('losses')
        for fn, verdict in cell['verdicts'].items():
            assert list(d.b.verdict(*d.b.body(d.b.BLOB, fn), *d.b.body(str(path), fn))) == verdict
        row = {'target_verdict': cell['verdicts'][symbol], 'canonical_body_changes': changed,
               'object_hash': cell['object_hash'], 'common_verdicts': len(cell['verdicts'])}
        if symbol == 'V8Process':
            ring = label.startswith('ring-1')
            counts = {stage: publications(path.parent, stage) for stage in STAGES}
            assert all(len(uids) == (1 if ring else 2) for uids in counts.values())
            original = d.b.insns(d.b.BLOB, symbol)
            emitted = d.b.insns(str(path), symbol)
            stores = lambda rows: [i for i, (m, o) in enumerate(rows) if m == 'mov' and ',0x220(' in o]
            assert len(stores(original)) == 1 and len(stores(emitted)) == (1 if ring else 2)
            row['ring_publication_uids'] = counts
            if ring:
                start = next(i for i, (m, o) in enumerate(emitted) if m == 'mov' and o == '0x220(%ebx),%eax')
                assert emitted[start:start+5] == [('mov', '0x220(%ebx),%eax'), ('movzwl', '(%eax),%ecx'),
                                                 ('add', '$0x2,%eax'), ('mov', '%cx,(%edi)'), ('add', '$0x2,%edi')]
                assert emitted[start+5][0] == 'cmp' and emitted[start+6][0] == 'ja'
                assert emitted[start+7] == ('lea', '0x228(%ebx),%eax')
                assert stores(emitted)[0] > start+7
                # All pole-cross cells still fail original pole-before-input.
                input_pos = next(i for i, (m, o) in enumerate(emitted) if m == 'movswl' and o.startswith('0x0(%ebp),'))
                pole_pos = next(i for i, (m, o) in enumerate(emitted) if m == 'movzwl' and o.startswith('0xdba(%ebx),'))
                assert input_pos < pole_pos
                row['pole_read_order_recovered'] = False
        else:
            emitted = d.b.insns(str(path), symbol)
            branch = emitted[5][0]
            assert emitted[4] == ('cmp', '$0xa0,%ax')
            assert branch == ('jbe' if label == 'baseline' else 'ja')
            if label != 'baseline':
                assert emitted[6:8] == [('cwtl', ''), ('mov', '%eax,(%ecx)')]
            row['count_branch'] = branch
        report[label] = row
    return report

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--integrated', action='store_true')
    args = ap.parse_args()
    root = d.ROOT/'build/partial-path-retention'
    reports = {'V32': audit_family(root, 'V32', 'V32FP_GetCleanedSamples', 2),
               'V8Interface': audit_family(root, 'V8Interface', 'V8Process', 2)}
    cross = d.ROOT/'build/v8-ring-pole-capture'
    reports['pole_cross'] = audit_family(cross, 'V8Interface', 'V8Process', 4)
    assert reports['V8Interface']['baseline']['object_hash'] == reports['pole_cross']['baseline']['object_hash']
    assert reports['V8Interface']['ring-1-symbol-0']['object_hash'] == reports['pole_cross']['ring-1-pole-0']['object_hash']
    assert len({r['object_hash'] for r in reports['pole_cross'].values()}) == 4
    observed_reads = {}
    capture = cross/'V8Interface/ring-1-pole-1'
    for stage in ('20.combine', '24.lreg', '25.greg', '27.flow2', '28.peephole2',
                  '30.rnreg', '31.bbro', '33.sched2', '35.mach'):
        observed_reads[stage] = read_order(capture, stage)
        assert observed_reads[stage] == (['pole', 'input'] if int(stage[:2]) < 33 else ['input', 'pole'])
    if args.integrated:
        for family, label, source, obj in (
            ('V32', 'accepted-count-first', 'src/pump/v32/V32.c', 'src_pump_v32_V32.c.o'),
            ('V8Interface', 'ring-1-symbol-0', 'src/v8/V8Interface.c', 'src_v8_V8Interface.c.o')):
            folder = root/family/label
            assert (folder/'candidate.o').read_bytes() == (d.ROOT/'build/tc_out'/obj).read_bytes()
            assert (folder/Path(source).name).read_bytes() == (d.ROOT/source).read_bytes()
    count = sum(r['common_verdicts'] for package in reports.values() for r in package.values())
    assert count == 54
    output = {'source_cells': 8, 'common_body_verdicts': count, 'rows': reports,
              'pole_capture_read_order': observed_reads,
              'partial_source_functions': 2, 'exact_gains': 0, 'exact_losses': 0,
              'static_locator_ownership_checks': 2 if args.integrated else 0,
              'integrated_raw_equal': args.integrated}
    (root/'audit.json').write_text(json.dumps(output, indent=2)+'\n')
    print('Path retention: 8 cells/54 verdicts; V32 accepted fallthrough and V8 ring stores2→1; '
          'seven-stage publication detectors fire; pole capture reversed at sched2; integrated=%s' % args.integrated)

if __name__ == '__main__':
    main()
