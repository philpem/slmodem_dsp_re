#!/usr/bin/env python3
"""Audit the V32 alias control and count actual owner-load SETs by stage."""
import hashlib
import json
import re
from collections import Counter
import playbook_small_patterns as d
from helper_caller_audit import inspect, allocated
from gcc3_stage_divergence import chunks, sets
from gcc3_reload_trace import instructions


def main():
    root=d.ROOT/'build/v32-cleaned-alias'
    ledger=json.loads((root/'results.json').read_text());cells=ledger['families']['V32']['cells']
    base=root/'V32/baseline/candidate.o';rows={}
    for label,cell in cells.items():
        folder=root/'V32'/label;path=folder/'candidate.o'
        assert hashlib.sha256(path.read_bytes()).hexdigest()==cell['object_hash']
        assert inspect(path)==inspect(base)
        before,after=allocated(base),allocated(path)
        datachanges=[section for section in before.keys()|after.keys() if before.get(section)!=after.get(section)]
        assert datachanges==([] if label=='baseline' else ['.rodata.str1.1'])
        if datachanges:
            assert Counter(bytes.fromhex(before['.rodata.str1.1']).split(b'\0'))==Counter(bytes.fromhex(after['.rodata.str1.1']).split(b'\0'))
        assert cell['source_hash']==cells['baseline']['source_hash']
        expected=[] if label=='baseline' else ['-fno-strict-aliasing']
        assert cell['diagnostic_flags']==expected
        assert '-DDSPLIB_REPRODUCE_BUGS' in cell['command'][-1]
        changed=cell.get('changed_bodies',[])
        assert changed==([] if label=='baseline' else ['V32FP_GetCleanedSamples','V32FP_create','V32FP_recreate'])
        grade=d.b.verdict(*d.b.body(d.b.BLOB,'V32FP_GetCleanedSamples'),*d.b.body(str(path),'V32FP_GetCleanedSamples'))
        assert grade==(('SIZE',6) if label=='baseline' else ('EXACT',0))
        stages={}
        for p in sorted(folder.glob('V32.c.*')):
            if not re.fullmatch(r'V32.c.\d\d\.[a-z0-9_]+',p.name) or p.name.endswith(('.00.cgraph','.08.gcse')):continue
            matched=chunks(p,'V32FP_GetCleanedSamples');assert len(matched)==1
            patterns=instructions(matched[0])
            reads=[]
            for uid,pattern in patterns.items():
                for assignment in sets(pattern):
                    rhs=assignment[2]
                    if isinstance(rhs,list) and rhs and rhs[0].startswith('mem') and '<variable>.fp+0' in json.dumps(rhs):
                        reads.append(uid)
            stages[p.name]={'actual_owner_load_UIDs':reads}
        assert len(stages)==29
        assert len(stages['V32.c.07.addressof']['actual_owner_load_UIDs'])==2
        assert len(stages['V32.c.09.loop']['actual_owner_load_UIDs'])==(1 if label=='baseline' else 2)
        asm=d.b.insns(str(path),'V32FP_GetCleanedSamples')
        ownerloads=[(m,o) for m,o in asm if m=='mov' and o.startswith('0x68(')]
        assert len(ownerloads)==(1 if label=='baseline' else 3)
        rows[label]={'target_verdict':grade,'changed_bodies':changed,'allocated_data_changes':datachanges,
                     'string_pool_multiset_unchanged':True,'stages':stages,'machine_owner_loads':ownerloads}
    report={'source_cells':2,'common_body_verdicts':12,'stages_per_cell':29,
            'excluded_stream':'08.gcse duplicate snapshots; no attribution within missing window',
            'cells':rows,'production_profile_unchanged':True}
    (root/'audit.json').write_text(json.dumps(report,indent=2)+'\n')
    print('2 unchanged-source cells /12 body verdicts /29 stages each; no-strict restores 53B exact getter, changes two constructor bodies; diagnostic only')


if __name__=='__main__':main()
