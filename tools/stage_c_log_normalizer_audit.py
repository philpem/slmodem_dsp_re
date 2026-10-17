#!/usr/bin/env python3
"""Audit normalizer/count writeback source graphs and the remaining tail allocation."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import playbook_small_patterns as d
from gcc3_value_carriers_audit import inspect
from gcc3_reload_trace import function, instructions
from gcc3_loop_memory_trace import trace as promotion
from gcc3_output_storage_trace import trace as storage

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--run-dir', type=Path, default=Path('build/stage-c-log-normalizer'))
p.add_argument('--index-dir', type=Path, default=Path('build/stage-c-log-index'))
a = p.parse_args()
reports = {}
for root in (a.run_dir, a.index_dir):
    cells = json.loads((root/'results.json').read_text())['families']['fpm_log10']['cells']
    base = root/'fpm_log10/baseline/candidate.o'
    meta = inspect(base)
    assert cells['baseline']['baseline_reproduced']
    for label, cell in cells.items():
        path = root/'fpm_log10'/label/'candidate.o'
        data = inspect(path)
        for key in ('records','allocated','nobits','relocations'): assert data[key] == meta[key], (label,key)
        assert len(d.b.sizes(str(path))) == 5
        assert set(cell.get('changed_bodies', [])) <= {'FPM_log10'}
        assert not cell.get('gains') and not cell.get('losses')
        for name in d.b.sizes(str(base)):
            if name != 'FPM_log10': assert d.b.body(str(path),name) == d.b.body(str(base),name)
        before, after = d.b.body(str(base),'FPM_log10'), d.b.body(str(path),'FPM_log10')
        assert Counter(before[1].values()) == Counter(after[1].values())
        reports[str(root.name)+'/'+label] = {'functions':5,'verdict':cell['verdicts']['FPM_log10'],
            'size':d.b.sizes(str(path))['FPM_log10'],'object_hash':cell['object_hash'],
            'four_bystanders_data_bindings_nontext_relocations_equal':True}

family = a.run_dir/'fpm_log10'
assert (family/'baseline/candidate.o').read_bytes() == (family/'pointed0-writeback1/candidate.o').read_bytes()
assert (a.index_dir/'fpm_log10/pointed-writeback/candidate.o').read_bytes() == (a.index_dir/'fpm_log10/word-index/candidate.o').read_bytes()
chosen = family/'pointed1-writeback1'
prefix = chosen/'fpm_log10'
loop = promotion(prefix.with_suffix('.c.09.loop').read_text(),'FPM_log10')
promotions = [v for report in loop['reports'] for v in report['promotions']]
assert len(promotions) == 1 and promotions[0]['mode'] == 'HI' and promotions[0]['access'] == 'r/w'
assert promotions[0]['store_uids'] == [140] and promotions[0]['store_loop_depths'] == {140:0}
homes = storage(prefix.with_suffix('.c.01.rtl').read_text(),prefix.with_suffix('.c.35.mach').read_text(),prefix.with_suffix('.s').read_text(),'FPM_log10')
assert {v['role']:v['offset'] for v in homes['output_homes']} == {'mantissa':-2,'count':-4}
assert homes['HI_stack_loads'] == [{'uid':79,'rtl_mode':'HI','stack_offset':12,'offset_mod4':0,'mnemonic':'movl','destination':'%edx','pattern':'*movhi_1/3'}]
trace = {}
for stage in ('01.rtl','09.loop','19.life','20.combine','21.ce2','22.regmove','24.lreg','25.greg','27.flow2','28.peephole2','30.rnreg','33.sched2'):
    stream = instructions(function(prefix.with_suffix('.c.'+stage).read_text(),'FPM_log10'))
    trace[stage] = {str(uid):stream[uid] for uid in (44,45,49,64,74,79,80,81,89,95,96,97,99,100,103,104,112,140) if uid in stream}
assert 'mem/f:HI' in str(trace['01.rtl']['81']) and '-4' in str(trace['01.rtl']['81'])
assert trace['21.ce2']['103'][2][1][1] == '93'
assert trace['22.regmove']['103'][2][1][1] == '86'
assert trace['25.greg']['103'][2][1][1] == '1'  # EDX result, first concrete binding.
assert trace['25.greg']['104'][1][1] == '0' and trace['25.greg']['104'][2][1][1] == '1'
assert trace['27.flow2']['104'] == trace['28.peephole2']['104'] == trace['30.rnreg']['104'] == trace['33.sched2']['104']
candidate_raw = d.b.body(str(chosen/'candidate.o'),'FPM_log10')[0]
original_raw = d.b.body(d.b.BLOB,'FPM_log10')[0]
assert candidate_raw[0x40:0x76] == original_raw[0x40:0x76]
production = Path('build/period/src_dsp_fpm_log10.o')
assert all(d.b.body(str(chosen/'candidate.o'),name) == d.b.body(str(production),name) for name in d.b.sizes(str(chosen/'candidate.o')))
report = {'cells':7,'emitted_body_comparisons':35,'shared_original_verdicts':7,'reports':reports,
          'promotion':loop,'homes_and_machine_selection':homes,'stage_trace':trace,
          'production_period_object_matches_retained_experiment':True,
          'period':'3 passed,0 failed (log10,sqrt,div)',
          'original_normalization_segment_raw_equal':{'offset':64,'bytes':54},
          'original_witness':'a8d60 HI count zero; a8d90/a8d93 count/mantissa outputs; a8d96 aligned MOVL count; a8da5 adjusted-count HI writeback before lookup; homes stack+c/+e.',
          'negative_controls':'Scalar count reuse alone is byte-identical to baseline. HI idx local is byte-identical to retained SI idx spelling; neither establishes a gain.',
          'residual':'Retained205B/original203B SIZE2 is multiple operand/scheduling differences, not two differing bytes. Original a8dd6 subtracts inEAX then a8dd8 CWDE(1B); retained UID103 inEDX and UID104 MOVSWL DX,EAX(3B). Current21.ce2 subtraction93 is coalesced to table-value86 by22.regmove;25.greg binds86EDX and coefficient90EAX, preserved by peephole2/rnreg. Original RTL unavailable; do not claim its first divergent compiler pass.',
          'next_discriminator':'Close home order and idx-width domains: homes already exact and idx control identical. Reopen only with independently witnessed table-result/coefficient owner or definition-order evidence that changes the subtraction interference graph. Do not repeat closed result/return palettes or force EAX.'}
(a.run_dir/'audit.json').write_text(json.dumps(report,indent=2)+'\n')
print('7 full-TU cells/35 bodies: only log10 changes; all4 helpers,data,bindings and relocation targets preserved')
print('Pointed countHI promotion store140 outside loop; same-home update81; stack+c/+e restored; period3/0')
print('Scalar reuse and HI idx controls identical; residual regmove86 -> gregEDX, MOVSWL return remains')
