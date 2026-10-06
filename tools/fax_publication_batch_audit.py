#!/usr/bin/env python3
"""Full-TU audit for bounded fax publication, accumulation and cleanup cells."""
import json, hashlib, re, subprocess
from collections import Counter
from pathlib import Path
import playbook_small_patterns as d
from elftools.elf.elffile import ELFFile
from gcc3_value_carriers_audit import inspect

PACKAGES={'fax-framer-publication':{'FAXVMI_process'},'fax-rx-accumulator':{'V17RX_modem','V21RX_modem','V27RX_modem'},'fax-rx-delete-dispatch':{'_delete_data_rx_modem'},'fax-next-state-publication':{'RxNextStateV21','TxNextStateV21'},'fax-unframe-publication':{'faxvmi_hdlc_unframe'}}

def main():
    rows=[]
    original_symbols=set(d.b.sizes(d.b.BLOB))
    body_calls={}
    def calls(path,name):
        key=(str(path),name)
        if key not in body_calls:
            text=subprocess.check_output(['python3',str(d.ROOT/'tools/dis.py'),str(path),name],text=True,stderr=subprocess.DEVNULL)
            body_calls[key]=dict(Counter(re.findall(r'R_386_PC32 ([\w.$]+)',text)))
        return body_calls[key]
    for package,allowed in PACKAGES.items():
        out=d.ROOT/'build'/package
        results=json.loads((out/'results.json').read_text())
        for family,values in results['families'].items():
            base=out/family/'baseline/candidate.o';baseline=inspect(base)
            for label,cell in values['cells'].items():
                obj=out/family/label/'candidate.o';current=inspect(obj)
                assert hashlib.sha256(obj.read_bytes()).hexdigest()==cell['object_hash'],(package,label,'object hash drift')
                source=out/family/label/Path(values['source_path']).name
                assert hashlib.sha256(source.read_bytes()).hexdigest()==cell['source_hash'],(package,label,'source hash drift')
                functions=sorted(d.b.sizes(str(obj)))
                assert functions==cell['functions'],(package,label,'symbol denominator drift')
                verdicts={name:list(d.b.verdict(*d.b.body(d.b.BLOB,name),*d.b.body(str(obj),name))) for name in functions if name in original_symbols}
                assert verdicts==cell['verdicts'],(package,label,'live canonical verdict drift')
                base_verdicts=values['cells']['baseline']['verdicts']
                gains=[n for n,v in verdicts.items() if v[0]=='EXACT' and base_verdicts[n][0]!='EXACT']
                losses=[n for n,v in base_verdicts.items() if v[0]=='EXACT' and verdicts[n][0]!='EXACT']
                if label!='baseline':
                    assert gains==cell['gains'] and losses==cell['losses'],(package,label,'live exact-set drift')
                pool_reordering=[]
                for kind in ['records','allocated','nobits','relocations']:
                    if kind=='allocated' and package=='fax-unframe-publication' and label=='current-parent-reads':
                        assert set(current[kind])==set(baseline[kind])
                        changed_pools=[name for name in current[kind] if current[kind][name]!=baseline[kind][name]]
                        assert changed_pools==['.rodata.str1.1'],changed_pools
                        pool='.rodata.str1.1'
                        before=bytes.fromhex(baseline[kind][pool]);after=bytes.fromhex(current[kind][pool])
                        assert len(before)==len(after)==197
                        assert Counter(before.split(b'\0'))==Counter(after.split(b'\0')),'format string multiset drift'
                        for path in [base,obj]:
                            with path.open('rb') as stream:
                                section=ELFFile(stream).get_section_by_name(pool)
                                assert section['sh_flags']==0x32 and section['sh_entsize']==section['sh_addralign']==1
                        def literals(data):
                            offset=0;found={}
                            for string in data.split(b'\0'):
                                if string:found[string.decode('ascii')]=offset
                                offset+=len(string)+1
                            return found
                        offsets_before=literals(before);offsets_after=literals(after)
                        pool_reordering=[{'literal':name,'baseline_offset':offsets_before[name],'candidate_offset':offsets_after[name]} for name in offsets_before if offsets_before[name]!=offsets_after[name]]
                        assert pool_reordering==[{'literal':'CRC is now OK!\n','baseline_offset':0x97,'candidate_offset':0xb5},{'literal':'Replacing the second byte...\n','baseline_offset':0xa7,'candidate_offset':0x97}]
                    else:
                        assert current[kind]==baseline[kind],(package,family,label,kind)
                changed=[name for name in d.b.sizes(str(base)) if d.b.body(str(obj),name)!=d.b.body(str(base),name)]
                bystanders=[]
                other=set(changed)-allowed
                if package=='fax-next-state-publication':
                    expected={'RxHdxDataV21','RxHdxWaitV21','RxHdxStartV21'} if family=='V21r_prc' else {'TxHdxDataV21','TxHdxIdleV21','TxHdxStartV21'}
                    assert other<=expected,(package,label,other)
                    baseline_source=(out/family/'baseline'/Path(values['source_path']).name).read_text()
                    for name in sorted(other):
                        assert d.function(baseline_source,name)[2]==d.function(source.read_text(),name)[2],(label,name,'bystander source changed')
                        before_calls=calls(base,name);after_calls=calls(obj,name)
                        assert before_calls==after_calls,(label,name,'bystander direct calls changed',before_calls,after_calls)
                        bystanders.append({'name':name,'direct_call_targets':after_calls,'source_unchanged':True,'baseline_to_candidate':list(d.b.verdict(*d.b.body(str(base),name),*d.b.body(str(obj),name)))})
                else:
                    assert not other,(package,label,changed)
                assert not cell.get('losses',[]),(package,label,'exact loss')
                if label=='baseline':assert cell['baseline_reproduced']
                rows.append({'package':package,'family':family,'label':label,'bodies':len(cell['functions']),'changed_bodies':changed,'gains':gains,'losses':losses,'reviewed_bystanders':bystanders,'reviewed_pool_reordering':pool_reordering})
    positive=[r for r in rows if r['package']=='fax-framer-publication' and r['label']=='post-callback-framer']
    assert len(positive)==1 and positive[0]['changed_bodies']==positive[0]['gains']==['FAXVMI_process']
    result={'cells':len(rows),'body_comparisons':sum(r['bodies'] for r in rows),'known_input_detector':{'changed_bodies':1,'exact_gains':1},'rows':rows}
    (d.ROOT/'build/fax-publication-batch-audit.json').write_text(json.dumps(result,indent=2)+'\n')
    print('fax publication audit:',result['cells'],'valid cells,',result['body_comparisons'],'function comparisons; knowninput detector fired; no exact losses; metadata/BSS/nontext relocations unchanged; one explicitly audited negative-control merge-string pool reorder')
if __name__=='__main__':main()
