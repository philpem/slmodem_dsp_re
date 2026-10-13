#!/usr/bin/env python3
"""Audit shift ownership and bounded counts without executing invalid C shifts."""
import hashlib
import json
import re
import playbook_small_patterns as d
from helper_caller_audit import inspect, allocated
from gcc3_adid_first_study_audit import masked
from gcc3_stage_divergence import chunks


def main():
    root=d.ROOT/'build/gcc3-agc-shift'
    cells=json.loads((root/'results.json').read_text())['families']['fpm_agc']['cells']
    basepath=root/'fpm_agc/baseline/candidate.o';base=inspect(basepath)
    assert basepath.read_bytes()==(d.ROOT/'build/agc-shift-before-objects/src_dsp_fpm_agc.c.o').read_bytes()
    rows={};verdicts=0
    for label,cell in cells.items():
        obj=root/'fpm_agc'/label/'candidate.o'
        assert hashlib.sha256(obj.read_bytes()).hexdigest()==cell['object_hash']
        assert inspect(obj)==base and allocated(obj)==allocated(basepath)
        changed=[n for n in cell['functions'] if masked(obj,n)!=masked(basepath,n)]
        assert set(changed)<={'FPM_AGC_agc'}
        assert not cell.get('gains') and not cell.get('losses')
        xs=d.b.insns(str(obj),'FPM_AGC_agc')
        row={'verdicts':cell['verdicts'],'changed_bodies':changed,
             'mask31':sum(m=='and' and '$0x1f,' in o for m,o in xs),
             'stack_byte_shift_reload':sum(m=='movzbl' and re.fullmatch(r'0x[0-9a-f]+\(%esp\),%ecx',o) is not None for m,o in xs),
             'calls':[o.split('@',1)[1] for m,o in xs if m=='call']}
        rtl=chunks(obj.parent/'fpm_agc.c.01.rtl','FPM_AGC_agc')[0]
        row['initial_shift_uses_cached_count']=bool(re.search(r'\(ashift:SI \(reg/v:SI \d+ \[ v \]\)\s*\(subreg:QI \(reg/v:SI \d+ \[ shift \]\) 0\)',rtl))
        greg=chunks(obj.parent/'fpm_agc.c.25.greg','FPM_AGC_agc')[0]
        row['reload_QI_from_shift_home']=bool(re.search(r'\(set \(reg:QI 2 cx\)\s*\(mem:QI .*?shift\+0 S1 A8',greg,re.S))
        rows[label]=row;verdicts+=len(cell['verdicts'])
    assert verdicts==16
    assert rows['baseline']['mask31']==rows['byte-mask']['mask31']==1
    assert rows['raw']['mask31']==rows['byte']['mask31']==0
    assert rows['raw']['stack_byte_shift_reload']==1
    assert not rows['byte']['stack_byte_shift_reload']
    assert rows['raw']['initial_shift_uses_cached_count'] and rows['raw']['reload_QI_from_shift_home']
    original=d.b.insns(d.b.BLOB,'FPM_AGC_agc')
    assert sum(m=='movzbl' and re.fullmatch(r'0x[0-9a-f]+\(%esp\),%ecx',o) is not None for m,o in original)==1
    original_calls=[o.split('@',1)[1] for m,o in original if m=='call']
    assert all(row['calls']==original_calls for row in rows.values())
    # Count identities, not evaluations of C shifts with invalid counts.
    count_values=range(-32768,32768)
    assert all((s&31)==((s&255)&31) for s in count_values)
    wrong_mask=sum((s&31)!=(s&15) for s in count_values)
    assert wrong_mask==32768
    norms={};positive_shifts=set()
    for denom in range(1,65536):
        mantissa=denom;n=0
        while mantissa<32768:
            mantissa*=2;n+=1
        assert 32768<=mantissa<=65535
        norms[n]=norms.get(n,0)+1
        if denom<32768:positive_shifts.add(n-1)
    assert positive_shifts==set(range(15)) and sum(norms.values())==65535
    # Monotonic nonnegative coefficients bound every input pair by the corner.
    level_max=32703
    for alpha in range(32769):
        beta=32768-alpha
        assert ((alpha*level_max)>>15)+((beta*level_max)>>15)<=level_max
    assert (root/'fpm_agc/raw/candidate.o').read_bytes()==(d.ROOT/'build/tc_out/src_dsp_fpm_agc.c.o').read_bytes()
    report={'cells':rows,'denominator':{'full_TU_cells':4,'shared_body_verdicts':16,
            'signed_word_counts':65536,'nonzero_denominators':65535,'unity_pair_corner_bounds':32769},
            'positive_level_shift_range':[0,14],'wrong_mask_control_differences':wrong_mask,
            'norm_histogram':norms,'source_limits':'No universal caller/config/state range; invalid counts are period machine behavior, not defined C shifts'}
    (d.ROOT/'build/agc-shift-audit.json').write_text(json.dumps(report,indent=2)+'\n')
    print('4 full-TU cells/16 shared verdicts: raw baseline/selection, metadata/data/calls/bystanders pass')
    print('Raw cached int -> QI reload -> stack MOVZBL; byte-cast negative control does not reproduce that load')
    print('65536 count identities, 65535 nonzero normalizations, 32769 unity-pair corner bounds; wrong-mask control differs32768 times; initialized range0..14')


if __name__=='__main__':
    main()
