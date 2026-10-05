#!/usr/bin/env python3
"""Audit full TUs and causal dump witnesses for the output-storage controls."""
import json
import playbook_small_patterns as d
from gcc3_value_carriers_audit import inspect
from gcc3_loop_memory_trace import trace as promotion
from gcc3_output_storage_trace import trace as storage, walk
from gcc3_reload_trace import function, instructions


def main():
    reports=[]
    for package,family,target in [('gcc3-output-storage','fpm_div32','FPM_div_32'),
                                 ('gcc3-output-storage-sqrt','fpm_sqrt','FPM_sqrt_dp'),
                                 ('gcc3-output-storage-log','fpm_log10','FPM_log10')]:
        root=d.ROOT/'build'/package/family
        results=json.loads((root.parent/'results.json').read_text())['families'][family]['cells']
        baseline=root/'baseline/candidate.o';before=inspect(baseline);names=d.b.sizes(str(baseline))
        assert results['baseline']['baseline_reproduced']
        for label,cell in results.items():
            obj=root/label/'candidate.o';after=inspect(obj)
            for key in ['records','allocated','nobits','relocations']:
                assert before[key]==after[key],(package,label,key)
            assert set(names)==set(d.b.sizes(str(obj)))
            changed=[n for n in names if d.b.body(str(obj),n)!=d.b.body(str(baseline),n)]
            assert set(changed)==set(cell.get('changed_bodies',[]))<={target}
            assert not cell.get('losses',[])
            expected=[target] if family=='fpm_div32' and label in ['pointed-sibling-outputs','pointed-grouped-outputs'] else []
            assert cell.get('gains',[])==expected,(label,cell.get('gains'))
            prefix=root/label/(family+'.c')
            evidence=storage(prefix.with_suffix('.c.01.rtl').read_text(),prefix.with_suffix('.c.35.mach').read_text(),
                             prefix.with_suffix('.s').read_text(),target)
            loop=promotion(prefix.with_suffix('.c.09.loop').read_text(),target)
            words=[p for r in loop['reports'] for p in r['promotions']
                   if p['access']=='r/w' and p['mode']=='HI' and 'count' in str(p['memory'])]
            pointed=(label!='baseline') if family=='fpm_log10' else label.startswith('pointed')
            assert len(words)==int(pointed)
            if pointed:
                assert len(words[0]['store_uids'])==1 and set(words[0]['store_loop_depths'].values())=={0}
                homes={h['role']:h['offset'] for h in evidence['output_homes']}
                paired=(family=='fpm_log10' or label!='pointed-control')
                assert homes==dict(mantissa=-2 if paired else -4,count=-4 if paired else -2)
                if family=='fpm_div32':
                    loads=evidence['HI_stack_loads'];assert len(loads)==1
                    assert loads[0]['rtl_mode']=='HI'
                    assert loads[0]['stack_offset']==(16 if paired else 18)
                    assert loads[0]['mnemonic']==('movl' if paired else 'movzwl')
            scratch=None
            if family=='fpm_sqrt' and label=='pointed-sibling-outputs':
                pre=instructions(function(prefix.with_suffix('.c.27.flow2').read_text(),target))
                peephole=instructions(function(prefix.with_suffix('.c.28.peephole2').read_text(),target))
                renamed=instructions(function(prefix.with_suffix('.c.30.rnreg').read_text(),target))
                inserted=[u for u in peephole if u not in pre and 'scratch' in str(peephole[u])]
                assert len(inserted)==1
                uid=inserted[0];assert peephole[uid]==renamed[uid]
                sets=[n for n in walk(peephole[uid]) if len(n)==3 and n[0]=='set']
                assert sets[0][1][:3]==['reg:SI','1','dx']
                scratch=dict(inserted_stage='28.peephole2',uid=uid,register='dx',renaming_unchanged=True)
            reports.append(dict(package=package,label=label,functions=len(names),
                                common_verdicts=len(cell['verdicts']),changed=changed,gains=expected,losses=[],
                                metadata_data_BSS_nontext_relocations_equal=True,storage=evidence,promotion=loop,epilogue_scratch=scratch))
        if family=='fpm_div32':
            assert (root/'pointed-sibling-outputs/candidate.o').read_bytes()==(root/'pointed-grouped-outputs/candidate.o').read_bytes()
            assert d.b.body(str(root/'baseline-sibling-outputs/candidate.o'),target)==d.b.body(str(baseline),target)
    summary=dict(cells=len(reports),common_verdicts=sum(r['common_verdicts'] for r in reports),
                 emitted_body_comparisons=sum(r['functions'] for r in reports),reports=reports)
    (d.ROOT/'build/output-storage-full-tu-audit.json').write_text(json.dumps(summary,indent=2)+'\n')
    print(summary['cells'],'full-TU cells;',summary['common_verdicts'],'common verdicts;',
          summary['emitted_body_comparisons'],'emitted bodies; one unique exact gain; all invariants pass')

if __name__=='__main__':main()
