#!/usr/bin/env python3
"""Audit three partial boundaries and all complete-TU collateral."""
import playbook_small_patterns as d
from gcc3_value_carriers_audit import inspect
from gcc3_adid_first_study_audit import masked
from narrow_call_boundary_screen import bodies,sha
from collections import Counter
import json
import re
from pathlib import Path

FAMILIES={'v23rx':('v23FP_rx_progress','unsigned-MRF'),
          'V21r_prc':('RxNextStateV21','post-diagnostic-owner'),
          'faxvmi_hdlc':('faxvmi_hdlc_unframe','current-parent-reads')}


def diagnostic_reload_rows(path):
    rows=bodies(path)['RxNextStateV21']
    return [rows[i+1] for i,r in enumerate(rows) if r.get('target')=='dsplibs_debug_printf'
            and i+1<len(rows) and rows[i+1]['mnemonic']=='mov'
            and re.fullmatch(r'0x4c\(%\w+\),%\w+',rows[i+1]['operands'])]


def frame_chains(path):
    rows=bodies(path)['faxvmi_hdlc_unframe'];found=[]
    for i,row in enumerate(rows):
        if row['mnemonic']!='mov':continue
        m=re.fullmatch(r'0x24\((%e(?:ax|bx|cx|dx|si|di|bp))\),(%\w+)',row['operands'])
        if not m:continue
        for following in rows[i+1:i+4]:
            if following['mnemonic']=='mov' and following['operands'].startswith('0x40('+m[2]+'),'):
                found.append([row,following]);break
    return found


def main():
    root=d.ROOT/'build/gcc3-partial-boundary-recoveries'
    ledger=json.loads((root/'results.json').read_text())['families']
    report={};total=verdicts=0
    assert len(diagnostic_reload_rows(d.b.BLOB))==3
    assert len(frame_chains(d.b.BLOB))>=3
    for family,(target,selected) in FAMILIES.items():
        cells=ledger[family]['cells'];basepath=root/family/'baseline/candidate.o';base=inspect(basepath)
        assert basepath.read_bytes()==(root/family/'retained.o').read_bytes()
        baseline_source=(root/family/'baseline'/Path(ledger[family]['source_path']).name).read_text()
        family_report={}
        for label,cell in cells.items():
            path=root/family/label/'candidate.o';now=inspect(path)
            assert sha(path)==cell['object_hash']
            sourcepath=path.parent/Path(ledger[family]['source_path']).name
            assert sha(sourcepath)==cell['source_hash']
            assert now['records']==base['records'] and now['nobits']==base['nobits']
            assert now['relocations']==base['relocations']
            changed=[n for n in cell['functions'] if masked(path,n)!=masked(basepath,n)]
            expected={target}
            if family=='V21r_prc':expected|={'RxHdxDataV21','RxHdxWaitV21','RxHdxStartV21'}
            assert set(changed)==(set() if label=='baseline' else expected),(family,label,changed)
            for name in changed:
                calls=lambda p:Counter(r['target'] for r in bodies(p)[name] if 'target' in r)
                assert calls(path)==calls(basepath),(family,label,name,'call inventory')
                if name!=target:
                    assert d.function(sourcepath.read_text(),name)[2]==d.function(baseline_source,name)[2]
            pool={}
            if family=='faxvmi_hdlc' and label!='baseline':
                differences=[k for k in base['allocated'] if base['allocated'][k]!=now['allocated'][k]]
                assert set(base['allocated'])==set(now['allocated']) and differences==['.rodata.str1.1']
                a=bytes.fromhex(base['allocated'][differences[0]]);b=bytes.fromhex(now['allocated'][differences[0]])
                assert len(a)==len(b)==197 and Counter(a.split(b'\0'))==Counter(b.split(b'\0'))
                pool={'section':differences[0],'bytes':len(a),'same_literal_multiset':True,'before':a.hex(),'after':b.hex()}
            else:assert now['allocated']==base['allocated']
            assert not cell.get('gains') and not cell.get('losses')
            proof={}
            if family=='V21r_prc':
                rows=diagnostic_reload_rows(path);assert len(rows)==(0 if label=='baseline' else 3)
                proof={'known_state_post_debug_reloads':rows}
            elif family=='v23rx':
                rtl=(path.parent/'v23rx.c.02.sibling').read_text().split(';; Function v23FP_rx_progress',1)[1]
                prefix=rtl[:rtl.index('(\"FPM_MRF_filter\")')]
                match=re.search(r'\((zero|sign)_extend:SI \(subreg:HI \(reg/v:SI \d+ \[ count \]\) 0\)\)',prefix[-2500:])
                assert match and match[1]==('sign' if label=='baseline' else 'zero')
                proof={'MRF_argument_sibling_RTL':match.group()}
            else:
                chains=frame_chains(path)
                if label=='baseline':
                    assert not chains
                else:
                    assert len(chains)>=3
                proof={'bounded_parent_frame_load_chains':chains,'general_def_use_proof':False}
            family_report[label]={'changed_bodies':changed,'all_verdicts':cell['verdicts'],'constant_pool_change':pool,
                                  'proof':proof,'metadata_data_nontext_relocations_calls_bystanders':'pass'}
            total+=1;verdicts+=len(cell['verdicts'])
        report[family]=family_report
    assert (total,verdicts)==(6,24)
    report['original']={'V21_post_debug_reloads':diagnostic_reload_rows(d.b.BLOB),
                        'unframe_bounded_load_chains':frame_chains(d.b.BLOB)}
    report['denominator']={'complete_TU_cells':total,'shared_verdicts':verdicts,'exact_gains':0,'exact_losses':0}
    (d.ROOT/'build/partial-boundary-audit.json').write_text(json.dumps(report,indent=2)+'\n')
    print('6 complete-TU cells /24 verdicts; baseline/metadata/data/calls and all collateral reviewed')
    print('V21 three known-state reloads and V23 sign-to-zero conversion controls fire; no exact gains/losses')
    print('Unframe changes only one197-byte literal pool order, preserving every literal; current-parent chains recorded')


if __name__=='__main__':main()
