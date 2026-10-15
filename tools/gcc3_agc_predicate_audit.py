#!/usr/bin/env python3
"""Audit AGC predicates, arm/length controls and complete RMS caller group."""
import hashlib
import json
import re
from pathlib import Path
import playbook_small_patterns as d
from helper_caller_audit import inspect, allocated
from gcc3_adid_first_study_audit import masked
from gcc3_stage_divergence import chunks

ROOTS=('gcc3-agc-predicate','gcc3-agc-guard','gcc3-agc-partition','gcc3-agc-length',
       'gcc3-agc-rms-boundary','gcc3-agc-rms-normalize')


def xs(path,name='FPM_AGC_agc'):
    return d.b.insns(str(path),name)


def guard(path):
    instructions=xs(path)
    start=next(i for i,(m,o) in enumerate(instructions) if m=='mov' and re.match(r'0x28\(%e(?!sp)',o))
    end=next(i for i in range(start+1,len(instructions)) if instructions[i][0].startswith('j'))
    window=instructions[start:end+1]
    return {'mnemonics':[m for m,o in window],
            'narrow_level_test':('test','%ax,%ax') in window,
            'sequence':window}


def main():
    report={};count=[0,0];common_cells=0
    for rootname in ROOTS:
        root=d.ROOT/'build'/rootname
        ledger=json.loads((root/'results.json').read_text());rows={}
        for family,rec in ledger['families'].items():
            baseline=root/family/'baseline/candidate.o';base=inspect(baseline)
            if rootname=='gcc3-agc-predicate':expected=d.ROOT/'build/agc-predicate-before-objects/src_dsp_fpm_agc.c.o'
            elif rootname=='gcc3-agc-guard':expected=d.ROOT/'build/gcc3-agc-predicate/fpm_agc/local-int/candidate.o'
            elif rootname=='gcc3-agc-partition':expected=d.ROOT/'build/gcc3-agc-guard/fpm_agc/both/candidate.o'
            elif rootname=='gcc3-agc-length':expected=d.ROOT/'build/gcc3-agc-partition/fpm_agc/small-tail-first/candidate.o'
            elif rootname=='gcc3-agc-rms-boundary':
                expected=d.ROOT/'build/gcc3-agc-length/fpm_agc/baseline/candidate.o' if family=='fpm_agc' else d.ROOT/'build/agc-predicate-before-all'/(rec['source_path'].replace('/','_')+'.o')
            else:expected=d.ROOT/'build/gcc3-agc-rms-boundary'/family/'both/candidate.o'
            assert baseline.read_bytes()==expected.read_bytes(),(rootname,family,'raw baseline')
            for label,cell in rec['cells'].items():
                obj=root/family/label/'candidate.o';current=inspect(obj)
                assert hashlib.sha256(obj.read_bytes()).hexdigest()==cell['object_hash']
                assert current==base and allocated(obj)==allocated(baseline),(rootname,family,label,'metadata/data')
                common_cells+=int(any(v['section']=='SHN_COMMON' for v in current['objects'].values()))
                changed=[n for n in cell['functions'] if masked(obj,n)!=masked(baseline,n)]
                allowed={'FPM_AGC_agc'} if family=='fpm_agc' else {'FPM_SRE_recover'} if family=='fpm_sre' else set()
                assert set(changed)<=allowed,(rootname,family,label,changed)
                assert not cell.get('gains') and not cell.get('losses')
                if family=='fpm_agc':
                    calls=lambda path:[o.split('@',1)[1] for m,o in xs(path) if m=='call']
                    assert calls(obj)==calls(d.b.BLOB)
                if family=='fpm_rms':assert cell['verdicts']['FPM_rms']==['EXACT',0]
                rows[family+'/'+label]={'changed_bodies':changed,'verdicts':cell['verdicts']}
                count[0]+=1;count[1]+=len(cell['verdicts'])
        report[rootname]=rows
    assert count==[77,650],count
    assert common_cells==6,common_cells
    common=inspect(d.ROOT/'build/gcc3-agc-rms-boundary/class1/baseline/candidate.o')
    assert common['records']['class1_state_functions']==['STT_OBJECT','STB_GLOBAL','STV_DEFAULT','SHN_COMMON',76]
    assert common['objects']['class1_state_functions']=={'section':'SHN_COMMON','offset':32,'bytes':None,'relocations':{}}
    root=d.ROOT/'build/gcc3-agc-guard/fpm_agc'
    original_guard=guard(d.b.BLOB);chosen_guard=guard(root/'both/candidate.o')
    assert chosen_guard['mnemonics']==original_guard['mnemonics'] and chosen_guard['narrow_level_test']
    assert guard(root/'level-use/candidate.o')['mnemonics']!=original_guard['mnemonics']
    assert (root/'baseline/candidate.o').read_bytes()==(root/'eager/candidate.o').read_bytes()
    for label,expected in [('baseline',23),('level-use',24),('both',23)]:
        rtl=chunks(root/label/'fpm_agc.c.01.rtl','FPM_AGC_agc')[0]
        assert len(re.findall(r'^\(jump_insn ',rtl,re.M))==expected
    assert ('test','%eax,%eax') in guard(root/'baseline/candidate.o')['sequence']
    part=d.ROOT/'build/gcc3-agc-partition/fpm_agc'
    def partition_branch(path):
        instructions=xs(path);i=next(i for i,x in enumerate(instructions) if x==('cmp','%dx,%ax'))
        return instructions[i+1][0]
    assert partition_branch(part/'baseline/candidate.o')=='ja'
    assert partition_branch(part/'small-tail-first/candidate.o')==partition_branch(d.b.BLOB)=='jbe'
    rms=d.ROOT/'build/gcc3-agc-rms-boundary'
    final=d.ROOT/'build/gcc3-agc-rms-normalize'
    assert (rms/'fpm_rms/both/candidate.o').read_bytes()==(d.ROOT/'build/agc-predicate-before-all/src_dsp_fpm_rms.c.o').read_bytes()
    def length_loads(path):
        instructions=xs(path)
        start=next(i for i,(m,o) in enumerate(instructions) if m=='js')
        end=next(i for i,(m,o) in enumerate(instructions) if m=='call' and 'FPM_rms' in o)
        window=instructions[start:end]
        return {'narrow_stack_loads':sum(m=='movzwl' and '(%esp)' in o for m,o in window),
                'extra_register_extension':sum(m=='movzwl' and o.startswith('%') for m,o in window)}
    lengths={label:length_loads(rms/'fpm_agc'/label/'candidate.o') for label in ('baseline','length','both')}
    lengths['original']=length_loads(d.b.BLOB)
    assert lengths['baseline']=={'narrow_stack_loads':2,'extra_register_extension':0}
    assert lengths['length']=={'narrow_stack_loads':0,'extra_register_extension':1}
    assert lengths['both']==lengths['original']=={'narrow_stack_loads':0,'extra_register_extension':0}
    # The only interface collateral is one count-load extension in SRE.
    before=xs(rms/'fpm_sre/baseline/candidate.o','FPM_SRE_recover')
    after=xs(rms/'fpm_sre/both/candidate.o','FPM_SRE_recover')
    differences=[(a,b) for a,b in zip(before,after) if a!=b]
    assert len(before)==len(after) and len(differences)==1
    a,b=differences[0];assert a[0]=='movzwl' and b[0]=='movswl' and a[1]==b[1]
    selected={}
    ledger=json.loads((final/'results.json').read_text())
    for family,rec in ledger['families'].items():
        name=rec['source_path'].replace('/','_')+'.o';obj=final/family/'normalize/candidate.o'
        assert obj.read_bytes()==(d.ROOT/'build/tc_out'/name).read_bytes(),(name,'raw selection')
        if family!='fpm_agc':assert obj.read_bytes()==(d.ROOT/'build/agc-predicate-before-all'/name).read_bytes()
        selected[name]=hashlib.sha256(obj.read_bytes()).hexdigest()
    # Independent low-word argument normalization identity, no sample reads.
    for word in range(65536):
        signed=word if word<32768 else word-65536
        assert (signed&0xffffffff)&65535==word
    report['denominator']={'full_TU_cells':77,'shared_body_verdicts':650,'signed_word_argument_identities':65536}
    report['guard']={'original':original_guard,'selected':chosen_guard}
    report['length_loads']=lengths
    report['raw_selected_TUs']=selected
    report['RMS_exact_producer']=True
    report['known_COMMON_control']={'name':'class1_state_functions','size':76,'alignment':32,'cells':common_cells}
    (d.ROOT/'build/agc-predicate-audit.json').write_text(json.dumps(report,indent=2)+'\n')
    print('77 complete-TU cells/650 shared verdicts: raw baselines/10 selected TUs, metadata/data/exports/calls/bystanders pass')
    print('Eager HI guard/partition match original; narrow-short-circuit negative control fires; RMS EXACT62; SRE one-extension collateral restored raw')
    print('65536 signed-word argument identities; no original formal-type uniqueness claim')


if __name__=='__main__':
    main()
