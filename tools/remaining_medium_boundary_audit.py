#!/usr/bin/env python3
"""Audit the six current medium-boundary controls without weakening data checks."""
from collections import Counter
import hashlib,json
import playbook_small_patterns as d
from gcc3_value_carriers_audit import inspect
from helper_caller_audit import inspect as named
from gcc3_adid_first_study_audit import masked
from narrow_call_boundary_screen import bodies
from service_constructor_fidelity_audit import strings


def main():
    root=d.ROOT/'build/remaining-medium-boundary'
    ledger=json.loads((root/'results.json').read_text())
    report={};cells=verdicts=0
    for family,record in ledger['families'].items():
        baseline=root/family/'baseline/candidate.o'
        assert baseline.read_bytes()==(root/family/'retained.o').read_bytes()
        base=inspect(baseline);rows={}
        target='_tx_scrambled_ones_state' if family=='cDATAtx' else 'faxvmi_hdlc_frame'
        for label,entry in record['cells'].items():
            path=root/family/label/'candidate.o';now=inspect(path)
            assert hashlib.sha256(path.read_bytes()).hexdigest()==entry['object_hash']
            assert now['records']==base['records'] and now['nobits']==base['nobits']
            assert now['relocations']==base['relocations'] and named(path)['objects']==named(baseline)['objects']
            data={k for k in set(now['allocated'])|set(base['allocated']) if now['allocated'].get(k)!=base['allocated'].get(k)}
            assert data<={'.rodata.str1.1','.rodata.str1.4'} and strings(path)==strings(baseline)
            changed=[n for n in entry['functions'] if masked(path,n)!=masked(baseline,n)]
            expected=({target} if label!='baseline' else set())
            if family=='faxvmi_hdlc' and label!='baseline':
                expected.add('faxvmi_hdlc_unframe')
                fn='faxvmi_hdlc_unframe'
                old_source=(baseline.parent/'faxvmi_hdlc.c').read_text()
                new_source=(path.parent/'faxvmi_hdlc.c').read_text()
                assert d.function(old_source,fn)[2]==d.function(new_source,fn)[2]
                before=d.b.insns(str(baseline),fn);after=d.b.insns(str(path),fn)
                assert len(before)==len(after)==393 and before[31:]==after[31:]
                diffs=[i for i,(a,b) in enumerate(zip(before,after)) if a!=b]
                assert diffs==[14,29,30],diffs
                assert before[12]==after[12]==('xor','%eax,%eax')
                assert before[27]==after[27]==('xor','%ecx,%ecx')
                assert before[14]==('mov','%eax,0x24(%esp)') and after[14]==('mov','%eax,0x1c(%esp)')
                assert before[29:31]==[('mov','%edx,0x20(%esp)'),('mov','%ecx,0x1c(%esp)')]
                assert after[29:31]==[('mov','%ecx,0x24(%esp)'),('mov','%edx,0x20(%esp)')]
                # Both zero definitions survive to the stores; no call/branch,
                # access to the two private spill destinations intervenes.
                for reg,start,end in [('eax',13,15),('ecx',28,31)]:
                    for index in range(start,end):
                        m,o=before[index]
                        assert not o.endswith(','+'%'+reg) and not m.startswith(('call','j'))
                assert before[31][0]=='jmp'
            assert set(changed)==expected
            assert not entry.get('gains') and not entry.get('losses')
            calls=lambda p:Counter(r['target'] for r in bodies(p)[target] if 'target'in r)
            assert calls(path)==calls(baseline)
            proof={}
            if family=='cDATAtx':
                sets=[m for m,o in d.b.insns(str(path),target) if m.startswith('set')]
                assert sets==(['setge'] if 'signed-ready' in label else ['setae']), (label,sets)
                proof={'readiness_SETcc':sets}
            rows[label]={'changed_bodies':changed,'verdicts':entry['verdicts'],'string_section_changes':sorted(data),
                         'operand_proof':proof,'metadata_named_data_nontext_calls_bystanders':'pass'}
            cells+=1;verdicts+=len(entry['verdicts'])
        report[family]=rows
    assert [m for m,o in d.b.insns(d.b.BLOB,'_tx_scrambled_ones_state') if m.startswith('set')]==['setge']
    assert (cells,verdicts)==(6,28)
    report['denominator']={'cells':cells,'shared_body_verdicts':verdicts}
    (root/'audit.json').write_text(json.dumps(report,indent=2)+'\n')
    print(cells,'full-TU cells /',verdicts,'shared verdicts; two selected bodies and an audited zero-store bystander change, no exact gain/loss')


if __name__=='__main__':main()
