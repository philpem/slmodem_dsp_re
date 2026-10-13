#!/usr/bin/env python3
"""Audit AGC shared-arm controls and trace saved RTL/CFG/layout evidence."""
import hashlib
import json
import re
import playbook_small_patterns as d
from helper_caller_audit import inspect, allocated
from gcc3_adid_first_study_audit import masked
from gcc3_stage_divergence import chunks
from bbalign import stream


def shape(path):
    xs=d.b.insns(str(path),'FPM_AGC_agc')
    quiet=next(i for i,(m,o) in enumerate(xs) if m=='movw' and o.startswith('$0x0,'))
    gain=next(i for i,(m,o) in enumerate(xs) if m=='movw' and o.startswith('$0x7fff,'))
    ret=next(i for i,(m,o) in enumerate(xs) if m=='ret')
    rms=next(i for i,(m,o) in enumerate(xs) if m=='call' and 'FPM_rms' in o)
    gate_words=sum(m=='cmp' and re.fullmatch(r'0x[0-9a-f]+\(%esp\),%cx',o) is not None for m,o in xs[rms:quiet])
    _,rows=stream(str(path),'FPM_AGC_agc')
    clear=next(i for i,r in enumerate(rows) if r['mn']=='movw' and r['ops'].startswith('$0x0,'))
    clear_branch=next(i for i in range(clear+1,len(rows)) if rows[i]['mn'].startswith('j'))
    common=rows[clear_branch+1]['addr']
    saturation=next(i for i,r in enumerate(rows) if r['mn']=='movw' and r['ops'].startswith('$0x7fff,'))
    exits=[r for r in rows[saturation+1:] if r['mn'] in ('je','jle')]
    common_exit=any(int(r['ops'],16)==common for r in exits)
    return {'instructions':len(xs),'gate_words_before_quiet':gate_words,
            'quiet_before_return_before_gain':quiet<ret<gain,
            'pointer_clear':bool(re.fullmatch(r'\$0x0,\(%e[a-z]+\)',xs[quiet][1])),
            'gain_exit_to_quiet_common_advance':common_exit,
            'calls':[o.split('@',1)[1] for m,o in xs if m=='call'],
            'returns':sum(m=='ret' for m,o in xs)}


def main():
    root=d.ROOT/'build/gcc3-agc-arm-ownership'
    ledger=json.loads((root/'results.json').read_text())['families']['fpm_agc']
    baseline=root/'fpm_agc/baseline/candidate.o';base=inspect(baseline)
    assert baseline.read_bytes()==(d.ROOT/'build/agc-arm-before-objects/src_dsp_fpm_agc.c.o').read_bytes()
    reports={};verdicts=0
    for label,cell in ledger['cells'].items():
        obj=root/'fpm_agc'/label/'candidate.o'
        assert hashlib.sha256(obj.read_bytes()).hexdigest()==cell['object_hash']
        assert inspect(obj)==base and allocated(obj)==allocated(baseline)
        changed=[n for n in cell['functions'] if masked(obj,n)!=masked(baseline,n)]
        assert set(changed)<= {'FPM_AGC_agc'}
        assert not cell.get('gains') and not cell.get('losses')
        row=shape(obj);row['verdicts']=cell['verdicts'];row['changed_bodies']=changed
        rtl=chunks(obj.parent/'fpm_agc.c.01.rtl','FPM_AGC_agc')[0]
        row['initial_RTL_sample_advances']=len(re.findall(r'\(set \(reg/v/f:SI \d+ \[ samples \]\)\s*\(plus:SI',rtl))
        row['stages']={}
        for stage in ('11.cfg','31.bbro'):
            text=chunks(obj.parent/('fpm_agc.c.'+stage),'FPM_AGC_agc')[0]
            match=re.search(r'^(\d+) basic blocks, (\d+) edges\.',text,re.M)
            assert match
            row['stages'][stage]={'blocks':int(match[1]),'edges':int(match[2]),'duplicated_blocks':re.findall(r'^Duplicated bb .*$',text,re.M)}
        reports[label]=row;verdicts+=len(cell['verdicts'])
    reports['original']=shape(d.b.BLOB)
    for label in ('baseline','pointer'):
        assert reports[label]['initial_RTL_sample_advances']==2
        assert reports[label]['gate_words_before_quiet']==1
    for label in ('arm','both'):
        assert reports[label]['initial_RTL_sample_advances']==1
        assert reports[label]['gate_words_before_quiet']==2
    for label in ('original','both'):
        row=reports[label]
        assert row['quiet_before_return_before_gain'] and row['pointer_clear']
        assert row['gain_exit_to_quiet_common_advance'] and row['returns']==1
    assert not reports['arm']['quiet_before_return_before_gain']
    assert len(reports['arm']['stages']['31.bbro']['duplicated_blocks'])==3
    assert len(reports['both']['stages']['31.bbro']['duplicated_blocks'])==1
    for row in reports.values():
        assert row['calls']==reports['original']['calls']
    assert verdicts==16
    assert (root/'fpm_agc/both/candidate.o').read_bytes()==(d.ROOT/'build/tc_out/src_dsp_fpm_agc.c.o').read_bytes()
    reports['denominator']={'full_TU_cells':4,'shared_body_verdicts':16}
    (d.ROOT/'build/agc-arm-audit.json').write_text(json.dumps(reports,indent=2)+'\n')
    print('4 full-TU cells/16 shared-body verdicts: raw baseline/selection, metadata/data/bystanders/calls preserved')
    print('Known negative and positive arm/pointer graph controls fire; initial RTL common advance; indexed bbro 3 duplicates versus pointer 1')


if __name__=='__main__':
    main()
