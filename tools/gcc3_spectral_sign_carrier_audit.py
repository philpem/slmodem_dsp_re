#!/usr/bin/env python3
"""Audit the finite spectral display controls, including negative cells."""
import playbook_small_patterns as d
from helper_caller_audit import inspect,allocated
from gcc3_adid_first_study_audit import masked
from narrow_call_boundary_screen import bodies,sha
import json


def signature(path):
    rows=bodies(path)['displaySpectralParams']
    return {'fcompp':sum(r['mnemonic']=='fcompp' for r in rows),
            'fcomp':sum(r['mnemonic']=='fcomp' for r in rows),
            'byte_extensions':sum(r['mnemonic']=='movsbl' for r in rows),
            'extended_loads':sum(r['mnemonic']=='fldt' for r in rows),
            'double_loads':sum(r['mnemonic']=='fldl' for r in rows),
            'double_memory_multiply':sum(r['mnemonic']=='fmull' for r in rows),
            'calls':[r['target'] for r in rows if 'target' in r]}


def main():
    root=d.ROOT/'build/gcc3-spectral-sign-carrier'
    family=root/'V90MappingParamsInt'
    cells=json.loads((root/'results.json').read_text())['families']['V90MappingParamsInt']['cells']
    basepath=family/'baseline/candidate.o';base=inspect(basepath);data=allocated(basepath)
    assert basepath.read_bytes()==(family/'retained.o').read_bytes()
    original=signature(d.b.BLOB)
    assert original['fcompp']==4 and original['byte_extensions']==0
    assert original['double_loads']==4 and original['extended_loads']==0
    reports={};verdicts=0
    for label,cell in cells.items():
        path=family/label/'candidate.o'
        assert sha(path)==cell['object_hash']
        now=inspect(path);newdata=allocated(path)
        assert now['records']==base['records'] and now['objects']==base['objects']
        changed=[n for n in cell['functions'] if masked(path,n)!=masked(basepath,n)]
        assert set(changed)<={'displaySpectralParams'}
        diff=[k for k in sorted(set(data)|set(newdata)) if data.get(k)!=newdata.get(k)]
        assert set(diff)==({'.rodata.cst8','.rodata.cst16'} if label.endswith('-df') else set()),(label,diff)
        assert not cell.get('gains') and not cell.get('losses')
        sig=signature(path);assert sig['calls']==original['calls']==['edprintf']*6
        if label.endswith('-df'):
            assert sig['extended_loads']==0 and sig['double_memory_multiply']==4
            assert sig['double_loads']==0  # Mode recovery alone is not original load/lifetime recovery.
        else:assert sig['extended_loads']==4
        reports[label]={'verdict':cell['verdicts']['displaySpectralParams'],'changed_bodies':changed,
                        'constant_pool_changes':{k:{'baseline':data.get(k),'candidate':newdata.get(k)} for k in diff},
                        'signature':sig,'metadata_named_data_bystanders_calls':'pass'}
        verdicts+=len(cell['verdicts'])
    assert (len(cells),verdicts)==(24,264)
    assert len({c['source_hash'] for c in cells.values()})==20
    assert len({c['object_hash'] for c in cells.values()})==9
    report={'original':original,'cells':reports,'denominator':{'labelled_full_TU_cells':24,
            'unique_sources':20,'unique_objects':9,'shared_verdicts':264,'exact_gains':0,'exact_losses':0},
            'adopted_source':False}
    (d.ROOT/'build/spectral-sign-carrier-audit.json').write_text(json.dumps(report,indent=2)+'\n')
    print('24 complete-TU cells /20 unique sources /9 emissions /264 verdicts; no gains or losses')
    print('Raw baseline, metadata/named data, ten bystanders and six-call sequence pass in every cell')
    print('DF cells change only two constant pools and emit four FMULL, not original four FLDL loads')


if __name__=='__main__':main()
