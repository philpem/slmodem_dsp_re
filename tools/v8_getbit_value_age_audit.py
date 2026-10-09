#!/usr/bin/env python3
"""Audit four V8global source controls without adopting partial matches."""
import hashlib,json
from pathlib import Path
import playbook_small_patterns as d
from gcc3_value_carriers_audit import inspect
from gcc3_reload_trace import function,instructions
ROOT=d.ROOT;OUT=ROOT/'build/v8-getbit-value-age'

def main():
    ledger=json.loads((OUT/'results.json').read_text());family=ledger['families']['V8global']
    base=OUT/'V8global/baseline/candidate.o';retained=ROOT/'build/tc_out/src_v8_V8global.c.o'
    assert base.read_bytes()==retained.read_bytes()
    meta=inspect(base);report=[]
    for label,cell in family['cells'].items():
        folder=OUT/'V8global'/label;obj=folder/'candidate.o'
        assert hashlib.sha256(obj.read_bytes()).hexdigest()==cell['object_hash']
        assert hashlib.sha256((folder/'V8global.c').read_bytes()).hexdigest()==cell['source_hash']
        names=sorted(d.b.sizes(str(obj)));assert names==family['cells']['baseline']['functions']
        grades={n:list(d.b.verdict(*d.b.body(d.b.BLOB,n),*d.b.body(str(obj),n))) for n in names}
        assert grades==cell['verdicts']
        changed=[n for n in names if d.b.body(str(base),n)!=d.b.body(str(obj),n)]
        assert set(changed)<={'v8_getbit'};assert not cell.get('gains') and not cell.get('losses')
        current=inspect(obj)
        for key in ('records','allocated','nobits','relocations'):
            assert current[key]==meta[key],(label,key)
        for name in names:
            if name!='v8_getbit':assert current['text_positions'][name][1]==meta['text_positions'][name][1]
        streams={}
        for stage in ('01.rtl','04.jump','06.cse','20.combine','24.lreg','28.peephole2'):
            path=folder/('V8global.c.'+stage)
            assert path.exists(),str(path)
            rows=instructions(function(path.read_text(),'v8_getbit'))
            streams[stage]={'instructions':len(rows),'logical_right_shift':sum('lshiftrt:' in str(r) for r in rows.values()),
                           'arithmetic_right_shift':sum('ashiftrt:' in str(r) for r in rows.values()),
                           'word_operand_compares':sum('compare:' in str(r) and ':HI' in str(r) for r in rows.values())}
        report.append({'cell':label,'grade':grades['v8_getbit'],'changed_bodies':changed,'stage_features':streams})
    assert len(report)==4
    assert report[2]['stage_features']['01.rtl']['word_operand_compares'] > report[0]['stage_features']['01.rtl']['word_operand_compares']
    assert report[2]['stage_features']['01.rtl']['arithmetic_right_shift']==0
    (OUT/'audit.json').write_text(json.dumps({'raw_baselines':1,'cells':4,'body_grades':52,'gains':0,'losses':0,'rows':report},indent=2)+'\n')
    print('V8 getbit: 1 raw baseline / 4 cells / 52 live body grades; 0 gains / 0 losses')
    print('Only v8_getbit changes; all 12 sibling bodies and metadata/nontext fixed')
    for r in report:print(r['cell'],r['grade'],r['stage_features'])

if __name__=='__main__':main()
