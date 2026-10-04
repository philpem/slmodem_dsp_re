#!/usr/bin/env python3
"""Audit complete normalization TUs and explicit loop-promotion controls."""
import json
import playbook_small_patterns as d
from gcc3_value_carriers_audit import inspect
from gcc3_loop_memory_trace import trace

def main():
    reports=[]
    for package in ['owner','loop','reciprocal','guard']:
        root=d.ROOT/'build'/('gcc3-normalization-'+package)
        result=json.loads((root/'results.json').read_text())
        for family,fr in result['families'].items():
            bp=root/family/'baseline/candidate.o';base=inspect(bp)
            assert fr['cells']['baseline']['baseline_reproduced']
            target={'fpm_log10':'FPM_log10','fpm_sqrt':'FPM_sqrt_dp',
                    'fpm_div':'FPM_div','fpm_div32':'FPM_div_32'}[family]
            names=d.b.sizes(str(bp))
            for label,c in fr['cells'].items():
                path=root/family/label/'candidate.o';q=inspect(path)
                for key in ['records','allocated','nobits','relocations']:
                    assert q[key]==base[key],(package,family,label,key)
                assert set(d.b.sizes(str(path)))==set(names)
                changed=[n for n in names if d.b.body(str(path),n)!=d.b.body(str(bp),n)]
                assert set(changed)==set(c.get('changed_bodies',[]))<={target}
                assert not c.get('losses',[])
                expected=[target] if package=='guard' and label=='helper-owned-init' else []
                assert c.get('gains',[])==expected,(package,family,label,c.get('gains'))
                dump=root/family/label/(family+'.c.09.loop')
                observed=trace(dump.read_text(),target)
                word=[e for r in observed['reports'] for e in r['promotions']
                      if e['access']=='r/w' and e['mode']=='HI' and 'count' in str(e['memory'])]
                pointed=('word-output' in label or label=='loop-carried-word-output'
                         or package=='guard' and label!='baseline')
                assert len(word)==int(pointed),(package,family,label,word)
                if word:
                    assert len(word[0]['store_uids'])==1
                    assert set(word[0]['store_loop_depths'].values())=={0}
                reports.append(dict(package=package,family=family,label=label,
                                    common_verdicts=len(c['verdicts']),all_emitted=len(names),
                                    changed=changed,gains=c.get('gains',[]),losses=[],
                                    metadata_data_BSS_nontext_relocations_equal=True,
                                    loop_trace=observed))
    summary=dict(cells=len(reports),common_verdicts=sum(r['common_verdicts'] for r in reports),
                 emitted_body_comparisons=sum(r['all_emitted'] for r in reports),reports=reports)
    (d.ROOT/'build/normalization-full-tu-audit.json').write_text(json.dumps(summary,indent=2)+'\n')
    print(summary['cells'],'full TU audits;',summary['common_verdicts'],'common verdicts;',
          summary['emitted_body_comparisons'],'emitted bodies; all promotion controls and invariants pass')

if __name__=='__main__':main()
