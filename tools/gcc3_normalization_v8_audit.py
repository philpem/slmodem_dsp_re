#!/usr/bin/env python3
"""Audit the four complete V8 normalization objects and promotion proof."""
import json
import playbook_small_patterns as d
from gcc3_value_carriers_audit import inspect
from gcc3_loop_memory_trace import trace

def main():
    out=d.ROOT/'build/gcc3-normalization-v8';family=out/'V8global'
    results=json.loads((out/'results.json').read_text())['families']['V8global']['cells']
    assert results['baseline']['baseline_reproduced']
    bp=family/'baseline/candidate.o';base=inspect(bp);names=d.b.sizes(str(bp));reports=[]
    for label,cell in results.items():
        path=family/label/'candidate.o';got=inspect(path)
        assert set(names)==set(d.b.sizes(str(path))), (label,'emitted helpers or lost symbols')
        for key in ['records','allocated','nobits','relocations']:assert got[key]==base[key],(label,key)
        changed=[name for name in names if d.b.body(str(bp),name)!=d.b.body(str(path),name)]
        assert set(changed)<={'V8agc'},(label,changed)
        assert not cell.get('gains',[]) and not cell.get('losses',[])
        dump=family/label/'V8global.c.09.loop';observed=trace(dump.read_text(),'V8agc')
        words=[event for report in observed['reports'] for event in report['promotions'] if event['access']=='r/w' and event['mode']=='HI' and 'count' in str(event['memory'])]
        assert len(words)==int(label.startswith('pointed-helper')),(label,words)
        for event in words:
            assert len(event['store_uids'])==1 and set(event['store_loop_depths'].values())=={0},(label,event)
        reports.append(dict(label=label,functions=len(names),unchanged_bodies=len(names)-len(changed),changed_bodies=changed,verdict=cell['verdicts']['V8agc'],V8agc_size=d.b.sizes(str(path))['V8agc'],data_metadata_NOBITS_nontext_relocations_equal=True,gains=[],losses=[],word_count_promotions=words,loop_trace=observed))
    summary=dict(cells=len(reports),emitted_body_comparisons=sum(r['functions'] for r in reports),reports=reports)
    (out/'full-tu-audit.json').write_text(json.dumps(summary,indent=2)+'\n')
    print(summary['cells'],'cells;',summary['emitted_body_comparisons'],'body comparisons;')
    for r in reports:print(r['label'],r['verdict'],r['V8agc_size'],'HI count promotions',len(r['word_count_promotions']))
if __name__=='__main__':main()
