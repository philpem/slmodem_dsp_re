#!/usr/bin/env python3
"""Audit complete V23 diagnostic/type controls and independently chosen objects."""
import hashlib
import json
import re
from pathlib import Path
import playbook_small_patterns as d
from helper_caller_audit import inspect, allocated
from gcc3_adid_first_study_audit import masked
from gcc3_stage_divergence import chunks


def main():
    reports = {}
    counts = [0, 0]
    for rootname in ('gcc3-missing-debug-boundary','gcc3-v23-status-width','gcc3-v23-lifecycle-debug'):
        root = d.ROOT/'build'/rootname
        manifest = json.loads((root/'results.json').read_text())
        rows = {}
        for family, record in manifest['families'].items():
            basepath = root/family/'baseline/candidate.o'
            base = inspect(basepath)
            assert basepath.read_bytes() == (d.ROOT/'build/v23-debug-before-objects'/Path(record['source_path'].replace('/','_')+'.o')).read_bytes()
            for label, cell in record['cells'].items():
                obj = root/family/label/'candidate.o'
                assert hashlib.sha256(obj.read_bytes()).hexdigest()==cell['object_hash']
                current = inspect(obj)
                differences = {n:[base['records'].get(n),current['records'].get(n)] for n in set(base['records'])|set(current['records']) if base['records'].get(n)!=current['records'].get(n)}
                imports = {'dsplibs_debug_level','dsplibs_debug_printf'}
                assert not differences or family in ('v23','bwchdem') and set(differences)==imports
                for old,new in differences.values():
                    assert old is None and new==['STT_NOTYPE','STB_GLOBAL','STV_DEFAULT','SHN_UNDEF',0]
                assert base['objects']==current['objects']
                changed = [name for name in cell['functions'] if masked(obj,name)!=masked(basepath,name)]
                allowed = {'v23':{'v23_create','v23_process'},'v23modem':{'V23ModemMain'},'bwchdem':{'BwChDem_Progress'},'v23rx':{'v23FP_rx_progress'}}.get(family,set())
                assert set(changed)<=allowed
                section_changes = [name for name in set(allocated(basepath))|set(allocated(obj)) if allocated(basepath).get(name)!=allocated(obj).get(name)]
                assert set(section_changes)<=({'.rodata.str1.1','.rodata.str1.4'} if family in ('v23','v23modem','bwchdem','v23rx') else set())
                assert not cell.get('gains') and not cell.get('losses')
                rows[family+'/'+label]={'changed_bodies':changed,'new_imports':sorted(differences),'changed_allocated_data':section_changes,'verdicts':cell['verdicts']}
                counts[0]+=1;counts[1]+=len(cell['verdicts'])
        reports[rootname]=rows
    assert counts==[43,135]
    first = d.ROOT/'build/gcc3-missing-debug-boundary/v23'
    assert (first/'both-debug/candidate.o').read_bytes()!=(first/'both-debug-owner-order/candidate.o').read_bytes()
    assert d.b.verdict(*d.b.body(d.b.BLOB,'v23_create'),*d.b.body(str(first/'both-debug-owner-order/candidate.o'),'v23_create'))==('BYTES',17)
    root = d.ROOT/'build/gcc3-v23-status-width'
    for family,src in [('v23','src_pump_v23_v23.c.o'),('v23modem','src_pump_v23_v23modem.c.o'),('bwchdem','src_pump_v23_bwchdem.c.o')]:
        selected = root/family/'both/candidate.o' if family=='v23' else d.ROOT/'build/gcc3-v23-lifecycle-debug'/family/'all-diagnostics/candidate.o'
        assert selected.read_bytes()==(d.ROOT/'build/tc_out'/src).read_bytes()
    target='v23_process'
    short=d.b.insns(str(root/'v23/diagnostics/candidate.o'),target)
    whole=d.b.insns(str(root/'v23/both/candidate.o'),target)
    assert ('movswl','%ax,%ebx') in short and not any(m=='movswl' and o.startswith('%ax,') for m,o in whole)
    # Attribute extension before later allocation/scheduling, from the initial RTL.
    pre=chunks(root/'v23/diagnostics/v23.c.01.rtl',target)[0]
    post=chunks(root/'v23/both/v23.c.01.rtl',target)[0]
    extension = r'\(set \(reg/v:SI \d+ \[ status \]\)\s*\(sign_extend:SI \(reg:HI \d+\)\)'
    assert re.search(extension, pre) and not re.search(extension, post)
    original=d.b.insns(d.b.BLOB,target)
    assert not any(m=='movswl' and o.startswith('%ax,') for m,o in original)
    for name in ('v23_create',target):
        original_calls=[o.split('@',1)[1] for m,o in d.b.insns(d.b.BLOB,name) if m=='call']
        current_calls=[o.split('@',1)[1] for m,o in d.b.insns(str(root/'v23/both/candidate.o'),name) if m=='call']
        assert original_calls==current_calls
    for family, name, expected in [('v23modem','V23ModemMain',1),('bwchdem','BwChDem_Progress',3),('v23rx','v23FP_rx_progress',3)]:
        selected = d.ROOT/'build/gcc3-v23-lifecycle-debug'/family/'all-diagnostics/candidate.o'
        for path in (d.b.BLOB,str(selected)):
            instructions=d.b.insns(path,name)
            assert sum(m=='call' and 'dsplibs_debug_printf' in o for m,o in instructions)==expected
        strings=lambda path: sorted(o.split('@',1)[1] for m,o in d.b.insns(path,name) if 'merge-string' in o)
        assert strings(d.b.BLOB)==strings(str(selected))
    report={'cells':reports,'denominator':{'full_TU_cells':43,'shared_body_verdicts':135},'selected_raw_production':True,'owner_order_worsens_verdict':True,'initial_RTL_extension_removed':True}
    (d.ROOT/'build/v23-debug-audit.json').write_text(json.dumps(report,indent=2)+'\n')
    print('43 full-TU cells/135 shared-body verdicts; metadata/named data/bystanders preserved; selected raw production matches')
    print('Original call targets restored; short-return extension absent from initial RTL and assembly; owner-order worsens BYTES10 to17')


if __name__=='__main__':
    main()
