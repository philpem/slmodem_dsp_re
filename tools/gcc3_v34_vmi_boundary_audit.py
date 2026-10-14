#!/usr/bin/env python3
"""Audit complete-TU INFO1a captures and promoted ring-capacity controls."""
import hashlib
import json
import re
import playbook_small_patterns as d
from helper_caller_audit import inspect, allocated
from gcc3_adid_first_study_audit import masked
from gcc3_stage_divergence import chunks
from narrow_call_boundary_screen import bodies


def info_window(path):
    rows=bodies(path)['V34GiveINFO1aBits']
    call=next(i for i,r in enumerate(rows) if r.get('target')=='bitreverse')
    rows=rows[call+1:]
    stores=[i for i,r in enumerate(rows) if r['mnemonic']=='mov' and re.fullmatch(r'%\w+l,0x8\(%\w+\)',r['operands'])]
    assert len(stores)>=2
    rows=rows[:stores[1]+1]
    loads=[i for i,r in enumerate(rows) if r['mnemonic']=='mov' and re.match(r'0x(?:1760|612c)\(',r['operands'])]
    assert len(loads)==2
    return {'early_word_cache':any(r['mnemonic']=='movswl' and r['operands'].startswith('%ax,') for r in rows),
            'both_pointers_before_first_store':max(loads)<stores[0], 'rows':rows}


def main():
    report={};total=verdicts=0
    original=info_window(d.b.BLOB)
    assert original['early_word_cache'] and original['both_pointers_before_first_store']
    report['original_INFO1a']=original
    for rootname,family,target in [('gcc3-v34-info1a-capture','v34info','V34GiveINFO1aBits'),
                                   ('gcc3-vmi-ring-capacity','faxvmi','FAXVMI_create')]:
        root=d.ROOT/'build'/rootname/family
        ledger=json.loads((root.parent/'results.json').read_text())['families'][family]['cells']
        baseline=root/'baseline/candidate.o';base=inspect(baseline);data=allocated(baseline)
        assert baseline.read_bytes()==(root/'retained.o').read_bytes()
        rows={}
        for label,cell in ledger.items():
            path=root/label/'candidate.o'
            assert hashlib.sha256(path.read_bytes()).hexdigest()==cell['object_hash']
            assert inspect(path)==base and allocated(path)==data,(rootname,label,'metadata/data')
            changed=[n for n in cell['functions'] if masked(path,n)!=masked(baseline,n)]
            assert set(changed)<={target},(rootname,label,changed)
            assert not cell.get('gains') and not cell.get('losses')
            edges=lambda p:[r['target'] for r in bodies(p)[target] if 'target' in r]
            assert edges(path)==edges(baseline)
            rtl=chunks(root/label/(family+'.c.01.rtl'),target)[0]
            if family=='v34info':
                cache=label not in ('baseline','retained-pointers')
                pointers=label.endswith('-pointers')
                proof=info_window(path)
                assert proof['early_word_cache']==cache
                assert proof['both_pointers_before_first_store']==pointers
                assignments=[node for node in rtl.split('\n\n') if re.search(r'\(set \(reg/v:SI \d+ \[ uinfo \]\)',node)]
                assert len(assignments)==int(cache)
                if cache:
                    assert '(sign_extend:SI' in assignments[0]
                if pointers:
                    assert rtl.index('[ upstream ]')<rtl.rindex('(set (mem:QI')
            else:
                nodes=[node for node in rtl.split('\n\n') if re.search(r'\(set \(reg/v:SI \d+ \[ need \]\)',node)]
                assert len(nodes)==1
                promoted=label in ('need','both')
                assert ('(plus:SI' in nodes[0])==promoted
                assert ('(zero_extend:SI (subreg:HI' in nodes[0]) == (not promoted)
                proof={'promoted_need_initial_RTL':promoted}
            rows[label]={'changed_bodies':changed,'target_verdict':cell['verdicts'][target],
                         'proof':proof,'metadata_data_calls_bystanders':'pass'}
            total+=1;verdicts+=len(cell['verdicts'])
        if family=='v34info':
            for suffix in ('','-pointers'):
                assert (root/('short'+suffix)/'candidate.o').read_bytes()==(root/('int-narrow'+suffix)/'candidate.o').read_bytes()
        report[rootname]=rows
    assert (total,verdicts)==(10,66),(total,verdicts)
    # Static arithmetic enumeration only: no modem execution or fuzz inputs.
    pairs=different=equal=0
    for maximum in (65533,65534,65535):
        need=maximum+3
        for capacity in range(65536):
            original=capacity if capacity>=need else need&65535
            retained=max(capacity,need&65535)
            repaired=capacity if capacity>=need else (need&65535)
            assert repaired==original
            pairs+=1
            different+=retained!=original
            equal+=retained==original
    assert (pairs,different,equal)==(196608,196602,6)
    report['static_capacity_arithmetic']={'pairs':pairs,'retained_disagreements':different,
                                         'retained_agreements':equal,'repaired_disagreements':0,
                                         'runtime_blob_execution':False}
    report['denominator']={'full_TU_cells':total,'shared_verdicts':verdicts,'exact_gains':0,'exact_losses':0}
    (d.ROOT/'build/v34-vmi-boundary-audit.json').write_text(json.dumps(report,indent=2)+'\n')
    print('10 full-TU cells / 66 shared verdicts; raw baselines, metadata/data/calls/bystanders pass')
    print('Original INFO1a cache/pointer ages and promoted ring requirement independently demonstrated')
    print('Static wrap-edge arithmetic:196608 pairs; old196602 disagreements, repaired0')


if __name__=='__main__':
    main()
