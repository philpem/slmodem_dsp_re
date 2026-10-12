#!/usr/bin/env python3
"""Audit ADID scheduling packets and bounded reference-code ownership controls."""
import hashlib
import json
import re
import playbook_small_patterns as d
from helper_caller_audit import inspect, allocated
from gcc3_adid_first_study_audit import masked

TARGET = '_ZN25V90AutoDigitalImpDetector17porcessFirstStudyEv'
FAMILY = 'V90AutoDigitalImpDetector'
MARKER = ';; Function void V90AutoDigitalImpDetector::porcessFirstStudy()'

def target(text):
    assert MARKER in text
    return text.split(MARKER, 1)[1].split(';; Function ', 1)[0]

def main():
    root = d.ROOT/'build/gcc3-adid-first-study-schedule'
    cell = json.loads((root/'results.json').read_text())['families'][FAMILY]['cells']['baseline']
    folder = root/FAMILY/'baseline'
    assert cell['compile_exit'] == 0 and cell['baseline_reproduced']
    assert (folder/'candidate.o').read_bytes() == (d.ROOT/'build/gcc3-adid-first-study-stages'/FAMILY/'baseline/candidate.o').read_bytes()
    text = target((folder/(FAMILY+'.cpp.33.sched2')).read_text())
    region = text.split('-- basic block 26 from 977 to 1002 -- after reload', 1)[1]
    region = region.split('scheduling visualization', 1)[0]
    rows = {}
    for line in region.splitlines():
        m = re.match(r'^;;\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+.*?:\s*([0-9 ]*)\s*$', line)
        if m:
            uid, code, bb, dep, pri, cost, edges = m.groups()
            rows[int(uid)] = {'code': int(code), 'incoming': int(dep), 'priority': int(pri),
                              'cost': int(cost), 'outgoing': [int(x) for x in edges.split()]}
    assert set(rows) == {977, 978, 497, 508, 912, 1002}
    assert [rows[x]['priority'] for x in (977, 978, 497, 508, 912)] == [6, 5, 1, 1, 5]
    assert 912 in rows[977]['outgoing'] and 912 in rows[978]['outgoing']
    assert not any(x in rows[497]['outgoing'] for x in (977, 978, 912, 508))
    assert 'Ready list after ready_sort:    508  497  977' in region
    assert 'Ready list (t =  0):    508  977  497' in region
    assert 'PPRO packet: 497 977' in region
    issued = [int(x) for x in re.findall(r'scheduling insn <<<(\d+)>>>', region)]
    assert issued == [497, 977, 978, 912, 508, 1002]
    domain = d.ROOT/'build/gcc3-adid-first-study-clear-capture'
    cells = json.loads((domain/'results.json').read_text())['families'][FAMILY]['cells']
    assert len(cells) == 4 and cells['baseline']['baseline_reproduced']
    basepath = domain/FAMILY/'baseline/candidate.o'
    base, data = inspect(basepath), allocated(basepath)
    reports = {}
    for label, candidate in cells.items():
        path = domain/FAMILY/label/'candidate.o'
        assert hashlib.sha256(path.read_bytes()).hexdigest() == candidate['object_hash']
        assert hashlib.sha256((path.parent/(FAMILY+'.cpp')).read_bytes()).hexdigest() == candidate['source_hash']
        assert inspect(path) == base and allocated(path) == data
        changed = [n for n in candidate['functions'] if masked(path, n) != masked(basepath, n)]
        assert set(changed) <= {TARGET}
        assert len(candidate['verdicts']) == 34
        assert not candidate.get('gains') and not candidate.get('losses')
        for fn, grade in candidate['verdicts'].items():
            assert list(d.b.verdict(*d.b.body(d.b.BLOB, fn), *d.b.body(str(path), fn))) == grade
        assert candidate['verdicts'][TARGET] == ['SIZE', 16 if label == 'baseline' else 24]
        reports[label] = {'object_hash': candidate['object_hash'], 'target_verdict': candidate['verdicts'][TARGET],
                          'canonical_body_changes': changed}
    assert len({v['object_hash'] for k, v in reports.items() if k != 'baseline'}) == 1
    # No nearest-size adoption: every original-supported capture spells the
    # same nonoriginal extra index load and maintained-index loss.
    assert (d.ROOT/'build/tc_out/src_pump_v90_V90AutoDigitalImpDetector.cpp.o').read_bytes() == basepath.read_bytes()
    output = {'diagnostic_raw_repeats': 1, 'dependency_rows': rows, 'issued': issued,
              'source_cells': 4, 'common_body_verdicts': 136, 'reports': reports,
              'source_adopted': False, 'exact_gains': 0, 'exact_losses': 0}
    (domain/'audit.json').write_text(json.dumps(output, indent=2)+'\n')
    print('ADID schedule audit: raw-equal verbose TU; ready sort→PPRO packet reorder fires; '
          '4 source cells/136 verdicts, three alternatives raw-identical and declined')

if __name__ == '__main__':
    main()
