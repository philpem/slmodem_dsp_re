#!/usr/bin/env python3
"""Read the two raw-preserving historical constructor dump controls."""
import json,re
from pathlib import Path
import playbook_small_patterns as d
from gcc3_reload_trace import instructions
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/v90-parameters-ratchet-ab'

def normalize(rows):
    return re.sub(r'0x[0-9a-f]{7,}', '<tree-address>',json.dumps(rows,sort_keys=True))

def constructors(path):
    chunks=[c for c in path.read_text().split(';; Function ') if c.startswith('V90Parameters::V90Parameters(')]
    assert len(chunks)==2
    return [instructions(c) for c in chunks]

def main():
    pre=OUT/'pre-434-retype';post=OUT/'post-434-retype'
    for cell in (pre,post):assert (cell/'candidate.o').read_bytes()==(cell/'raw-before-dumps.o').read_bytes()
    report=[]
    for path in sorted(pre.glob('V90Parameters.cpp.[0-9][0-9].*')):
        stage=path.name.split('.cpp.')[1]
        if stage in ('00.cgraph','08.gcse'):continue
        a=constructors(path);b=constructors(post/path.name)
        equal=[normalize(x)==normalize(y) for x,y in zip(a,b)]
        report.append({'stage':stage,'equal_constructor_patterns':equal})
    first=next(r['stage'] for r in report if not r['equal_constructor_patterns'][0])
    assert first=='28.peephole2'
    assert all(all(r['equal_constructor_patterns']) for r in report if int(r['stage'][:2])<28)
    for cell in (pre,post):
        asm=(cell/'V90Parameters.s').read_text()
        assert asm.index('_ZN13V90ParametersC2EP19_tagModemParameters:')<asm.index('_ZN13V90ParametersC1EP19_tagModemParameters:')
    facts={};writer={}
    for label,cell in [('pre',pre),('post',post)]:
        facts[label]={};writer[label]={}
        for stage in ('27.flow2','28.peephole2','30.rnreg','35.mach'):
            chunks=[c for c in (cell/('V90Parameters.cpp.'+stage)).read_text().split(';; Function ') if c.startswith('void V90Parameters::setToDefault(')]
            assert len(chunks)==1
            writer[label][stage]={str(u):p for u,p in instructions(chunks[0]).items() if '1076' in str(p)}
            roots=constructors(cell/('V90Parameters.cpp.'+stage))[0]
            facts[label][stage]={str(u):p for u,p in roots.items() if u in (28,32,116,117,118,119)}
    output={'raw_objects_preserved':2,'emitted_body_repeat_comparisons':18,'constructor_clone_streams':len(report)*4,
            'first_C2_pattern_divergence':first,'excluded_nonstream_dumps':['00.cgraph','08.gcse'],
            'ordered_dump_constructor_mapping':['C2','C1'],'stage_equalities':report,'selected_C2_patterns':facts,'prior_writer_field_patterns':writer}
    (OUT/'constructor-stage-trace.json').write_text(json.dumps(output,indent=2)+'\n')
    print('2 raw objects preserved /18 emitted-body repeats; %d constructor clone streams'%output['constructor_clone_streams'])
    print('Both constructor instruction-pattern streams equal through27.flow2; C2 first differs at28.peephole2')
if __name__=='__main__':main()
