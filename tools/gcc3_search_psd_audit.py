#!/usr/bin/env python3
"""Audit all declared search/PSD cells and the selected original graphs."""
import argparse
import hashlib
import json
import re
import playbook_small_patterns as d
from helper_caller_audit import inspect, allocated
from gcc3_adid_first_study_audit import masked
from gcc3_stage_divergence import chunks

PSD = '_ZN3Psd7processEPfjS0_NS_12OutputOptionE'
ROOTS = ('gcc3-search-psd-boundaries', 'gcc3-psd-scalar-operands',
         'gcc3-psd-initial-read', 'gcc3-mapping-comparison')
SELECTED = {
    'src/pump/v90/V90MappingParamsInt.cpp': ('gcc3-mapping-comparison', 'V90MappingParamsInt', 'axis-0-1'),
    'src/dsp/psd.cpp': ('gcc3-psd-initial-read', 'psd', 'bins-after-division'),
}


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--integrated', action='store_true')
    args = ap.parse_args()
    report = {}
    count = verdict_count = 0
    for rootname in ROOTS:
        root = d.ROOT / 'build' / rootname
        ledger = json.loads((root / 'results.json').read_text())
        groups = {}
        for family, group in ledger['families'].items():
            cells = group['cells']
            base = root / family / 'baseline/candidate.o'
            rows = {}
            for label, cell in cells.items():
                path = root / family / label / 'candidate.o'
                assert hashlib.sha256(path.read_bytes()).hexdigest() == cell['object_hash']
                assert inspect(path) == inspect(base) and allocated(path) == allocated(base)
                grades = {n: d.b.verdict(*d.b.body(d.b.BLOB,n), *d.b.body(str(path),n))
                          for n in cell['verdicts']}
                assert {n:list(v) for n,v in grades.items()} == cell['verdicts']
                assert not cell.get('gains') and not cell.get('losses')
                changed = [n for n in cell['functions'] if masked(path,n) != masked(base,n)]
                allowed = {'getConstellationsIndex', 'setV92CPpckFromParamsInfo'} if family != 'psd' else {PSD}
                assert set(changed) <= allowed
                rows[label] = {'grades':grades, 'changed_bodies':changed,
                               'metadata_data_unchanged':True}
                count += 1
                verdict_count += len(grades)
            groups[family] = rows
        report[rootname] = groups
    assert (count,verdict_count) == (18,218)
    boundary = d.ROOT / 'build/gcc3-search-psd-boundaries'
    scalar = d.ROOT / 'build/gcc3-psd-scalar-operands'
    initial = d.ROOT / 'build/gcc3-psd-initial-read'
    comparison = d.ROOT / 'build/gcc3-mapping-comparison'
    assert (scalar/'psd/baseline/candidate.o').read_bytes() == (boundary/'psd/axis-1-1/candidate.o').read_bytes()
    assert (initial/'psd/baseline/candidate.o').read_bytes() == (scalar/'psd/axis-1-1/candidate.o').read_bytes()
    assert (comparison/'V90MappingParamsInt/baseline/candidate.o').read_bytes() == (boundary/'V90MappingParamsInt/axis-1-1/candidate.o').read_bytes()
    for family in ('V90MappingParamsInt','psd'):
        assert (boundary/family/'baseline/candidate.o').read_bytes() == (d.ROOT/'build/search-psd-integration'/('src_pump_v90_V90MappingParamsInt.cpp.o' if family != 'psd' else 'src_dsp_psd.cpp.o')).read_bytes()
    chosen_psd = initial/'psd/bins-after-division/candidate.o'
    original = d.b.insns(d.b.BLOB,PSD)
    chosen = d.b.insns(str(chosen_psd),PSD)
    f = lambda ins: [(m,o) for m,o in ins if m.startswith('f')]
    j = lambda ins: [(m,o) for m,o in ins if m.startswith('j')]
    assert len(f(original)) == 60 and f(original) == f(chosen)
    assert len(j(original)) == 27 and j(original) == j(chosen)
    assert f(d.b.insns(str(boundary/'psd/baseline/candidate.o'),PSD)) != f(original)
    assert d.b.alpha_why(original,chosen) is None
    differences = [{'row':i, 'original':a, 'candidate':b}
                   for i,(a,b) in enumerate(zip(original,chosen)) if a != b]
    assert len(original) == len(chosen) and len(differences) == 6
    assert [r['row'] for r in differences] == [175,177,197,198,199,202]
    assert all(r['original'][0] == r['candidate'][0] for r in differences)
    assert d.b.verdict(*d.b.body(d.b.BLOB,PSD),*d.b.body(str(chosen_psd),PSD)) == ('BYTES',6)
    rtl = chunks(initial/'psd/bins-after-division/psd.cpp.01.rtl',
                 'void Psd::process(float*, unsigned int, float*, Psd::OutputOption)')
    assert len(rtl) == 1
    assert re.search(r'reg/v:SF \d+ \[ peak \]',rtl[0])
    assert rtl[0].count('(div:SF') == 3
    report['psd_original_graph'] = {'x87_rows':60, 'branch_rows':27,
                                  'alpha_equivalent':True, 'remaining_rows':differences}
    selected_mapping = comparison/'V90MappingParamsInt/axis-0-1/candidate.o'
    mapping = d.b.insns(str(selected_mapping),'getConstellationsIndex')
    assert mapping[18:24] == original_mapping_entry()
    assert ('cmp','$0x0,%ebx') in mapping and ('ja','.+160') in mapping
    # Publication must precede the count-minus-one group store.
    tail = mapping[-10:]
    assert [m for m,o in tail] == ['mov','mov','mov','mov','mov','inc','mov','dec','mov','jmp']
    report['mapping_boundaries'] = {'guarded_length_entry':mapping[18:24], 'publication_tail':tail}
    if args.integrated:
        for source,(root,family,label) in SELECTED.items():
            folder = d.ROOT/'build'/root/family/label
            assert (d.ROOT/source).read_bytes() == (folder/(family+'.cpp')).read_bytes()
            assert (d.ROOT/('build/tc_out/'+source.replace('/','_')+'.o')).read_bytes() == (folder/'candidate.o').read_bytes()
    report['denominator'] = {'compiled_cells':count,'body_verdicts':verdict_count}
    report['integrated'] = args.integrated
    (d.ROOT/'build/search-psd-audit.json').write_text(json.dumps(report,indent=2)+'\n')
    print('18 cells /218 body verdicts; PSD all60 x87/27 branches match, six register-only rows remain; Mapping boundaries recovered; no exact gains/losses')


def original_mapping_entry():
    return d.b.insns(d.b.BLOB,'getConstellationsIndex')[18:24]


if __name__ == '__main__':
    main()
