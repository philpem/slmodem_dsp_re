#!/usr/bin/env python3
"""Validate five constructor-history observations without changing state."""
import json
from pathlib import Path
import re
import playbook_small_patterns as d
from gentoo_peep2_search_audit import validate
from gentoo_peep2_search_reproduce import PINS,sha
from gcc3_stage_divergence import normalize
from gcc3_clone_label_trace import trace


def comparable(node):
    # A string_cst address printed by GCC is a compiler heap pointer, not the
    # constant value or pool destination. Preserve the symbol and real values.
    node=normalize(node)
    if not isinstance(node,list):return node
    result=[comparable(x) for x in node]
    for i in range(1,len(result)):
        if result[i-1]=='<string_cst' and isinstance(result[i],str) and re.fullmatch(r'0x[0-9a-f]+>',result[i]):
            result[i]='<tree-address>>'
    return result


def controls():
    a=['symbol_ref:SI',['"*.LC23"'],'<string_cst','0x123>']
    b=['symbol_ref:SI',['"*.LC23"'],'<string_cst','0x456>']
    assert comparable(a)==comparable(b)
    c=['symbol_ref:SI',['"*.LC24"'],'<string_cst','0x456>']
    assert comparable(a)!=comparable(c)
    assert comparable(['const_int','0x123'])!=comparable(['const_int','0x456'])
    return {'string_annotation_normalized':True,'pool_identity_retained':True,'real_constant_retained':True}


def main():
    root=d.ROOT/'build/residual-constructor-scratch'
    saved=json.loads((root/'results.json').read_text());assert len(saved['controls'])==5
    assert not saved['source_or_rtl_mutation']
    reports=[];all_groups={};streams={}
    for control in saved['controls']:
        label=control['control'];folder=root/label
        assert sha(root/control['compiler'])==PINS[control['compiler']]==control['compiler_sha256']
        assert sha(d.ROOT/'tools/gentoo_peep2_search_trace.py')==control['observer_sha256']
        hashes={'raw':sha(folder/'raw.o'),'traced':sha(folder/'traced.o'),
                'saved':sha(Path(control['source_directory'])/'candidate.o')}
        assert hashes==control['complete_object_hashes'] and len(set(hashes.values()))==1
        events=json.loads((folder/'events.json').read_text());streams[label]=events
        groups,matches=validate(events);all_groups[label]=list(groups.values())
        assert len(groups)==control['searches']
        visits=sum(len(g['candidates']) for g in groups.values());assert visits==control['candidate_visits']
        reports.append({'control':label,'searches':len(groups),'candidate_visits':visits,
                        'rejected_replacements':sum(not m['replacement_returned'] for m in matches.values()),
                        'raw_traced_saved_equal':True})
    assert streams['queue-baseline']==streams['queue-baseline-repeat']
    targets={}
    for label,groups in all_groups.items():
        targets[label]=[{'assembler_name':g['entry']['assembler_name'],'uid':g['entry']['uid'],
                         'entry':g['entry']['cursor_before'],'selected':g['return']['selected'],
                         'exit':g['return']['cursor_after']} for g in groups
                        if 'C1' in g['entry']['assembler_name'] or 'C2' in g['entry']['assembler_name']]
    scalar='_ZN5QueueIfE5writeEf'
    first,second=all_groups['queue-baseline'],all_groups['queue-isfull']
    # All events before the first scalar-write search agree, including visits
    # and selected registers. The alternate drops its dead-stack deallocator.
    cut=next(i for i,g in enumerate(first) if g['entry']['assembler_name']==scalar)
    cut2=next(i for i,g in enumerate(second) if g['entry']['assembler_name']==scalar)
    assert cut==cut2==22
    assert first[:cut]==second[:cut]
    writes={label:[g for g in groups if g['entry']['assembler_name']==scalar]
            for label,groups in all_groups.items() if label in ('queue-baseline','queue-isfull')}
    assert [g['entry']['uid'] for g in writes['queue-baseline']]==[117,29]
    assert [g['entry']['uid'] for g in writes['queue-isfull']]==[38]
    stack=writes['queue-baseline'][0]['entry']['input_pattern']
    assert stack['code']=='PARALLEL'
    assert stack['operands'][0]['operands'][0]['reg']==7
    assert stack['operands'][0]['operands'][1]['code']=='PLUS'
    assert stack['operands'][0]['operands'][1]['operands'][1]['value']==4
    assert [t['entry'] for t in targets['queue-baseline'] if 'C2' in t['assembler_name']][0]==0
    assert [t['entry'] for t in targets['queue-isfull'] if 'C2' in t['assembler_name']][0]==2
    rows=json.loads((d.ROOT/'build/residual-queue-context-audit.json').read_text())['cells']
    a,b=rows[0]['trace']['stages'],rows[1]['trace']['stages'];windows={}
    for stage in ('24.lreg','25.greg','26.postreload','27.flow2','28.peephole2','30.rnreg'):
        x=a[stage]['instruction_patterns'];y=b[stage]['instruction_patterns']
        delta=sorted(uid for uid in x.keys()|y.keys() if comparable(x.get(uid))!=comparable(y.get(uid)))
        windows[stage]=delta
        if stage<'28.peephole2':assert not delta
    assert len(windows['28.peephole2'])==12 and len(windows['30.rnreg'])==6
    previous=json.loads((d.ROOT/'build/residual-clone-label-audit.json').read_text())
    jd1=next(r['traces'][0] for r in previous['targets'] if r['symbol']=='_ZN5V90JdC1EP13V90Parameters')
    proof=json.loads((d.ROOT/'build/residual-stage-audit.json').read_text())
    source=next(r['baseline_directory'] for r in proof['targets'] if r['symbol']==jd1['symbol'])
    jd2=trace(d.ROOT/source,'V90Jd.cpp',jd1['header'],'_ZN5V90JdC2EP13V90Parameters')
    jd_windows={}
    for stage in ('24.lreg','25.greg','26.postreload','27.flow2','28.peephole2','30.rnreg'):
        x=jd1['stages'][stage]['instruction_patterns'];y=jd2['stages'][stage]['instruction_patterns']
        delta=sorted(uid for uid in x.keys()|y.keys() if comparable(x.get(uid))!=comparable(y.get(uid)))
        jd_windows[stage]=delta
        if stage<'28.peephole2':assert not delta
    assert jd_windows['28.peephole2']==['263','264'] and len(jd_windows['30.rnreg'])==9
    totals={k:sum(r[k] for r in reports) for k in ('searches','candidate_visits','rejected_replacements')}
    assert totals=={'searches':638,'candidate_visits':4304,'rejected_replacements':0}
    output={'controls':reports,'totals':totals,'constructor_searches':targets,'queue_C2_changed_pattern_uids':windows,
            'detector_controls':controls(),'baseline_repeat_identical':True,'jd_clone_changed_pattern_uids':jd_windows,
            'original_cursor_recovered':False,'source_adopted':False}
    (root/'audit.json').write_text(json.dumps(output,indent=2)+'\n')
    print('5 raw-equal controls;',totals,'repeat identical; 3 annotation/value controls pass')
    print('Queue C2 patterns identical through flow2; 12 peephole2 changes / 6 renaming residuals')
    print('Jd clones identical through flow2; 2 peephole2 changes / 9 renaming residuals')


if __name__=='__main__':main()
