#!/usr/bin/env python3
"""Audit bounded scalar transfer, narrowing falsifiers and integrated output."""
import hashlib
import json
from collections import Counter
import playbook_small_patterns as d
from helper_caller_audit import inspect, allocated
from gcc3_adid_first_study_audit import masked
from gcc3_sine_scalar_transfer import overlays as scalar_overlays
from gcc3_sine_phase_boundary import overlays as owner_overlays
from gcc3_sine_publication_boundary import overlays as publication_overlays
from gcc3_agc_scalar_transfer import overlays as agc_overlays

SINE = '_ZN8SineWaveIffE8generateEPfm'
AGC = '_ZN3AgcIfE7processEPKfPfj'
FAMILIES = (('gcc3-sine-scalar-transfer','VpcmFloModem',scalar_overlays),
            ('gcc3-sine-phase-boundary','VpcmFloModem',owner_overlays),
            ('gcc3-sine-publication-boundary','VpcmFloModem',publication_overlays),
            ('gcc3-agc-scalar-transfer','V90Demodulator',agc_overlays))


def ledger(root):
    return json.loads((d.ROOT/'build'/root/'results.json').read_text())


def main():
    total = verdicts = 0
    rows = {}
    for rootname, family, generator in FAMILIES:
        root = d.ROOT/'build'/rootname
        manifest = ledger(rootname)
        fr = manifest['families'][family]
        base = root/family/'baseline/candidate.o'
        baseline = inspect(base)
        report = {}
        for label, cell in fr['cells'].items():
            path = root/family/label/'candidate.o'
            assert hashlib.sha256(path.read_bytes()).hexdigest() == cell['object_hash']
            overlay = generator(fr['source_path'],label)
            assert {n:hashlib.sha256(t.encode()).hexdigest() for n,t in overlay.items()} == cell['overlay_header_hashes']
            current = inspect(path)
            assert current['records'] == baseline['records'] and current['objects'] == baseline['objects']
            changes = [n for n in cell['functions'] if masked(path,n) != masked(base,n)]
            if family == 'VpcmFloModem':
                assert set(changes) <= {SINE}
            else:
                assert set(changes) <= {AGC,'_ZN14V90Demodulator5resetEj','_ZN14V90Demodulator8progressEPiRjPfj'}
            assert not cell.get('gains') and not cell.get('losses')
            old_data, new_data = allocated(base), allocated(path)
            sections = [n for n in sorted(set(old_data)|set(new_data)) if old_data.get(n) != new_data.get(n)]
            allowed = {'.gnu.linkonce.t.'+(SINE if family == 'VpcmFloModem' else AGC)}
            if family == 'V90Demodulator' and label in ('producer','both'):
                allowed.add('.rodata.cst4')
            assert set(sections) <= allowed
            report[label] = {'body_verdicts':len(cell['verdicts']), 'changed_bodies':changes,
                             'changed_allocated_sections':sections, 'target':cell['verdicts'][SINE if family=='VpcmFloModem' else AGC]}
            total += 1
            verdicts += len(cell['verdicts'])
        rows[rootname] = report
    assert (total,verdicts) == (15,498)
    sine_root = d.ROOT/'build/gcc3-sine-scalar-transfer/VpcmFloModem'
    for first,second in (('scalar-sinl','scalar-sinf'),('scalar-sinl-entry','scalar-sinf-entry')):
        assert (sine_root/first/'candidate.o').read_bytes() == (sine_root/second/'candidate.o').read_bytes()
    chosen = d.ROOT/'build/gcc3-sine-phase-boundary/VpcmFloModem/phase-after-guard-less/candidate.o'
    original, candidate = d.b.insns(d.b.BLOB,SINE),d.b.insns(str(chosen),SINE)
    nonbranch = lambda stream: Counter((m,o) for m,o in stream if not m.startswith('j'))
    assert nonbranch(original) == nonbranch(candidate)
    assert d.b.verdict(*d.b.body(d.b.BLOB,SINE),*d.b.body(str(chosen),SINE)) == ('BYTES',50)
    assert chosen.read_bytes() == (d.ROOT/'build/tc_out/src_pump_v90_VpcmFloModem.cpp.o').read_bytes()
    # Positive/negative narrowing detector: the tempting AGC score gain adds a spill.
    narrow = lambda stream: sum(m == 'fstps' and '(%esp)' in o for m,o in stream)
    assert narrow(d.b.insns(d.b.BLOB,AGC)) == 0
    agc_root = d.ROOT/'build/gcc3-agc-scalar-transfer/V90Demodulator'
    assert narrow(d.b.insns(str(agc_root/'baseline/candidate.o'),AGC)) == 0
    assert narrow(d.b.insns(str(agc_root/'both/candidate.o'),AGC)) == 1
    observations = requests = 0
    for rootname in ('gcc3-expansion-transfer-sine-observation','gcc3-sine-scalar-stack',
                     'gcc3-expansion-transfer-fir-observation','gcc3-expansion-transfer-agc-observation',
                     'gcc3-agc-scalar-stack'):
        report = ledger(rootname)
        for label,cell in report['cells'].items():
            assert cell['raw_object_unchanged']
            trace = cell['trace']
            assert trace['exit_codes'] == [0] and not trace['errors'] and not trace['pending']
            events = trace['events']
            if rootname == 'gcc3-sine-scalar-stack':
                sf = [e for e in events if e['mode']=='SFmode']
                native = label.startswith('scalar')
                assert len(sf) == (0 if native else 1)
                assert all(f['locals_size'] == (8 if native else 12) for f in trace['frames'])
            if 'agc' in rootname:
                agc = [e for e in events if e['assembler_name']==AGC]
                assert len(agc) == 2 and agc[0]['mode']=='SFmode' and agc[0]['size']==4
                assert 'gen_truncdfsf2' in agc[0]['callers']
            requests += len(events)
            observations += 1
    assert (observations,requests) == (13,55)
    prepared = sum(len(f['cells']['baseline']['verdicts']) for root in
                   ('gcc3-expansion-transfer-prepare','gcc3-expansion-transfer-agc-prepare')
                   for f in ledger(root)['families'].values())
    assert prepared == 73
    integration = d.ROOT/'build/expansion-transfer-integration'
    before = json.loads((integration/'objects-before.json').read_text())
    after = {p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in (d.ROOT/'build/tc_out').glob('*.o')}
    assert len(before)==len(after)==300
    assert [n for n in before if before[n]!=after[n]] == ['src_pump_v90_VpcmFloModem.cpp.o']
    assert (integration/'.build-config').read_bytes() == (d.ROOT/'build/tc_out/.build-config').read_bytes()
    owners = [p.name for p in (d.ROOT/'build/tc_out').glob('*.o') if SINE in d.b.sizes(str(p))]
    assert owners == ['src_pump_v90_VpcmFloModem.cpp.o']
    report = {'retained_cells':total,'shared_body_verdicts':verdicts,'preparation_body_verdicts':prepared,
              'raw_observations':observations,'stack_requests':requests,'families':rows,
              'sine_defining_copies':owners,'integrated_changed_objects':1,'unchanged_objects':299,
              'nonbranch_operand_multiset_equal':True,'agc_narrowing_positive_negative_control':True}
    (d.ROOT/'build/expansion-transfer-audit.json').write_text(json.dumps(report,indent=2)+'\n')
    print('15 cells/498 verdicts;3 raw preparations/73 verdicts;13 observations/55 stack requests')
    print('SineWave BYTES50: nonbranch operand multiset original;1/300 objects changes; AGC narrowing detector fires/clears')


if __name__ == '__main__':
    main()
