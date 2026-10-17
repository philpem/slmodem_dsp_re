#!/usr/bin/env python3
"""Audit unchanged TU and the deliberately wrong-scale ELF control.

Run recovery_equalizer_numeric.py, make period T=t_v90equnumeric, then
probe_equalizer_numeric.py with baseline/candidate.o and wrong-scale.o into
build/equalizer-numeric/{baseline,wrong-scale}-probe respectively. A wrong
control remains a failing command; this audit checks its recorded failure.
"""
import json
from pathlib import Path
import playbook_small_patterns as d
from gcc3_value_carriers_audit import inspect


def main():
    root=Path(__file__).resolve().parents[1]
    out=root/'build/recovery-equalizer-numeric'
    base=out/'V90Equalizer/baseline/candidate.o'
    wrong=out/'V90Equalizer/wrong-scale.o'
    result=json.loads((out/'results.json').read_text())['families']['V90Equalizer']['cells']['baseline']
    assert result['baseline_reproduced']
    a,b=inspect(base),inspect(wrong)
    for key in ('records','nobits','relocations','text_positions'):
        assert a[key]==b[key],key
    changed_sections=[name for name in a['allocated'] if a['allocated'][name]!=b['allocated'][name]]
    assert changed_sections==['.rodata.cst4'],changed_sections
    mutation=json.loads((out/'V90Equalizer/wrong-scale.json').read_text())
    before=bytearray.fromhex(a['allocated']['.rodata.cst4'])
    at=mutation['section_offset'];assert before[at:at+4].hex()==mutation['before']
    before[at:at+4]=bytes.fromhex(mutation['after'])
    assert before.hex()==b['allocated']['.rodata.cst4']
    names=result['functions'];assert len(names)==35
    changed=[n for n in names if d.b.body(str(base),n)!=d.b.body(str(wrong),n)]
    assert changed==['_ZN12V90Equalizer21convertEqualizerToMmxEv'],changed
    exact=lambda obj:sorted(n for n in names if d.b.verdict(*d.b.body(d.b.BLOB,n),*d.b.body(str(obj),n))[0]=='EXACT')
    assert exact(base)==exact(wrong)
    reports={}
    for label,status,text in [('baseline',0,'PASS constructed equalizer reciprocal-scale quantization 812 checks'),
                              ('wrong-scale',1,'FAIL constructed equalizer reciprocal-scale quantization 81/812 checks failed')]:
        folder=root/'build/equalizer-numeric'/(label+'-probe')
        report=json.loads((folder/'results.json').read_text())
        assert report['link_exit']==0 and report['run_exit']==status
        assert text in (folder/'run.log').read_text()
        reports[label]={'run_exit':status,'checks':812,'failures':0 if status==0 else 81,
                        'object_sha256':report['equalizer_object_sha256']}
    report={'raw_baseline_reproduced':True,'functions':len(names),'exact_names':exact(base),
            'canary_changed_bodies':changed,'unchanged_bystanders':len(names)-len(changed),
            'bindings_positions_nontext_relocations_equal':True,'canary':mutation,'probes':reports,
            'source_conclusion':'Current source already expresses 1/(beta*2**k). Retained compiler emits 2**-k/beta; no source error established.',
            'scope':'54 constructed component trials, 18 crossed exponent boundaries, finite positive betas; no negotiated modem or NaN claim',
            'original_anchors':{'LE':['373d8 multiply 2**24','373de reciprocal','373e2 multiply maximum','3742c shift +d0','37441 beta +cc'],
                                'DFE':['3788a multiply 2**20','37890 reciprocal','37894 multiply maximum','378de shift +110','378f3 beta +10c']},
            'stop':'No source/flag graph forcing: existing grouping matches original; naming a denominator would be an unproven carrier experiment, not an established correction.',
            'next_numeric_discriminator':'V90Parameters::setToDefault: map each differing pool value to positioned destination-field stores, including immediate stores, before treating pool absence as a value defect.'}
    (out/'2026-10-17-audit.json').write_text(json.dumps(report,indent=2)+'\n')
    print('PASS: raw baseline; 35 bodies, 34 canary bystanders, 20 exact names preserved; positive 812/812, negative 81/812 failures')

if __name__=='__main__':main()
