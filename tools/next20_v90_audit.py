#!/usr/bin/env python3
"""Complete inventories and retained bodies for next20 V90 controls."""
import json
from pathlib import Path
import playbook_small_patterns as d
from gcc3_value_carriers_audit import inspect, function_chunk
from gcc3_alignment_vector_audit import normalized_rtl

def main():
    for name in ['dil','boundaries','coefficients','input-cfg','filter-cross','bitloop','precoder-copy']:
        out=d.ROOT/'build'/('next20-v90-'+name)
        if not (out/'results.json').exists():continue
        package=json.loads((out/'results.json').read_text());rows={}
        for family,fr in package['families'].items():
            assert fr['cells']['baseline']['baseline_reproduced']
            bp=out/family/'baseline/candidate.o';base=inspect(bp)
            for cell,cr in fr['cells'].items():
                p=out/family/cell/'candidate.o';q=inspect(p)
                row={k:q[k]==base[k] for k in ['records','allocated','nobits','relocations']}
                assert all(row.values()),(name,family,cell,row)
                changed=[n for n in d.b.sizes(str(bp)) if d.b.body(str(bp),n)!=d.b.body(str(p),n)]
                row.update(changed_bodies=changed,common_bodies=len(d.b.sizes(str(bp))),unchanged_bodies=len(d.b.sizes(str(bp)))-len(changed),gains=cr.get('gains',[]),losses=cr.get('losses',[]))
                rows[family+'/'+cell]=row
        (out/'full-tu-audit.json').write_text(json.dumps(rows,indent=2)+'\n')
        print(name,'cells',len(rows),'data/meta/relocs equal',sum(all(row[k] for k in ['records','allocated','nobits','relocations']) for row in rows.values()))
    # Preserve the first incoming pointer-index RTL distinction, not just size score.
    out=d.ROOT/'build/next20-v90-boundaries/V90Demapper'
    for stage in ['01.rtl','02.sibling','04.jump','05.null','06.cse','09.loop','20.combine','34.stack','35.mach']:
        left=function_chunk(out/'baseline'/('V90Demapper.cpp.'+stage),'V90Demapper::process')
        right=function_chunk(out/'integer-index-owner'/('V90Demapper.cpp.'+stage),'V90Demapper::process')
        import difflib
        diff='\n'.join(difflib.unified_diff(normalized_rtl(left).splitlines(),normalized_rtl(right).splitlines(),fromfile='baseline',tofile='integer-index-owner'))+'\n'
        (out/('process-'+stage+'.diff')).write_text(diff)
if __name__=='__main__':main()
