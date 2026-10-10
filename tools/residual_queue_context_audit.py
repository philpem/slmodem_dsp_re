#!/usr/bin/env python3
"""Audit the Queue replay and resolve saved ambiguous headings with labels."""
import json
import hashlib
from pathlib import Path
import playbook_small_patterns as d
from gcc3_clone_label_trace import trace,self_test
from gcc3_value_carriers_audit import inspect
from gcc3_stage_divergence import fingerprint


def main():
    folder=d.ROOT/'build/residual-queue-context/V92Modulator'
    result=json.loads((folder.parent/'results.json').read_text())['families']['V92Modulator']
    header='V92Modulator::V92Modulator(unsigned int, V92Phase2Info*, V92Ja*, tagV90DILdescriptor*, V92CP*, V92MappingParams*, V92Parameters*)'
    constructor='_ZN12V92ModulatorC2EjP13V92Phase2InfoP5V92JaP19tagV90DILdescriptorP5V92CPP16V92MappingParamsP13V92Parameters'
    allowed={constructor,'_ZN5QueueIfE5writeEf','_ZN12V92Modulator8progressEPiRjPfj'}
    baseline=folder/'baseline/candidate.o';meta=inspect(baseline);records=[]
    assert result['cells']['baseline']['baseline_reproduced']
    for label,entry in result['cells'].items():
        obj=folder/label/'candidate.o';actual=inspect(obj)
        assert all(meta[k]==actual[k] for k in meta if k!='text_positions')
        changed={n for n in entry['functions'] if d.b.body(str(obj),n)!=d.b.body(str(baseline),n)}
        assert changed<=allowed
        if label!='baseline':
            assert set(entry['gains'])=={'_ZN5QueueIfE5writeEf'} and set(entry['losses'])=={constructor}
            assert changed==allowed
        t=trace(folder/label,'V92Modulator.cpp',header,constructor)
        records.append({'cell':label,'metadata_equal':True,'changed_bodies':sorted(changed),'trace':t})
    assert (folder/'isfull/candidate.o').read_bytes()==(folder/'space-local/candidate.o').read_bytes()
    # The three constructor argument constants do not exist as register loads
    # before peephole2; do not attribute their colors to earlier allocation.
    for row in records:
        stages=row['trace']['stages']
        for uid in ('611','613','615'):
            assert uid not in stages['27.flow2']['instruction_patterns']
            assert stages['28.peephole2']['instruction_patterns'][uid][0]=='set'
    verdicts=sum(len(e['verdicts']) for e in result['cells'].values());assert verdicts==90
    (folder.parent.parent/'residual-queue-context-audit.json').write_text(json.dumps({'cells':records,'controls':self_test(),'common_verdicts':verdicts,'source_adopted':False},indent=2)+'\n')
    saved=json.loads((d.ROOT/'build/residual-stage-audit.json').read_text());rows=[]
    inventory=json.loads((d.ROOT/'build/residual-stage-inventory.json').read_text())
    assert saved['revision']==inventory['revision']
    for entry in saved['targets']:
        if entry['mapping']!='ambiguous':continue
        obj=d.ROOT/entry['baseline_directory']/'candidate.o'
        expected=inventory['object_sha256'][entry['source'].replace('/','_')+'.o']
        assert hashlib.sha256(obj.read_bytes()).hexdigest()==expected,'archived baseline drift'
        successes=[];refusals=[]
        for h in sorted(set(entry['matched_headers'])):
            try:t=trace(d.ROOT/entry['baseline_directory'],Path(entry['source']).name,h,entry['symbol'])
            except (ValueError,AssertionError) as error:refusals.append({'header':h,'reason':str(error).split('; available:')[0]})
            else:successes.append(t)
        assert len(successes)<=1
        rows.append({'symbol':entry['symbol'],'demangled':entry['demangled'],'status':'linked' if successes else 'refused','traces':successes,'refusals':refusals})
    assert len(rows)==12 and sum(r['status']=='linked' for r in rows)==9
    (d.ROOT/'build/residual-clone-label-audit.json').write_text(json.dumps({'revision':saved['revision'],'targets':rows,'controls':self_test()},indent=2)+'\n')
    print('Queue: 3 full-TU cells / 90 verdicts, same exact gain and constructor loss; no adoption')
    print('Saved ambiguous headings: 9 linked / 12; remaining 3 refused; label controls:',self_test())


if __name__=='__main__':main()
