#!/usr/bin/env python3
"""Audit complete AGC return/word controls, including known positive/negative cases."""
import hashlib
import json
import re
import playbook_small_patterns as d
from helper_caller_audit import inspect, allocated
from gcc3_adid_first_study_audit import masked
from gcc3_stage_divergence import chunks


def instructions(path,name='FPM_AGC_agc'):
    return d.b.insns(str(path),name)


def categories(path):
    xs=instructions(path)
    word_tests=sum(m=='test' and re.fullmatch(r'%(?:ax|bx|cx|dx|si|di|bp),%(?:ax|bx|cx|dx|si|di|bp)',o) is not None for m,o in xs)
    gate_cmps=sum(m=='cmp' and re.fullmatch(r'0x[0-9a-f]+\(%esp\),%cx',o) is not None for m,o in xs)
    pointer_clear=any(m=='movw' and re.fullmatch(r'\$0x0,\(%e[a-z]+\)',o) for m,o in xs)
    return {'word_tests':word_tests,'word_gate_comparisons':gate_cmps,'pointer_clear':pointer_clear}


def main():
    reports={};counts=[0,0]
    allowed={'fpm_agc':{'FPM_AGC_agc'},'bwchdem':{'BwChDem_Progress'},'v23rx':{'v23FP_rx_progress'},'B103prc':{'RxDetMarkB103'}}
    roots=('gcc3-v23-agc-return','gcc3-agc-word-loops','gcc3-agc-gain-words')
    for rootname in roots:
        root=d.ROOT/'build'/rootname
        ledger=json.loads((root/'results.json').read_text()); rows={}
        for family,record in ledger['families'].items():
            baseline=root/family/'baseline/candidate.o';base=inspect(baseline)
            if rootname=='gcc3-v23-agc-return':
                expected=d.ROOT/'build/v23-agc-before-objects'/(record['source_path'].replace('/','_')+'.o')
            elif rootname=='gcc3-agc-word-loops':
                expected=d.ROOT/'build/gcc3-v23-agc-return/fpm_agc/return-only/candidate.o'
            else:
                expected=d.ROOT/'build/gcc3-agc-word-loops/fpm_agc/word-3/candidate.o'
            assert baseline.read_bytes()==expected.read_bytes(),(rootname,family,'raw baseline')
            for label,cell in record['cells'].items():
                obj=root/family/label/'candidate.o';current=inspect(obj)
                assert hashlib.sha256(obj.read_bytes()).hexdigest()==cell['object_hash']
                assert current==base,(rootname,family,label,'metadata/data')
                assert allocated(obj)==allocated(baseline)
                changed=[n for n in cell['functions'] if masked(obj,n)!=masked(baseline,n)]
                assert set(changed)<=allowed.get(family,set()),(rootname,family,label,changed)
                assert not cell.get('gains') and not cell.get('losses')
                rows[family+'/'+label]={'changed_bodies':changed,'verdicts':cell['verdicts']}
                counts[0]+=1;counts[1]+=len(cell['verdicts'])
        reports[rootname]=rows
    assert counts==[76,644],counts
    root=d.ROOT/'build/gcc3-agc-word-loops/fpm_agc'
    cats={label:categories(root/label/'candidate.o') for label in ('baseline',*(f'word-{i}' for i in range(1,8)))}
    cats['original']=categories(d.b.BLOB)
    assert cats['baseline']['word_gate_comparisons']==0
    assert cats['baseline']['word_tests']==0
    for i in range(1,8):
        c=cats['word-'+str(i)]
        assert c['word_gate_comparisons']==(2 if i&2 else 0)
        assert c['pointer_clear']==bool(i&4)
        assert bool(c['word_tests'])==bool(i&1)
    assert cats['original']['word_gate_comparisons']==2 and cats['original']['pointer_clear']
    # Full-register return comes from the same register used to publish signal.
    for obj in (d.b.BLOB,root/'word-3/candidate.o'):
        xs=instructions(obj)
        assert sum(m=='ret' for m,o in xs)==1
        assert any(m=='mov' and re.fullmatch(r'%eax,0x1c\(%e[a-z]+\)',o) for m,o in xs)
    rtl_before=chunks(root/'baseline/fpm_agc.c.01.rtl','FPM_AGC_agc')[0]
    rtl_after=chunks(root/'word-3/fpm_agc.c.01.rtl','FPM_AGC_agc')[0]
    narrowing=r'\(set \(reg/v:SI \d+ \[ i \]\)\s*\(sign_extend:SI \(subreg:HI \(reg:SI \d+\) 0\)\)'
    # Initial index already narrows in both; the decrement adds a second boundary.
    assert len(re.findall(narrowing,rtl_after))==len(re.findall(narrowing,rtl_before))+1
    for name in ('acquire_level','squelch_level'):
        pattern=r'\(compare:CCGC \(subreg/s:HI \(reg/v:SI \d+ \[ level \]\) 0\)\s*\(subreg/s:HI \(reg/v:SI \d+ \[ '+name+r' \]\) 0\)\)'
        assert not re.search(pattern,rtl_before) and re.search(pattern,rtl_after)
    callroot=d.ROOT/'build/gcc3-v23-agc-return'
    for family,target,field in [('bwchdem','BwChDem_Progress','0x4c(%esi)'),('v23rx','v23FP_rx_progress','0x24(%esi)')]:
        before=instructions(callroot/family/'baseline/candidate.o',target)
        after=instructions(callroot/family/'both-direct/candidate.o',target)
        assert any(m=='mov' and o.startswith(field+',') for m,o in before)
        assert not any(m=='mov' and o.startswith(field+',') for m,o in after)
        assert any(m=='movswl' and o.startswith('%ax,') for m,o in after)
    before=instructions(callroot/'B103prc/baseline/candidate.o','RxDetMarkB103')
    after=instructions(callroot/'B103prc/both-direct/candidate.o','RxDetMarkB103')
    assert sorted(before)==sorted(after)
    differing=[i for i in range(len(before)) if before[i]!=after[i]]
    assert len(differing)==2 and differing[1]==differing[0]+1
    assert before[differing[0]]==after[differing[1]] and before[differing[1]]==after[differing[0]]
    assert all(before[i][0]=='mov' and before[i][1].endswith('(%esp)') for i in differing)
    selections={'src_dsp_fpm_agc.c.o':root/'word-3/candidate.o',
                'src_pump_v23_bwchdem.c.o':callroot/'bwchdem/both-direct/candidate.o',
                'src_pump_v23_v23rx.c.o':callroot/'v23rx/both-direct/candidate.o',
                'src_pump_b103_B103prc.c.o':callroot/'B103prc/both-direct/candidate.o'}
    for name,obj in selections.items():
        assert obj.read_bytes()==(d.ROOT/'build/tc_out'/name).read_bytes(),(name,'selected raw')
    reports['categories']=cats
    reports['initial_RTL']='Additional decrement sign extension and two HI gate comparisons precede allocation'
    reports['denominator']={'full_TU_cells':counts[0],'shared_body_verdicts':counts[1]}
    reports['selected_raw_production']=list(selections)
    reports['B103_collateral']='Only two adjacent outgoing-argument stores exchange order'
    (d.ROOT/'build/v23-agc-return-audit.json').write_text(json.dumps(reports,indent=2)+'\n')
    print('76 full-TU cells/644 shared-body verdicts: raw baselines, metadata, named/allocated data, bystanders and selected production pass')
    print('Known negative/positive word-gate, word-counter and pointer-clear controls fire; two V23 AX consumers restored; B103 only two argument stores swap')


if __name__=='__main__':
    main()
