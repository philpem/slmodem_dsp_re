#!/usr/bin/env python3
"""Require the integrated reciprocal normalizer to reproduce its independently built winner."""
import json
import playbook_small_patterns as d
from gcc3_value_carriers_audit import inspect

def main():
    before=d.ROOT/'build/production-before'
    after=d.ROOT/'build/tc_out'
    names={p.name for p in before.glob('*.o')}
    assert len(names)==300 and names=={p.name for p in after.glob('*.o')}
    assert (before/'.build-config').read_bytes()==(after/'.build-config').read_bytes()
    changed=[n for n in sorted(names) if (before/n).read_bytes()!=(after/n).read_bytes()]
    assert changed==['src_dsp_fpm_div32.c.o'],changed
    winner=d.ROOT/'build/gcc3-output-storage/fpm_div32/pointed-sibling-outputs/candidate.o'
    integrated=after/changed[0]
    assert winner.read_bytes()==integrated.read_bytes()
    a,b=inspect(before/changed[0]),inspect(integrated)
    for key in ['records','allocated','nobits','relocations']:
        assert a[key]==b[key],key
    functions=d.b.sizes(str(integrated))
    changed_bodies=[n for n in functions if d.b.body(str(before/changed[0]),n)!=d.b.body(str(integrated),n)]
    assert changed_bodies==['FPM_div_32'],changed_bodies
    assert d.b.verdict(*d.b.body(d.b.BLOB,'FPM_div_32'),*d.b.body(str(integrated),'FPM_div_32'))[0]=='EXACT'
    old=json.loads((d.ROOT/'build/baseline-byteident.json').read_text())
    new=json.loads((d.ROOT/'build/output-storage-final-byteident.json').read_text())
    gains=sorted(set(new['exact_symbols'])-set(old['exact_symbols']))
    losses=sorted(set(old['exact_symbols'])-set(new['exact_symbols']))
    assert gains==['FPM_div_32'] and not losses,(gains,losses)
    assert new['compared']==old['compared']==1852
    assert new['exact']==old['exact']+1==1057
    assert new['exact_bytes']==old['exact_bytes']+147==114842
    result=dict(objects=300,raw_unchanged=299,changed_objects=changed,
                winner_raw_reproduced=True,configuration_equal=True,
                metadata_data_BSS_nontext_relocations_equal=True,
                changed_bodies=changed_bodies,gains=gains,losses=losses,
                exact=new['exact'],compared=new['compared'],exact_bytes=new['exact_bytes'])
    (d.ROOT/'build/output-storage-production-audit.json').write_text(json.dumps(result,indent=2)+'\n')
    print('300 objects; 299 raw unchanged; winner raw reproduced; 1057/1852 exact, +147 bytes; zero losses')

if __name__=='__main__':main()
