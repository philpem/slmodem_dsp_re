#!/usr/bin/env python3
"""Full-TU/control audit for the near-frame vector ownership domains."""
import json
import re
import playbook_small_patterns as d
from gcc3_value_carriers_audit import inspect,function_chunk
P='_ZN18V90Phase3Modulator'
PHASE=P+'15generateJdPhaseEv';V92=P+'13generateV92JdEv';TRN=P+'13generateTRN1dEv';NOT=P+'13generateJdNotEv'

def normalized_rtl(text):
    # Only unstable compiler addresses, numeric assembly-label IDs and source
    # locations are erased. Pseudos, instructions and operands stay intact.
    text=re.sub(r'(?<=<function_decl )0x[0-9a-f]+','ADDRESS',text)
    text=re.sub(r'0x[0-9a-f]+(?= NOTE_INSN_BLOCK_(?:BEG|END))','ADDRESS',text)
    text=re.sub(r'((?:code_label(?::\w+)?)[^\n]*?\d+ \d+ \d+ (?:\d+ )?)\d+( "")',r'\1LABEL\2',text)
    text=re.sub(r' \d+ "[^\"]+"',' LINE "FILE"',text)
    return text

def main():
    reports=[]
    for package in ['vector','vector-narrow','vector-late','vector-split','vector-consumer']:
        root=d.ROOT/'build'/('gcc3-alignment-'+package);family='V90Phase3Modulator'
        cells=json.loads((root/'results.json').read_text())['families'][family]['cells']
        assert cells['baseline']['baseline_reproduced']
        bp=root/family/'baseline/candidate.o';base=inspect(bp)
        for label,cell in cells.items():
            path=root/family/label/'candidate.o';got=inspect(path)
            for key in ['records','allocated','nobits','relocations']:
                assert got[key]==base[key],(package,label,key)
            changed=cell.get('changed_bodies',[])
            assert set(changed)<={PHASE,V92,TRN,NOT,P+'17generateV90SymbolEv'}
            for name in cell['functions']:
                if name not in changed:assert d.b.body(path,name)==d.b.body(bp,name)
            if package=='vector-split' and label=='phase-1-v92-1':
                assert set(cell['gains'])=={PHASE,V92,TRN} and not cell['losses']
                assert set(changed)=={PHASE,V92,TRN}
            reports.append({'package':package,'label':label,'body_verdicts':len(cell['verdicts']),
                            'changed':changed,'gains':cell.get('gains',[]),'losses':cell.get('losses',[]),
                            'unchanged_bystanders':len(cell['functions'])-len(changed),
                            'target_verdicts':{name:cell['verdicts'][name] for name in [PHASE,V92,TRN,NOT]}})
    # The non-source TRN gain must be classified at its actual first pass.
    root=d.ROOT/'build/gcc3-alignment-vector/V90Phase3Modulator'
    checks=[]
    for stage in ['01.rtl','20.combine','25.greg','26.postreload','35.mach']:
        chunks=[function_chunk(root/label/('V90Phase3Modulator.cpp.'+stage),'::generateTRN1d(')
                for label in ['baseline','direct-0-level-1']]
        same=normalized_rtl(chunks[0])==normalized_rtl(chunks[1])
        assert same==(stage!='35.mach'),stage
        checks.append({'stage':stage,'same_normalized_RTL':same})
    observations=json.loads((d.ROOT/'build/gcc3-alignment-vector-observation/results.json').read_text())
    assert len(observations['cells'])==4
    frame_checks=0;slot_checks=0
    for label,cell in observations['cells'].items():
        assert cell['raw_object_unchanged']
        trace=cell['trace']
        assert trace['frames'] and {e['function'] for e in trace['frames']}=={'generateJdPhase','generateV92Jd','generateTRN1d'}
        for e in trace['frames']:
            assert e['locals_size']==0 and e['preferred_stack_boundary']==128 and e['outgoing_args_size']==8
            assert not e['frame_pointer_needed'] and e['stack_alignment_needed']==32
            n=e['layout']['nregs'];offset=4+n*4;pad=(-offset-8)%16
            assert e['layout']['padding2']==pad and e['layout']['to_allocate']==8+pad
            assert e['layout']['frame_pointer_offset']==offset and e['layout']['stack_pointer_offset']==offset+8+pad
            frame_checks+=1
        for e in trace['events']:
            assert e['mode']=='BLKmode' and e['size']==0 and e['frame_before']==e['frame_after']==0
            slot_checks+=1
        known=[e for e in trace['callees'] if e['callee']=='process' and e['asm_written']]
        assert known and all(e['known_incoming_boundary']==0 for e in known)
    assert frame_checks==196 and slot_checks==16
    report={'driver_cells':len(reports),'body_verdicts':sum(r['body_verdicts'] for r in reports),
            'reports':reports,'observational_frame_checks':frame_checks,'zero_size_slot_checks':slot_checks,'TRN_first_difference_checks':checks,
            'comparator_unchanged':True}
    (d.ROOT/'build/gcc3-alignment-vector-audit.json').write_text(json.dumps(report,indent=2)+'\n')
    print(len(reports),'complete-TU audits;',report['body_verdicts'],'body verdicts;5 TRN stage controls')
if __name__=='__main__':main()
