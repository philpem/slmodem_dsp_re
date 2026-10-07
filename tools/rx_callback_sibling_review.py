#!/usr/bin/env python3
"""Read-only bounded receive wrapper graph inventory, not a source search."""
import json
from pathlib import Path
import playbook_small_patterns as d
from gcc_x87_transfer_screen import assembly
ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT.parent/'byteexact-x87-scheduling/build/production-before'

def main():
    blob=assembly(d.b.BLOB);rows=[]
    for family in ('V17','V21','V27','V29'):
        obj=BASE/('src_fax_'+family+'r_stc.c.o');ours=assembly(obj)
        for suffix in ('control','status'):
            name=family+'RX_'+suffix
            record={'symbol':name,'object':str(obj),'source':'src/fax/'+family+'r_stc.c',
                    'grade':d.b.verdict(*d.b.body(d.b.BLOB,name),*d.b.body(str(obj),name))}
            for label,body in [('original',blob[name]),('retained',ours[name])]:
                record[label]={'RET_count':sum(m=='ret' for a,m,o in body),
                               'CALL_count':sum(m=='call' for a,m,o in body),
                               'early_XOREAX':any(m=='xor' and o=='%eax,%eax' for a,m,o in body[:6]),
                               'literal_one_writes':[[a,m,o] for a,m,o in body if m=='mov' and o=='$0x1,%eax'],
                               'instructions':body}
            rows.append(record)
    output={'functions':8,'TUs':4,'already_exact':sum(r['grade'][0]=='EXACT' for r in rows),'rows':rows}
    (ROOT/'build/rx-callback-sibling-review.json').write_text(json.dumps(output,indent=2)+'\n')
    for r in rows:print(r['symbol'],r['grade'],'RET',r['original']['RET_count'],r['retained']['RET_count'],'CALL',r['original']['CALL_count'],r['retained']['CALL_count'],'earlyzero',r['original']['early_XOREAX'],r['retained']['early_XOREAX'])
if __name__=='__main__':main()
