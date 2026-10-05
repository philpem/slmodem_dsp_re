#!/usr/bin/env python3
"""Audit complete fax boundary experiment objects, with firing controls."""
import json, hashlib
from pathlib import Path
import playbook_small_patterns as d
from gcc3_value_carriers_audit import inspect

ROOT=d.ROOT

def main():
    report=[]
    for package in ['fax-crc-boundary','fax-rx-flags','fax-adapter-result','fax-adapter-consumers']:
        out=ROOT/'build'/package
        result=json.loads((out/'results.json').read_text())
        for family,data in result['families'].items():
            base=out/family/'baseline/candidate.o'
            base_i=inspect(base)
            for label,cell in data['cells'].items():
                obj=out/family/label/'candidate.o'
                assert hashlib.sha256(obj.read_bytes()).hexdigest()==cell['object_hash'],(package,family,label,'object hash')
                for symbol, verdict in cell['verdicts'].items():
                    assert list(d.b.verdict(*d.b.body(d.b.BLOB,symbol),*d.b.body(str(obj),symbol)))==verdict,(package,family,label,symbol,'live verdict')
                audit=inspect(obj)
                for kind in ['records','allocated','nobits','relocations']:
                    assert audit[kind]==base_i[kind],(package,family,label,kind)
                changes=[name for name in d.b.sizes(str(base)) if d.b.body(str(obj),name)!=d.b.body(str(base),name)]
                allowed=({'faxvmi_gen_fcs16'} if package=='fax-crc-boundary' else {'V21RX_control'} if package=='fax-rx-flags' else {f'v{mode}{side}_process' for mode in ['17','21','27','29'] for side in ['tx','rx']})
                assert set(changes)<=allowed,(package,family,label,changes)
                assert not cell.get('losses',[]),(package,family,label)
                if label=='baseline':assert cell['baseline_reproduced']
                if package=='fax-adapter-consumers' and not family.startswith('Vmi_v'):
                    assert obj.read_bytes()==base.read_bytes(),(family,label,'consumer raw drift')
                report.append({'package':package,'family':family,'label':label,'bodies':len(cell['functions']),'changed_bodies':changes,'gains':cell.get('gains',[]),'losses':cell.get('losses',[])})
    # Known source-level recovery must fire the body detector on both adapters.
    cells=[row for row in report if row['package']=='fax-adapter-result' and row['label']=='modem-status']
    assert len(cells)==4 and all(len(row['changed_bodies'])==len(row['gains'])==2 for row in cells)
    data={'cells':len(report),'body_comparisons':sum(row['bodies'] for row in report),'firing_controls':{'adapter_changed_bodies':8,'adapter_exact_gains':8},'reports':report}
    path=ROOT/'build/fax-boundary-full-tu-audit.json';path.write_text(json.dumps(data,indent=2)+'\n')
    print('complete TU audit:',data['cells'],'valid cells,',data['body_comparisons'],'body comparisons; original known-input detector fired8times; no metadata/data/BSS/nontext relocation drift or exact losses')
if __name__=='__main__':main()
