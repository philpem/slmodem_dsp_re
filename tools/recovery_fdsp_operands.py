#!/usr/bin/env python3
"""Test coefficient capture and memory-operand ownership after stage diagnosis.

Reference ae900 loads coef before ae904 computes the hist address; ae907
multiplies hist memory. Baseline initial RTL computes hist address first;
20.combine folds coef memory into the multiply. Cross explicit coef capture
with source operand order, keeping adaptation and its closed domain intact.
"""
import json
import re
import subprocess
from collections import Counter
import playbook_small_patterns as d
from gcc3_value_carriers_audit import function_chunk
from recovery_fax_debug_audit import Object
from recovery_fixedrc_apparatus import allocated
from recovery_generator_separation import named_data

def variants(path, source):
    a,z,fn=d.function(source,'EchoCanceler')
    old="""			for (j = 0; j < ntaps; j++)
				acc += coef[j] * hist[pos + j];"""
    assert fn.count(old)==1
    result={'baseline':source}
    for captured,reverse in ((False,True),(True,False),(True,True)):
        coef='c' if captured else 'coef[j]'
        expr=('hist[pos + j] * '+coef) if reverse else (coef+' * hist[pos + j]')
        if captured:
            block='\t\t\tfor (j = 0; j < ntaps; j++) {\n\t\t\t\tfloat c = coef[j];\n\t\t\t\tacc += '+expr+';\n\t\t\t}'
        else:block='\t\t\tfor (j = 0; j < ntaps; j++)\n\t\t\t\tacc += '+expr+';'
        label=('capture-' if captured else '')+('history-first' if reverse else 'coef-first')
        result[label]=source[:a]+fn.replace(old,block)+source[z:]
    assert len(result)==len(set(result.values()))==4
    return result

def insn(chunk, uid):
    m=re.search(r'\(insn(?::\w+)? '+str(uid)+r' ',chunk)
    if m is None:return ''
    return chunk[m.start():chunk.find('\n\n',m.start())]


def stack_census(path):
    text=subprocess.check_output(['python3',str(d.ROOT/'tools/dis.py'),str(path),'EchoCanceler'],text=True,stderr=subprocess.DEVNULL)
    frame=int(re.search(r'sub\s+\$0x([0-9a-f]+),%esp',text)[1],16)
    offsets=[int(x,16) if x else 0 for x in re.findall(r'(0x[0-9a-f]+)?\(%esp\)',text)]
    locals_=Counter(x for x in offsets if x<frame)
    assert locals_ and set(locals_)=={0,4}
    return {'frame_bytes':frame,'stack_operand_count':len(offsets),
            'local_operand_count':sum(locals_.values()),'local_offsets':dict(locals_),
            'unreferenced_upper_frame_bytes':frame-8}


def audit():
    root=d.ROOT/'build'/d.OUT_NAME/'Fdspkrnl'
    before=root/'baseline/candidate.o';base=Object(before).canonical()
    report={'cells':4,'bodies_per_cell':len(base[0]),'collateral':{},'stages':{}}
    for label in ('history-first','capture-coef-first','capture-history-first'):
        obj=root/label/'candidate.o';cur=Object(obj).canonical()
        assert set(base[0])==set(cur[0]) and base[1:]==cur[1:]
        changed=[n for n in base[0] if base[0][n]!=cur[0][n]]
        assert set(changed)<={'EchoCanceler'}
        assert named_data(before)==named_data(obj)
        assert {n:v for n,v in allocated(before).items() if n!='.text'}=={
            n:v for n,v in allocated(obj).items() if n!='.text'}
        report['collateral'][label]={'changed':changed,'unchanged_bystanders':5,
                                     'data_bindings_consumers_equal':True,
                                     'raw_baseline_equal':before.read_bytes()==obj.read_bytes()}
    for label in ('baseline','history-first'):
        report['stages'][label]={}
        for stage in ('01.rtl','19.life','20.combine','24.lreg','25.greg','33.sched2','34.stack'):
            text=function_chunk(root/label/('Fdspkrnl.c.'+stage),'EchoCanceler')
            report['stages'][label][stage]={str(uid):insn(text,uid) for uid in (54,55,56,187,386)}
    stages=report['stages']
    assert '[ coef ]' in stages['baseline']['19.life']['54']
    assert '[ hist ]' in stages['baseline']['19.life']['55']
    assert stages['baseline']['20.combine']['54']==''
    assert '[ coef ]' in stages['baseline']['20.combine']['56']
    assert '[ hist ]' not in stages['baseline']['20.combine']['56']
    assert '[ hist ]' in stages['history-first']['20.combine']['56']
    assert '[ coef ]' not in stages['history-first']['20.combine']['56']
    assert '[ coef ]' in stages['history-first']['20.combine']['55']
    # Candidate positive and baseline negative for the nominated memory operand.
    def folds_history(row):return '[ hist ]' in row['56']
    assert folds_history(stages['history-first']['20.combine'])
    assert not folds_history(stages['baseline']['20.combine'])
    for label in ('baseline','history-first'):
        combine=stages[label]['20.combine']['187']
        reload=stages[label]['25.greg']['187']
        load=stages[label]['25.greg']['386']
        assert '(mem:SF' in combine and '(reg:SF 112)' in combine
        assert '(mem:SF' not in reload and '(reg:SF 9 st(1) [112])' in reload
        assert '(mem:SF' in load and '[107]' in load
    report['reference_stack']=stack_census(d.b.BLOB)
    report['candidate_stack']=stack_census(root/'history-first/candidate.o')
    assert report['reference_stack']['frame_bytes']==12
    assert report['candidate_stack']['frame_bytes']==8
    assert report['reference_stack']['unreferenced_upper_frame_bytes']==4
    report['limits']='Original frame has no final reference to its upper four bytes; prior eliminated slots and backend alignment remain unproven alternatives. No stack source change justified. Adaptation consumer reversal domain remains closed.'
    report['reference_dot_product']='ae900 FLDS coef; ae904 LEA hist index; ae907 FMULS hist'
    report['candidate_dot_product']='40 FLDS coef;44 LEA hist index;47 FMULS hist'
    assert (root/'history-first/Fdspkrnl.c').read_bytes()==(d.ROOT/'src/service/Fdspkrnl.c').read_bytes()
    (root.parent/'audit.json').write_text(json.dumps(report,indent=2)+'\n')
    print('operand audit: 4 cells;18 candidate body verdicts;15 unchanged neighbours; memory-fold positive/negative controls passed')
    print('stack census:',report['reference_stack'],report['candidate_stack'])


if __name__=='__main__':
    d.REV='5bfa2f09c';d.OUT_NAME='echo-operands'
    d.SOURCE_PATHS=('src/service/Fdspkrnl.c',);d.DUMP_FLAGS=('-da',)
    d.variants=variants;d.main();audit()
