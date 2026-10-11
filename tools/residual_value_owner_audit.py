#!/usr/bin/env python3
"""Audit all seventeen SDM/value-owner cells, including every nonexact body."""
import json
from pathlib import Path
import playbook_small_patterns as d
from gcc3_value_carriers_audit import inspect
from gcc3_stage_divergence import chunks,fingerprint
from gcc3_reload_trace import instructions


def main():
    rows=[];verdicts=0
    for experiment in ('residual-sdm-init','residual-value-owner'):
        folder=d.ROOT/'build'/experiment
        result=json.loads((folder/'results.json').read_text())
        for family,record in result['families'].items():
            source=record['source_path'];root=folder/family
            target=('FPM_SDM_init' if family=='fpm_sdm' else 'SDM_init' if family=='SDM' else
                    '_ZN15V92BitsToSymbol19setSymbolsBlockSizeEj' if family=='V92bitsToSymbol' else '_ZN16V92EchoCanceller16resetEchoHistoryEv')
            header=('unsigned int V92BitsToSymbol::setSymbolsBlockSize(unsigned int)' if family=='V92bitsToSymbol' else
                    'void V92EchoCanceller::resetEchoHistory()' if family=='V92EchoCanceller' else target)
            baseline=root/'baseline/candidate.o';meta=inspect(baseline)
            assert record['cells']['baseline']['baseline_reproduced']
            for cell,entry in record['cells'].items():
                obj=root/cell/'candidate.o';actual=inspect(obj)
                assert all(meta[key]==actual[key] for key in meta if key!='text_positions')
                changed=[n for n in entry['functions'] if d.b.body(str(baseline),n)!=d.b.body(str(obj),n)]
                assert set(changed)<={target}
                assert not entry.get('losses',[])
                windows={}
                for before,after in [('24.lreg','25.greg'),('27.flow2','28.peephole2'),('28.peephole2','30.rnreg')]:
                    streams=[]
                    for stage in (before,after):
                        selected=chunks(root/cell/(Path(source).name+'.'+stage),header)
                        assert len(selected)==1
                        streams.append(instructions(selected[0]))
                    a,b=streams
                    windows[before+' -> '+after]=[uid for uid in sorted(set(a)|set(b)) if fingerprint(a.get(uid))!=fingerprint(b.get(uid))]
                row={'experiment':experiment,'family':family,'cell':cell,'target':target,'target_verdict':entry['verdicts'][target],
                     'target_bytes':d.b.sizes(str(obj))[target],'unchanged_bystanders':len(entry['functions'])-1,
                     'metadata_nontext_equal':True,'changed_bodies':changed,'stage_windows':windows}
                rows.append(row);verdicts+=len(entry['verdicts'])
                print(family,cell,row['target_bytes'],row['target_verdict'])
            if family=='V92bitsToSymbol':
                assert (root/'parameter-0-early-0/candidate.o').read_bytes()==baseline.read_bytes()
                assert (root/'parameter-0-early-1/candidate.o').read_bytes()==(root/'parameter-1-early-1/candidate.o').read_bytes()
    assert len(rows)==17 and verdicts==134
    positives=[r for r in rows if r['target_verdict'][0]=='EXACT']
    assert len(positives)==3 and len({r['target'] for r in positives})==2
    (d.ROOT/'build/residual-value-owner-audit.json').write_text(json.dumps({'cells':rows,'function_verdicts':verdicts,'strict_gains':2,
                                                                       'source_syntax_uniqueness':False},indent=2)+'\n')
    print('17 full-TU cells / 134 verdicts; two distinct exact gains; every bystander and metadata preserved')


if __name__=='__main__':main()
