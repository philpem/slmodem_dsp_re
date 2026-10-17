#!/usr/bin/env python3
"""Audit both complete call TUs and the witnessed local-object positions."""
import json
import re
from pathlib import Path
import gcc3_value_carriers_audit as a

ROOT=Path(__file__).resolve().parents[1]

def positions(path):
    text=a.function_chunk(path,'call_create')
    cfg=re.search(r'virtual-stack-vars\)\s+\(const_int (-?\d+) .*?cfg.w0',text,re.S)
    buf=re.search(r'\[ buf \]\).*?virtual-stack-vars\)\s+\(const_int (-?\d+)',text,re.S)
    assert cfg and buf
    return {'cfg_virtual_offset':int(cfg[1]),'dialstr_virtual_offset':int(buf[1])}

def main():
    root=ROOT/'build/recovery-call-create-stack'
    cells=json.loads((root/'results.json').read_text())['families']['call']['cells']
    assert cells['baseline']['baseline_reproduced']
    base=a.inspect(root/'call/baseline/candidate.o')
    candidate=a.inspect(root/'call/buffer-first/candidate.o')
    for key in ('records','allocated','nobits','relocations'):
        assert base[key]==candidate[key],key
    new=cells['buffer-first']
    assert new['changed_bodies']==['call_create']
    assert not new['gains'] and not new['losses']
    exact={label:sorted(n for n,v in c['verdicts'].items() if v[0]=='EXACT') for label,c in cells.items()}
    assert exact['baseline']==exact['buffer-first'] and len(exact['baseline'])==7
    observed={label:positions(root/'call'/label/'call.c.01.rtl') for label in cells}
    assert observed['baseline']=={'cfg_virtual_offset':-16,'dialstr_virtual_offset':-80}
    assert observed['buffer-first']=={'cfg_virtual_offset':-80,'dialstr_virtual_offset':-64}
    # Match original concrete identity of the two live objects, not frame size.
    dis=(root/'call/buffer-first/call_create.dis').read_text()
    assert 'mov    %ecx,0x10(%esp)' in dis
    assert 'lea    0x10(%esp),%esi' in dis
    assert 'mov    %cl,0x20(%esp)' in dis
    assert 'lea    0x20(%esp),%edx' in dis
    report={'function_verdicts':22,'stable_bystanders':10,'exact_names':exact,
            'data_bindings_and_all_nontext_relocations_equal':True,
            'nontext_relocation_denominator':len(base['relocations']),
            'initial_rtl_positions':observed,
            'known_negative_baseline_positions_detected':True,
            'original_addresses':{'cfg':'2c17 storeESP+10;2c3a LEAESP+10','dialstr':'2d76 LEAESP+20;2d80 storeESP+20'},
            'source_claim':'Reproduces witnessed stack-object allocation; no unique original declaration spelling claimed.'}
    (root/'audit.json').write_text(json.dumps(report,indent=2)+'\n')
    print('call stack audit: 2 cells, 22 function verdicts, 10 stable bystanders, 22 nontext relocs; baseline position negative control fired')

if __name__=='__main__':
    main()
