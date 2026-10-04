#!/usr/bin/env python3
"""Original-backed V92 Ja post-call owner x unsigned-wide sample cross."""
import itertools
import json
import sys
from gcc3_value_carriers_audit import inspect,function_chunk
from gcc3_alignment_vector_audit import normalized_rtl
import playbook_small_patterns as d

def variants(path,source):
    cells={}
    for wide,postcall in itertools.product((False,True),repeat=2):
        start,end,fn=d.function(source,'jaSymbol')
        if wide:
            assert fn.count('short sample;')==1
            fn=fn.replace('short sample;','unsigned int sample;')
            fn=fn.replace('sample = m->codeLevel;','sample = (unsigned short)m->codeLevel;')
            fn=fn.replace('sample = (short)-sample;','sample = -sample;')
            fn=fn.replace('return sample;','return (short)sample;')
        if postcall:
            old='\tm->polarity ^= m->scrambler.process(bit);\n\n\tsample = '+('(unsigned short)' if wide else '')+'m->codeLevel;'
            new='\tint scrambled = m->scrambler.process(bit);\n\tsample = '+('(unsigned short)' if wide else '')+'m->codeLevel;\n\tm->polarity ^= scrambled;'
            assert fn.count(old)==1
            fn=fn.replace(old,new)
        cells['baseline' if not(wide or postcall) else 'wide-%d-postcall-%d'%(wide,postcall)]=source[:start]+fn+source[end:]
    return cells
def audit():
    folder=d.ROOT/'build/gcc3-v92-sample';family='V92Phase3Modulator'
    cells=json.loads((folder/'results.json').read_text())['families'][family]['cells']
    target='_ZN18V92Phase3Modulator10generateJaEv';pump='_ZN18V92Phase3Modulator14generateSymbolEv'
    trn='_ZN18V92Phase3Modulator13generateTRN1uEv'
    bp=folder/family/'baseline/candidate.o';base=inspect(bp);reports={}
    assert len(cells)==4 and cells['baseline']['baseline_reproduced']
    for label,cell in cells.items():
        path=folder/family/label/'candidate.o';q=inspect(path);wide=label.startswith('wide-1')
        for key in ('records','allocated','nobits'):assert q[key]==base[key],(label,key)
        assert not cell.get('losses',[])
        assert cell.get('gains',[])==([target] if wide else [])
        changed=cell.get('changed_bodies',[])
        assert set(changed)==({target,pump} if wide else set()),(label,changed)
        bystanders={}
        for name in cell['functions']:
            if name not in changed:
                assert d.b.body(path,name)==d.b.body(bp,name),(label,name)
                bystanders[name]='raw body/canonical relocations identical'
        before=base['relocations'];after=q['relocations'];assert len(before)==len(after)==16
        expected_deltas=[0,-10,0,-10,-5]+[0]*11 if wide else [0]*16
        table=[]
        for index,(old,new) in enumerate(zip(before,after)):
            assert old[:3]==new[:3] and old[3][:2]==new[3][:2]==('audited-code-destination',pump)
            assert new[3][2]-old[3][2]==expected_deltas[index],(label,index,old,new)
            table.append({'state':index,'before':old[3][2],'after':new[3][2],'instruction_boundary_proven':True})
        stages={}
        for stage in ('01.rtl','20.combine','25.greg','26.postreload','35.mach'):
            original=function_chunk(folder/family/'baseline'/('V92Phase3Modulator.cpp.'+stage),'::generateTRN1u(')
            candidate=function_chunk(folder/family/label/('V92Phase3Modulator.cpp.'+stage),'::generateTRN1u(')
            assert normalized_rtl(original)==normalized_rtl(candidate),(label,stage,'unchanged TRN source/stage')
            stages[stage]='TRN normalized RTL identical'
        ja=function_chunk(folder/family/label/'V92Phase3Modulator.cpp.01.rtl','::generateJa(')
        assert ja.count('(sign_extend:SI')==(2 if wide else 3)
        assert ja.count('(zero_extend:SI')==(2 if wide else 1)
        reports[label]={'function_count':len(cell['functions']),'bystanders':bystanders,'changes':changed,
          'target_verdict':cell['verdicts'][target],'pump_verdict':cell['verdicts'][pump],
          'TRN_verdict':cell['verdicts'][trn],'TRN_stages':stages,'table_targets':table,
          'metadata_named_data_allocated_nontext_imports_equal':True,
          'initial_Ja_sign_extend_nodes':ja.count('(sign_extend:SI'),'initial_Ja_zero_extend_nodes':ja.count('(zero_extend:SI')}
    assert (folder/family/'wide-1-postcall-0/candidate.o').read_bytes()==(folder/family/'wide-1-postcall-1/candidate.o').read_bytes()
    assert (folder/family/'baseline/candidate.o').read_bytes()==(folder/family/'wide-0-postcall-1/candidate.o').read_bytes()
    (folder/'complete-tu-audit.json').write_text(json.dumps({'audited_cells':4,'reports':reports},indent=2)+'\n')
    print('4/4 complete-TU audits; one77B Ja gain;20 raw-identical bystanders per winningcell;16 tabletargets accounted;zero losses')

if __name__=='__main__':
    if '--audit' in sys.argv:
        audit();raise SystemExit
    d.REV='14769433';d.OUT_NAME='gcc3-v92-sample';d.SOURCE_PATHS=('src/pump/v90/V92Phase3Modulator.cpp',)
    d.DUMP_FLAGS=('-v','-save-temps','-da');d.variants=variants;d.main()
