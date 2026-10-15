#!/usr/bin/env python3
"""Audit complete-TU boundary transfer controls, including rejected collateral."""
import json,re
from pathlib import Path
from collections import Counter
import playbook_small_patterns as d
from gcc3_value_carriers_audit import inspect,function_chunk
from helper_caller_audit import inspect as named
from gcc3_adid_first_study_audit import masked
from narrow_call_boundary_screen import bodies,sha

TARGETS={'V32int':'DemodDataV32','B103prc':'DemodDataB103','V17r_int':'DemodDataV17',
 'V21r_int':'DemodDataV21','V27r_int':'DemodDataV27','V29r_int':'DemodDataV29',
 'Rxcid':'cid_modem','V21t_prc':'TxNextStateV21','V27t_prc':'GenEQTrnSequenceV27',
 'V22int':'DemodDataV22','v22_mrf':'V22_MRF_filter','fpm_tone':'FPM_TONE_find_rev',
 'FloatIIR':'_ZN10GenericIIRIfdE7processEf'}


def v21_destination(path):
    rows=bodies(path)['DemodDataV21']
    i=next(i for i,r in enumerate(rows)if r.get('target')=='FPM_AGC_agc')
    window=rows[i+1:i+8]
    loads=[];stores=[]
    for j,row in enumerate(window):
        if row['mnemonic'].startswith(('call','j','ret')):
            break
        if (re.search(r'%(?:eax|ax|al|ah)$',row['operands'])
                and not row['mnemonic'].startswith(('cmp','test','push'))):
            break
        if row['mnemonic']=='mov' and re.fullmatch(r'0x50\(%\w+\),%\w+',row['operands']):loads.append(j)
        if row['mnemonic']=='mov' and re.fullmatch(r'%eax,0x4\(%\w+\)',row['operands']):stores.append(j)
    return {'post_call_child_before_EAX_store':bool(loads and stores and loads[0]<stores[0]),'window':window}


def sibling_destination_age(path):
    nodes=function_chunk(path,'DemodDataV21').split('\n\n')
    call=next(i for i,n in enumerate(nodes)if '("FPM_AGC_agc")' in n)
    store=next(i for i,n in enumerate(nodes)if '.int_0004+0' in n and '(set (mem/' in n)
    match=re.search(r'\(plus:SI \(reg/f:SI (\d+)\)',nodes[store]);assert match
    prefix='(set (reg/f:SI '+match[1]+')'
    load=max(i for i,n in enumerate(nodes[:store])if prefix in n and '.dsp+0' in n)
    return {'load_node':load,'call_node':call,'store_node':store,'destination_after_call':load>call,
            'load':nodes[load],'call':nodes[call],'store':nodes[store]}


def tx_reloads(path):
    rows=bodies(path)['TxNextStateV21']
    return [rows[i+1]for i,r in enumerate(rows)if r.get('target')=='dsplibs_debug_printf' and i+1<len(rows)
            and rows[i+1]['mnemonic']=='mov' and re.fullmatch(r'0x20\(%\w+\),%\w+',rows[i+1]['operands'])]


def acc_publications(path):
    return [(m,o)for m,o in d.b.insns(str(path),TARGETS['FloatIIR'])
            if m=='fstl' and o.startswith('0x2c(')]


def energy_updates(path):
    rows=d.b.insns(str(path),'FPM_TONE_find_rev')
    return [rows[i:i+2]for i in range(len(rows)-1)
            if rows[i][0]=='add' and rows[i+1][0]=='sub'
            and rows[i][1].split(',')[-1]==rows[i+1][1].split(',')[-1]]


