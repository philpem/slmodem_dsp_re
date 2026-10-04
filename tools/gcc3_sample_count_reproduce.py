#!/usr/bin/env python3
import json,sys
import playbook_small_patterns as d
from gcc3_value_carriers_audit import inspect

def variants(path,source):
    name='RxHdxDataV'+('17' if 'V17' in path else '27' if 'V27' in path else '29')
    start,end,fn=d.function(source,name)
    variable='units' if 'V29' in path else 'r'
    assert fn.count('short '+variable+';')==1
    fn=fn.replace('short '+variable+';','unsigned int '+variable+';')
    if variable=='r':
        old='r = (short)(QualityDetectV'+name[-2:]+'(modem) != V'+name[-2:]+'_QUALITY_UNRELIABLE ? n : 0);'
        new=old.replace('(short)(','(',1)
    else:
        old='units = (short)((short)QualityDetectV29(modem) != V29Q_NO_CARRIER\n\t\t\t? (short)n : 0);'
        new='units = ((short)QualityDetectV29(modem) != V29Q_NO_CARRIER\n\t\t\t? n : 0);'
    assert fn.count(old)==1
    fn=fn.replace(old,new).replace('return '+variable+';','return (short)'+variable+';')
    return {'baseline':source,'wide-result':source[:start]+fn+source[end:]}

def audit():
    folder=d.ROOT/'build/gcc3-sample-count';result=json.loads((folder/'results.json').read_text());reports={}
    for family,fr in result['families'].items():
        bp=folder/family/'baseline/candidate.o';base=inspect(bp);cells=fr['cells'];target='RxHdxDataV'+family[1:3]
        assert len(cells)==2 and cells['baseline']['baseline_reproduced']
        reports[family]={}
        for label,cell in cells.items():
            path=folder/family/label/'candidate.o';q=inspect(path)
            for key in ('records','allocated','nobits','relocations'):assert q[key]==base[key],(family,label,key)
            changed=cell.get('changed_bodies',[])
            assert set(changed)<={target},(family,label,changed)
            for name in cell['functions']:
                if name!=target:assert d.b.body(path,name)==d.b.body(bp,name),(family,label,name)
            assert not cell.get('losses',[])
            reports[family][label]={'functions':len(cell['functions']),'changed':changed,'gains':cell.get('gains',[]),
              'target_verdict':cell['verdicts'][target],'target_size':len(d.b.body(path,target)[0]),
              'unchanged_bystanders':len(cell['functions'])-1,'metadata_data_nontext_relocations_equal':True}
    (folder/'complete-tu-audit.json').write_text(json.dumps(reports,indent=2)+'\n')
    print('6/6 complete-TU masked-count audits',json.dumps(reports))
if __name__=='__main__':
    if '--audit' in sys.argv:audit();raise SystemExit
    d.REV='14769433';d.OUT_NAME='gcc3-sample-count'
    d.SOURCE_PATHS=tuple('src/fax/V%sr_prc.c'%x for x in ('17','27','29'))
    d.DUMP_FLAGS=('-v','-save-temps','-da');d.variants=variants;d.main()
