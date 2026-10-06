#!/usr/bin/env python3
"""Rescore full reset controls and trace late call merging/duplication."""
import hashlib
import json
from pathlib import Path
import playbook_small_patterns as d
from gcc3_value_carriers_audit import inspect, function_chunk
from gcc3_reload_trace import instructions

TARGET = '_ZN18V92Phase3Modulator5resetEs23V92Phase3ModulatorStatejP5V92JaPK19tagV90DILdescriptorj'
STAGES = ('01.rtl', '04.jump', '06.cse', '09.loop', '18.cse2', '20.combine',
          '25.greg', '26.postreload', '27.flow2', '31.bbro', '33.sched2')


def main():
    root = d.ROOT/'build/v92-reset-branch'
    ledger = json.loads((root/'results.json').read_text())
    family = ledger['families']['V92Phase3Modulator']
    retained = d.ROOT/'build/production-before/src_pump_v90_V92Phase3Modulator.cpp.o'
    baseline = root/'V92Phase3Modulator/baseline/candidate.o'
    assert baseline.read_bytes() == retained.read_bytes(), 'raw baseline mismatch'
    metadata = inspect(baseline)
    rows = []
    for label, cell in family['cells'].items():
        obj = root/'V92Phase3Modulator'/label/'candidate.o'
        source = obj.parent/'V92Phase3Modulator.cpp'
        assert hashlib.sha256(obj.read_bytes()).hexdigest() == cell['object_hash']
        assert hashlib.sha256(source.read_bytes()).hexdigest() == cell['source_hash']
        assert sorted(d.b.sizes(str(obj))) == cell['functions']
        for name, expected in cell['verdicts'].items():
            assert list(d.b.verdict(*d.b.body(d.b.BLOB, name), *d.b.body(str(obj), name))) == expected
        current = inspect(obj)
        for key in ('records', 'allocated', 'nobits', 'relocations'):
            assert current[key] == metadata[key], (label, key)
        changed = [name for name in cell['functions']
                   if d.b.body(str(obj), name) != d.b.body(str(baseline), name)]
        assert changed == ([] if label == 'baseline' else [TARGET]), (label, changed)
        trace = {}
        for stage in STAGES:
            dump = obj.parent/('V92Phase3Modulator.cpp.'+stage)
            patterns = instructions(function_chunk(dump, 'V92Phase3Modulator::reset('))
            trace[stage] = [uid for uid, pattern in patterns.items() if '"edprintf"' in str(pattern)]
        expected = 2 if label.startswith('split-1') else 1
        assert len(trace['01.rtl']) == expected
        assert len(trace['25.greg']) == expected
        assert len(trace['26.postreload']) == expected
        assert len(trace['27.flow2']) == 1
        assert len(trace['31.bbro']) == expected
        rows.append({'cell': label, 'size': d.b.sizes(str(obj))[TARGET],
                     'verdict': cell['verdicts'][TARGET], 'edprintf_uids': trace,
                     'changed_bodies': changed})
    report = {'cells': len(rows), 'live_body_grades': sum(len(c['verdicts']) for c in family['cells'].values()),
              'raw_baseline_equal': True, 'metadata_data_bss_nontext_equal': True,
              'gains': [], 'losses': [], 'rows': rows}
    (root/'audit.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
