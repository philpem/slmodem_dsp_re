#!/usr/bin/env python3
"""Audit raw scheduler repeats, conversion movement and complete TU controls."""
import argparse
import hashlib
import json
import re
from pathlib import Path
import playbook_small_patterns as d
from eia6_x87_audit import stream, floating, assembly_packets, TARGET
from gcc3_value_carriers_audit import inspect, function_chunk
from fse_scheduler_dependencies_audit import dependency_rows

PACKAGES = ('eia6-scheduler', 'eia6-fraction-use', 'eia6-magnitude-lifetime', 'eia6-tail-reload')
STAGES = ('01.rtl', '19.life', '20.combine', '22.regmove', '24.lreg',
          '27.flow2', '31.bbro', '33.sched2', '34.stack')


def pattern(node):
    return next(child for child in node[2:] if isinstance(child, list))


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--prior-root', type=Path, required=True,
                    help='PR279 build/eia6-x87-default-math directory')
    args = ap.parse_args()
    production = d.ROOT/'build/production-before/src_pump_v90_V90PreFilter.cpp.o'
    metadata = inspect(production)
    report = {'cells': [], 'body_grades': 0, 'raw_repeats': 0, 'streams': 0,
              'gains': [], 'losses': []}
    known = {}
    for package in PACKAGES:
        root = d.ROOT/'build'/package
        ledger = json.loads((root/'results.json').read_text())
        assert ledger['revision'] == '75e7ef4b'
        cells = ledger['families']['V90PreFilter']['cells']
        assert (root/'V90PreFilter/baseline/candidate.o').read_bytes() == production.read_bytes()
        for label, cell in cells.items():
            folder = root/'V90PreFilter'/label
            obj = folder/'candidate.o'
            assert hashlib.sha256(obj.read_bytes()).hexdigest() == cell['object_hash']
            assert hashlib.sha256((folder/'V90PreFilter.cpp').read_bytes()).hexdigest() == cell['source_hash']
            assert sorted(d.b.sizes(str(obj))) == cell['functions']
            for symbol, grade in cell['verdicts'].items():
                assert list(d.b.verdict(*d.b.body(d.b.BLOB, symbol),
                                      *d.b.body(str(obj), symbol))) == grade
            report['body_grades'] += len(cell['functions'])
            changed = [n for n in cell['functions'] if d.b.body(str(obj), n) != d.b.body(str(production), n)]
            assert changed == ([] if label == 'baseline' else [TARGET])
            expected_gains = [TARGET] if label == 'tail-reload-else' else []
            assert cell.get('gains', []) == expected_gains and not cell.get('losses')
            if expected_gains:
                assert cell['verdicts'][TARGET][0] == 'EXACT'
                report['gains'].extend(expected_gains)
            current = inspect(obj)
            for key in ('records', 'nobits', 'relocations', 'allocated'):
                assert current[key] == metadata[key], (package, label, key)
            if package == 'eia6-scheduler':
                assert obj.read_bytes() == (args.prior_root/'V90PreFilter'/label/'candidate.o').read_bytes()
                report['raw_repeats'] += 1
            if label in known:
                assert obj.read_bytes() == known[label]
                report['raw_repeats'] += 1
            known[label] = obj.read_bytes()
            traces = {stage: floating(folder/('V90PreFilter.cpp.'+stage)) for stage in STAGES}
            report['streams'] += len(traces)
            packets = assembly_packets(folder/'V90PreFilter.s')
            assert all(op['uid'] in packets for op in traces['34.stack'])
            report['cells'].append({'package': package, 'label': label,
                                    'grade': cell['verdicts'][TARGET],
                                    'traces': traces, 'assembly_packets': packets})
    folder = d.ROOT/'build/eia6-scheduler/V90PreFilter/sf-default-math'
    stages = {s: stream(folder/('V90PreFilter.cpp.'+s)) for s in STAGES}
    life = {int(n[1]): n for n in stages['19.life']}
    combine = {int(n[1]): n for n in stages['20.combine']}
    assert 'fix:SI' in str(pattern(life[183]))
    assert 'fix:SI' not in str(pattern(life[207]))
    assert 183 not in combine and 'fix:SI' in str(pattern(combine[207]))
    assert list(life).index(183) < list(life).index(206)
    assert list(combine).index(207) > list(combine).index(206)
    bbro = [int(n[1]) for n in stages['31.bbro']]
    sched = [int(n[1]) for n in stages['33.sched2']]
    assert bbro.index(181) > bbro.index(422) and sched.index(181) < sched.index(422)
    text = function_chunk(folder/'V90PreFilter.cpp.33.sched2', 'V90PreFilter::setParamEia6(')
    rows = dependency_rows(text)
    issued = [int(x) for x in re.findall(r'scheduling insn <<<(\d+)>>>', text)]
    assert rows[181]['incoming'] == 2
    incoming = sorted(uid for uid, row in rows.items() if 181 in row['outgoing'])
    assert incoming == [24, 114]
    assert issued.index(181) < issued.index(168) < issued.index(422)
    report['scale_dependencies'] = {'incoming': incoming, 'row': rows[181],
                                    'issue_indices': {uid: issued.index(uid) for uid in (114, 181, 168, 422)}}
    assert known['fraction-builtin-abs'] == known['fraction-explicit-if']
    report['fraction_use_identical_objects'] = True
    early = d.b.insns(str(d.ROOT/'build/eia6-magnitude-lifetime/V90PreFilter/early-live-magnitude/candidate.o'), TARGET)
    original = d.b.insns(d.b.BLOB, TARGET)
    assert early[:109] == original[:109]
    assert early[163] == ('jmp', '.+482') and original[163] == ('jmp', '.+485')
    report['early_magnitude_exact_prefix_instructions'] = 109
    output = d.ROOT/'build/eia6-scheduler-audit.json'
    output.write_text(json.dumps(report, indent=2)+'\n')
    print(f"EIA6 scheduling audit: {len(report['cells'])} cells / {report['body_grades']} live grades / "
          f"{report['streams']} streams / {report['raw_repeats']} raw repeats; 1 gain / 0 losses")
    print('Known detections: combine sinks fraction FIX183→207; sched2 hoists scale181; builtin/if objects identical')


if __name__ == '__main__':
    main()
