#!/usr/bin/env python3
"""Audit the nine bounded echo append cells and their raw-null exit controls."""
import json
from pathlib import Path
import playbook_small_patterns as d
from gcc3_value_carriers_audit import inspect
from gcc3_stage_divergence import chunks,fingerprint
from gcc3_reload_trace import instructions

TARGET='_ZN16V92EchoCanceller17updateEchoHistoryEPfj'
HEADER='void V92EchoCanceller::updateEchoHistory(float*, unsigned int)'


def main():
    rows=[];verdicts=0
    for package in ('residual-echo-append','residual-echo-append-exit'):
        root=d.ROOT/'build'/package/'V92EchoCanceller'
        family=json.loads((root.parent/'results.json').read_text())['families']['V92EchoCanceller']
        baseline=root/'baseline/candidate.o';meta=inspect(baseline)
        assert family['cells']['baseline']['baseline_reproduced']
        for label,entry in family['cells'].items():
            obj=root/label/'candidate.o';actual=inspect(obj)
            assert all(actual[k]==meta[k] for k in meta if k!='text_positions')
            changed=[n for n in entry['functions'] if d.b.body(str(obj),n)!=d.b.body(str(baseline),n)]
            assert set(changed)<={TARGET} and not entry.get('gains') and not entry.get('losses')
            stages={}
            for stage in ('01.rtl','09.loop','24.lreg','25.greg','27.flow2','28.peephole2','30.rnreg','31.bbro','33.sched2'):
                selected=chunks(root/label/('V92EchoCanceller.cpp.'+stage),HEADER)
                assert len(selected)==1
                stages[stage]=len(instructions(selected[0]))
            rows.append({'package':package,'cell':label,'bytes':d.b.sizes(str(obj))[TARGET],
                         'verdict':entry['verdicts'][TARGET],'stage_instruction_counts':stages,
                         'unchanged_bystanders':len(entry['functions'])-1,'metadata_nontext_equal':True})
            verdicts+=len(entry['verdicts'])
    first=d.ROOT/'build/residual-echo-append/V92EchoCanceller'
    second=d.ROOT/'build/residual-echo-append-exit/V92EchoCanceller'
    assert (first/'baseline/candidate.o').read_bytes()==(second/'baseline/candidate.o').read_bytes()
    assert (first/'cursor-1-increment-1/candidate.o').read_bytes()==(second/'combined-repeat/candidate.o').read_bytes()
    assert (second/'combined-repeat/candidate.o').read_bytes()==(second/'shared-1-late-0/candidate.o').read_bytes()
    assert (second/'shared-0-late-1/candidate.o').read_bytes()==(second/'shared-1-late-1/candidate.o').read_bytes()
    assert len(rows)==9 and verdicts==135
    report={'cells':rows,'common_body_verdicts':verdicts,'strict_gains':0,'exact_losses':0,
            'raw_shared_exit_nulls':2,'source_adopted':False}
    (d.ROOT/'build/residual-echo-append-audit.json').write_text(json.dumps(report,indent=2)+'\n')
    for row in rows:print(row['package'],row['cell'],row['bytes'],row['verdict'])
    print('9 full-TU cells / 135 verdicts; all 14 bystanders and metadata preserved; no adoption')


if __name__=='__main__':main()