def main():
    report={};cells=verdicts=emitted=0
    assert v21_destination(d.b.BLOB)['post_call_child_before_EAX_store']
    assert len(tx_reloads(d.b.BLOB))==3
    assert len(acc_publications(d.b.BLOB))==4
    assert len(energy_updates(d.b.BLOB))==1
    for package in ['gcc3-boundary-transfer','gcc3-v22-boundary-transfer','gcc3-publication-partial-review']:
        root=d.ROOT/'build'/package
        ledger=json.loads((root/'results.json').read_text())
        for family,f in ledger['families'].items():
            fr=root/family;basepath=fr/'baseline/candidate.o';base=inspect(basepath);bn=named(basepath)
            assert basepath.read_bytes()==(fr/'retained.o').read_bytes()
            rows={}
            for label,cell in f['cells'].items():
                path=fr/label/'candidate.o';now=inspect(path)
                assert sha(path)==cell['object_hash'] and sha(path.parent/Path(f['source_path']).name)==cell['source_hash']
                assert now['records']==base['records'] and now['nobits']==base['nobits']
                assert now['relocations']==base['relocations'] and named(path)['objects']==bn['objects']
                diff={k:[base['allocated'].get(k),now['allocated'].get(k)]for k in set(base['allocated'])|set(now['allocated'])if base['allocated'].get(k)!=now['allocated'].get(k)}
                if family=='FloatIIR':
                    assert set(diff)<={'.rodata.cst8'}
                    assert all(v==0 for pair in diff.values()for h in pair if h for v in bytes.fromhex(h))
                else:assert not diff,(package,family,label,diff)
                changed=[n for n in cell['functions']if masked(path,n)!=masked(basepath,n)]
                allowed={TARGETS[family]}
                if family=='V32int':allowed|={'SetAdaptEcV32','SetAdaptEqV32','SetECRndTripDelayV32'}
                if family=='V21t_prc':allowed|={'TxHdxDataV21','TxHdxIdleV21','TxHdxStartV21'}
                assert set(changed)<=allowed,(family,label,changed)
                for n in changed:
                    edges=lambda p:Counter(r['target']for r in bodies(p)[n]if 'target'in r)
                    assert edges(path)==edges(basepath),(family,label,n,'calls')
                    if n!=TARGETS[family]:
                        assert d.function((path.parent/Path(f['source_path']).name).read_text(),n)[2]==d.function((basepath.parent/Path(f['source_path']).name).read_text(),n)[2]
                gains=cell.get('gains',[]);losses=cell.get('losses',[])
                assert gains==(['DemodDataV21']if family=='V21r_int' and label=='count-result-cache'else [])
                assert losses==(['SetAdaptEqV32']if family=='V32int' and label=='count'else [])
                proof={}
                if family=='V21r_int':
                    proof=v21_destination(path)
                    assert proof['post_call_child_before_EAX_store']==label.endswith('cache')
                    early=sibling_destination_age(path.parent/'V21r_int.c.02.sibling')
                    assert early['destination_after_call']==(label not in ('result','count-result'))
                    proof['sibling_RTL_destination_age']=early
                if family=='V21t_prc':
                    proof={'known_state_post_debug_reloads':tx_reloads(path)}
                    assert len(proof['known_state_post_debug_reloads'])==(0 if label=='baseline' else 3)
                if family=='v22_mrf':assert path.read_bytes()==basepath.read_bytes()
                if family=='FloatIIR':
                    rtl=function_chunk(path.parent/'FloatIIR.cpp.01.rtl','::process(Sample)')
                    proof={'initial_RTL_member_references':rtl.count('.m_acc+0'),'native_acc_publications':acc_publications(path)}
                    assert len(proof['native_acc_publications'])==(2 if label=='baseline' else 4)
                if family=='fpm_tone':
                    proof={'native_separate_energy_updates':energy_updates(path)}
                    assert len(proof['native_separate_energy_updates'])==(0 if label=='baseline' else 1)
                rows[label]={'verdicts':cell['verdicts'],'changed_bodies':changed,'gains':gains,'losses':losses,
                             'zero_constant_pool_delta':diff,'proof':proof,'metadata_named_data_nontext_calls':'pass'}
                cells+=1;verdicts+=len(cell['verdicts']);emitted+=len(cell['functions'])
            if family=='FloatIIR':assert rows['member-acc']['proof']['initial_RTL_member_references']>rows['baseline']['proof']['initial_RTL_member_references']
            report[package+'/'+family]=rows
    assert cells==46,cells
    report['denominator']={'complete_TU_cells':cells,'shared_verdicts':verdicts,'emitted_body_comparisons':emitted,
                           'experimental_exact_gain_cells':1,'experimental_exact_loss_cells':1}
    report['original_V21_destination']=v21_destination(d.b.BLOB)
    (d.ROOT/'build/boundary-transfer-audit.json').write_text(json.dumps(report,indent=2)+'\n')
    print(cells,'complete-TU cells /',verdicts,'shared verdicts /',emitted,'emitted body comparisons')
    print('V21 exact gain and V32 collateral exact loss explicitly detected; V21 direct/cached destinations distinguish pointer age')
    print('All raw baselines, metadata/named data/nontext/calls pass; only numeric-zero IIR pool changes')


if __name__=='__main__':main()
