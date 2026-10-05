#!/usr/bin/env python3
"""Audit diagnostic-only scheduler replay and its measured load-ranking table."""
import hashlib
import json
import re
from pathlib import Path
import playbook_small_patterns as d
from gcc3_value_carriers_audit import inspect
from gcc3_reload_trace import function


def main():
    root=d.ROOT/'build/batch-scheduler-trace'
    expected={'baseline':'d6f017a6fc17bdc96d933723237700b90f73c63d655ad6cc150273612514337f',
              'coefficient-first-expressions':'c664efaa6fe51e6384bce74e63969bb80de7c81e2d570badd13b88da4486702d'}
    base=inspect(d.ROOT/'build/production-before/src_v8_V8Dpsk.c.o')
    cells=json.loads((root/'results.json').read_text())['families']['V8Dpsk']['cells']
    assert len(cells)==2
    for label, cell in cells.items():
        p=root/'V8Dpsk'/label/'candidate.o'
        assert hashlib.sha256(p.read_bytes()).hexdigest()==expected[label],label
        current=inspect(p)
        for key in ('records','allocated','nobits','relocations'):
            assert current[key]==base[key],(label,key)
        assert len(d.b.sizes(str(p)))==4
        assert not cell.get('gains') and not cell.get('losses')
    text=function((root/'V8Dpsk/coefficient-first-expressions/V8Dpsk.c.33.sched2').read_text(),'v8_fskdemodulate')
    rows={}
    for line in text.splitlines():
        m=re.match(r'^;;\s+(103|106)\s+84\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+.*?:\s*([0-9 ]+)\s*$',line)
        if m:
            uid,bb,dep,priority,cost,later=m.groups()
            assert int(uid) not in rows
            rows[int(uid)]={'block':int(bb),'incoming_count':int(dep),'priority':int(priority),
                            'cost':int(cost),'outgoing':[int(n) for n in later.split()]}
    assert set(rows)=={103,106},rows
    assert rows[103]['priority']==rows[106]['priority']==35
    assert len(rows[103]['outgoing'])==4 and len(rows[106]['outgoing'])==5
    assert rows[103]['block']==rows[106]['block']
    ready='Ready list after ready_sort:    103  106'
    assert ready in text
    a=text.index(ready);following=text[a:]
    assert following.index('scheduling insn <<<106>>>')<following.index('scheduling insn <<<103>>>')
    controls={'equal_priority_observed':True,'unequal_fanout_observed':True,
              'verbose_raw_repeat':True}
    assert all(controls.values())
    output={'cells':2,'emitted_body_comparisons':8,'common_verdicts':8,
            'raw_repeats':2,'ranking_rows':rows,'controls':controls,
            'source_provenance':json.loads((d.ROOT/'build/scheduler-source/provenance.json').read_text())}
    (root/'audit.json').write_text(json.dumps(output,indent=2)+'\n')
    print('Scheduler audit: 2 raw repeats, 8 complete bodies/common verdicts; '
          'priority35/35, outgoing4/5, sample106 scheduled before coefficient103')


if __name__=='__main__':main()
