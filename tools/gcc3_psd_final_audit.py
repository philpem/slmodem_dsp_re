#!/usr/bin/env python3
"""Audit the PSD scalar cross and read-only conversion observations."""
import hashlib
import json
import re
import playbook_small_patterns as d
from helper_caller_audit import inspect, allocated
from gcc3_adid_first_study_audit import masked

GETTER = '_ZNK3Psd14getFrequenciesEPff'
PROCESS = '_ZN3Psd7processEPfjS0_NS_12OutputOptionE'


def read(path):
    return json.loads((d.ROOT / 'build' / path / 'results.json').read_text())


def main():
    root = d.ROOT / 'build/gcc3-psd-frequency-scalar/psd'
    ledger = read('gcc3-psd-frequency-scalar')
    assert ledger['config'] == (d.ROOT / 'build/tc_out/.build-config').read_text()
    cells = ledger['families']['psd']['cells']
    baseline = root / 'baseline/candidate.o'
    base = inspect(baseline)
    rows = {}
    for label, cell in cells.items():
        path = root / label / 'candidate.o'
        assert hashlib.sha256(path.read_bytes()).hexdigest() == cell['object_hash']
        current = inspect(path)
        for key in ('records', 'objects', 'allocated_sizes'):
            assert current[key] == base[key], (label, key)
        assert allocated(path) == allocated(baseline)
        changed = [n for n in cell['functions'] if masked(path, n) != masked(baseline, n)]
        assert set(changed) <= {GETTER}
        assert len(cell['verdicts']) == 13
        assert masked(path, PROCESS) == masked(baseline, PROCESS)
        rows[label] = {'changed_bodies': changed, 'getter': cell['verdicts'][GETTER]}
    assert rows['axis-1-1']['getter'][0] == 'EXACT'
    assert len(d.b.body(str(root / 'axis-1-1/candidate.o'), GETTER)[0]) == 77
    assert (d.ROOT / 'src/dsp/psd.cpp').read_bytes() == (root / 'axis-1-1/psd.cpp').read_bytes()
    stack = read('gcc3-psd-frequency-scalar-stack')['cells']
    requests = 0
    for label, cell in stack.items():
        assert cell['raw_object_unchanged']
        events = cell['trace']['events']
        scalar = label in ('axis-1-0', 'axis-1-1')
        assert len(events) == (1 if scalar else 2)
        sf = [e for e in events if e['mode'] == 'SFmode']
        assert len(sf) == (0 if scalar else 1)
        if sf:
            assert sf[0]['size'] == 4 and 'gen_truncxfsf2' in sf[0]['callers']
        assert all(f['locals_size'] == (0 if scalar else 4) for f in cell['trace']['frames'])
        requests += len(events)
    unchanged = next(iter(read('gcc3-psd-frequency-stack')['cells'].values()))
    assert unchanged['raw_object_unchanged'] and len(unchanged['trace']['events']) == 2
    scratch = next(iter(read('gcc3-psd-conversion-plain-observation')['cells'].values()))
    assert scratch['raw_object_unchanged']
    events = scratch['trace']['events']
    process = [e for e in events if e['function'] == 'process']
    assert len(events) == 9 and {e['insn_uid'] for e in process} == {464, 196, 130, 71, 65}
    rename = read('gcc3-psd-rename-observation')
    assert rename['raw_assembly_object_equal'] and len(rename['trace']['events']) == 44
    selections = {}
    for chain, old, selected in (((787,790),0,4), ((831,789),1,1),
                                 ((780,783),0,3), ((828,782),1,0)):
        hits = [e for e in rename['trace']['events'] if tuple(x['uid'] for x in e['chain']) == chain]
        assert len(hits) == 1 and (hits[0]['old'], hits[0]['selected']) == (old, selected)
        selections[str(chain)] = hits[0]
    rtlroot = d.ROOT / 'build/gcc3-psd-conversion-plain/psd/baseline'
    def instruction(stage, uid):
        text = (rtlroot / ('psd.cpp.' + stage)).read_text()
        hits = re.findall(r'\(insn(?:\:[^ ]+)? '+str(uid)+r' .*?(?=\n\((?:insn|note|jump_insn|call_insn|code_label)|\Z)', text, re.S)
        assert len(hits) == 1
        return hits[0]
    for uid in (788,781):
        text = instruction('27.flow2',uid)
        assert '(set (reg:SI 1 dx [ frames+4 ])' in text and '(const_int 0' in text
    for uid in (831,828):
        assert '(reg:SI 1 dx [ frames+4 ])' in instruction('28.peephole2',uid)
    for uid, reg in ((787,4),(831,1),(780,3),(828,0)):
        assert '(reg:SI '+str(reg)+' ' in instruction('30.rnreg',uid)
    canonical = d.ROOT / 'build/tc_out/src_dsp_psd.cpp.o'
    assert canonical.read_bytes() == (root / 'axis-1-1/candidate.o').read_bytes()
    archived = (d.ROOT / 'build/psd-final-integration/src_dsp_psd.cpp.o').read_bytes()
    assert baseline.read_bytes() == archived
    for preparation in ('gcc3-psd-conversion-prepare', 'gcc3-psd-conversion-plain'):
        prepared = d.ROOT / 'build' / preparation / 'psd/baseline/candidate.o'
        assert prepared.read_bytes() == archived
        ledger = read(preparation)['families']['psd']['cells']['baseline']
        assert len(ledger['verdicts']) == 13
    report = {'cells': rows, 'body_verdicts':52, 'getter_stack_observations':5,
              'getter_stack_requests':requests+2, 'scratch_events':9,
              'process_scratch_events':5, 'process_rename_decisions':44,
              'conversion_rename_selections': selections}
    (d.ROOT / 'build/psd-final-audit.json').write_text(json.dumps(report,indent=2)+'\n')
    print('4 cells/52 body verdicts; only getter changes; winner EXACT77B')
    print('5 raw-equal getter observations/8 stack requests; 9 scratch events/5 process; 44 rename decisions')


if __name__ == '__main__':
    main()
