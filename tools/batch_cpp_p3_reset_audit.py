#!/usr/bin/env python3
"""Audit the bounded P3 DIL factoring control, including the late merge."""
import hashlib
import json
from pathlib import Path
import playbook_small_patterns as d
from gcc3_value_carriers_audit import inspect, function_chunk
from gcc3_reload_trace import instructions

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT/'build/batch-cpp-p3-reset'
TARGET = '_ZN18V90Phase3Modulator5resetE7PcmTypeh20Phase3ModulatorStatejP5V90JdP5V92JdPK19tagV90DILdescriptorj'

def main():
    results = json.loads((OUT/'results.json').read_text())
    cells = results['families']['V90Phase3Modulator']['cells']
    family = OUT/'V90Phase3Modulator'
    base = family/'baseline/candidate.o'
    candidate = family/'arm-local-dil/candidate.o'
    assert base.read_bytes() == (ROOT/'build/production-before/src_pump_v90_V90Phase3Modulator.cpp.o').read_bytes()
    assert base.read_bytes() == candidate.read_bytes(), 'full raw object changed'
    assert inspect(base) == inspect(candidate), 'full metadata/data audit changed'
    assert len(cells['baseline']['functions']) == 28
    for label, cell in cells.items():
        obj = family/label/'candidate.o'
        source = obj.parent/'V90Phase3Modulator.cpp'
        assert hashlib.sha256(obj.read_bytes()).hexdigest() == cell['object_hash']
        assert hashlib.sha256(source.read_bytes()).hexdigest() == cell['source_hash']
        assert sorted(d.b.sizes(str(obj))) == cell['functions']
        for name, expected in cell['verdicts'].items():
            assert list(d.b.verdict(*d.b.body(d.b.BLOB, name), *d.b.body(str(obj), name))) == expected
        assert cell['verdicts'][TARGET] == ['SIZE', 52]
        assert sum(v[0] == 'EXACT' for v in cell['verdicts'].values()) == 18
        assert not cell.get('changed_bodies') and not cell.get('gains') and not cell.get('losses')
    trace = {}
    for label in cells:
        stages = {}
        for path in sorted((family/label).glob('*.cpp.[0-9][0-9].*')):
            stage = path.name.split('.cpp.')[1]
            # cgraph is not RTL; gcse embeds earlier instruction snapshots
            # with duplicate UIDs, so neither is a single instruction stream.
            if stage in ('00.cgraph', '08.gcse'):
                continue
            chunk = function_chunk(path, 'V90Phase3Modulator::reset(')
            sites = [uid for uid, pattern in instructions(chunk).items()
                     if 'resetDILGenerator' in str(pattern)]
            expected = 1 if label == 'baseline' or int(stage[:2]) >= 27 else 2
            assert len(sites) == expected, (label, stage, sites)
            stages[stage] = sites
        assert len(stages) == 29, (label, len(stages))
        trace[label] = stages
    report = {'cells': 2, 'emitted_body_comparisons': 56,
              'raw_object_equal': True, 'full_metadata_data_equal': True,
              'exact_per_cell': [18, 28], 'gains': [], 'losses': [],
              'reset_bytes': {'original': 479, 'both_controls': 427},
              'stage_streams_checked': 58, 'first_candidate_merge': '27.flow2',
              'excluded_non_stream_dumps': ['00.cgraph', '08.gcse'],
              'DIL_call_uids': trace}
    (OUT/'audit.json').write_text(json.dumps(report, indent=2)+'\n')
    print('2 controls / 56 emitted-body comparisons; raw objects and all metadata/data identical')
    print('58 RTL streams checked; positive source duplication 1 -> 2 survives through postreload; flow2 merges 2 -> 1')
    print('18/28 exact per cell; reset 427/479 bytes; gains 0, losses 0')

if __name__ == '__main__':
    main()
