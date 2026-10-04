#!/usr/bin/env python3
"""Audit the complete V8 unsigned-run and count-loop controls."""
import json
import playbook_small_patterns as d
from gcc3_value_carriers_audit import inspect

def main():
    root=d.ROOT/'build/unblock-v8-runs'
    result=json.loads((root/'results.json').read_text())
    reports=[]
    for family,fr in result['families'].items():
        baseline=root/family/'baseline/candidate.o'
        assert fr['cells']['baseline']['baseline_reproduced']
        before=inspect(baseline);names=d.b.sizes(str(baseline))
        for label,cell in fr['cells'].items():
            path=root/family/label/'candidate.o';after=inspect(path)
            assert set(names)==set(d.b.sizes(str(path)))
            for key in ['records','allocated','nobits','relocations']:
                assert before[key]==after[key],(label,key)
            changed=[n for n in names if d.b.body(str(path),n)!=d.b.body(str(baseline),n)]
            assert set(changed)<={'v8_fskdemodulate'},changed
            assert not cell.get('gains',[]) and not cell.get('losses',[])
            reports.append(dict(family=family,label=label,functions=len(names),
                                common_verdicts=len(cell['verdicts']),changed=changed,
                                metadata_data_BSS_nontext_relocations_equal=True))
    summary=dict(cells=len(reports),emitted_body_comparisons=sum(r['functions'] for r in reports),
                 common_verdicts=sum(r['common_verdicts'] for r in reports),reports=reports)
    (d.ROOT/'build/unblock-v8-runs-unit-audit.json').write_text(json.dumps(summary,indent=2)+'\n')
    print(summary['cells'],'complete-TU controls;',summary['emitted_body_comparisons'],
          'body comparisons; bystanders and metadata/data/relocations unchanged; zero gains/losses')

if __name__=='__main__':main()
