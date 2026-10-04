#!/usr/bin/env python3
"""Audit all complete-TU data-mode controls, permitting only blob diagnostics."""
import json
import playbook_small_patterns as d
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
b=d.b
source=(d.ROOT/'tools/playbook_bwch_unit_audit.py').read_text()
exec(source[source.index('def inspect'):source.index('reports=')])

def canonical(path):
    report=inspect(path);report['nontext']={};report['nontext_relocations']={}
    with path.open('rb') as stream:
        elf=ELFFile(stream);syms=elf.get_section_by_name('.symtab');names={i:s.name for i,s in enumerate(elf.iter_sections())}
        for index,section in enumerate(elf.iter_sections()):
            if not section['sh_flags']&2 or section.name=='.text':continue
            raw=bytearray(section.data());rels={}
            for rs in elf.iter_sections():
                if not isinstance(rs,RelocationSection) or rs['sh_info']!=index:continue
                for r in rs.iter_relocations():
                    off=r['r_offset'];symbol=syms.get_symbol(r['r_info_sym']);kind={1:'R_386_32',2:'R_386_PC32'}[r['r_info_type']]
                    target=b.relocation_target(kind,symbol.name or names[symbol['st_shndx']],bytes(raw[off:off+4]),b.section_symbols(str(path)))
                    if target[:2]==('section','.text'):
                        owners=[s for s in syms.iter_symbols() if s['st_info']['type']=='STT_FUNC' and s['st_shndx']==elf.get_section_index('.text') and s['st_value']<=target[2]<s['st_value']+s['st_size']]
                        assert len(owners)==1;owner=owners[0];target=('function',owner.name,target[2]-owner['st_value'])
                    rels[off]=(kind,target);raw[off:off+4]=b'\0'*4
            report['nontext'][section.name]=raw.hex();report['nontext_relocations'][section.name]=rels
        report['text_relocations']=[]
        for rs in elf.iter_sections():
            if not isinstance(rs,RelocationSection) or elf.get_section(rs['sh_info']).name!='.text':continue
            raw=elf.get_section(rs['sh_info']).data()
            for r in rs.iter_relocations():
                off=r['r_offset'];symbol=syms.get_symbol(r['r_info_sym']);kind={1:'R_386_32',2:'R_386_PC32'}[r['r_info_type']]
                target=b.relocation_target(kind,symbol.name or names[symbol['st_shndx']],raw[off:off+4],b.section_symbols(str(path)))
                report['text_relocations'].append((kind,target))
    return report

allowed_functions={'v23rx':{'v23FP_rx_create','v23FP_rx_progress'},'bwchdem':{'BwChDem_Progress'},'v23tx':{'v23FP_tx_create'},'V32':{'V32FP_create'},'v23':{'v23_create','v23_process'},'v23modem':{'CreateV23Modem','V23ModemMain'},'B103prc':{'B103OriginateNextState','B103AnswerNextState'}}
with open(b.BLOB,'rb') as stream:
    blobelf=ELFFile(stream)
    blobstrings={x for sec in blobelf.iter_sections() if sec.name.startswith('.rodata.str') for x in sec.data().split(b'\0') if x}
    blobsyms=blobelf.get_section_by_name('.symtab')
    fw=next(x for x in blobsyms.iter_symbols() if x.name=='fw_ch_samp_per_bit_table')
    bw=next(x for x in blobsyms.iter_symbols() if x.name=='bw_ch_samp_per_bit_table')
    assert bw['st_value']-fw['st_value']==6
    data=blobelf.get_section(fw['st_shndx']).data()
    assert data[fw['st_value']:fw['st_value']+6].hex()=='070007000600'
    assert data[bw['st_value']:bw['st_value']+6].hex()=='6b006b006a00'
allreports={};count=0;invalid=0
for rootname in ['batch20-data-states','batch20-v23-debug','batch20-v23-rx','batch20-v23-rx-ratio','batch20-v23-wrapper','batch20-v23-composite','batch20-v23-constructor','batch20-b103-state','batch20-v23-progress-literal-fixed','batch20-v23-order']:
    root=d.ROOT/'build'/rootname;ledger=json.loads((root/'results.json').read_text())
    for family,data in ledger['families'].items():
        base=root/family/'baseline/candidate.o';baseline=canonical(base)
        for label,c in data['cells'].items():
            report=canonical(root/family/label/'candidate.o')
            if family=='v23modem':
                values=lambda records:{k:{q:v[q] for q in ('section','bytes','relocations')} for k,v in records.items()}
                assert values(report['objects'])==values(baseline['objects'])
                assert set(v['offset'] for v in report['objects'].values())=={0,6}
                # Constructor arm polarity changes deferred static table emission.
                # The exact winner places fw first, as the blob does at777c/7782.
                if rootname=='batch20-v23-constructor' and label=='debug-arm-ternary':
                    assert report['objects']['fw_ch_samp_per_bit_table']['offset']==0
                    assert report['objects']['bw_ch_samp_per_bit_table']['offset']==6
            else:
                assert report['objects']==baseline['objects'],(rootname,family,label,'named data')
            records={k:v for k,v in report['records'].items() if k not in ['dsplibs_debug_level','dsplibs_debug_printf']}
            brecords={k:v for k,v in baseline['records'].items() if k not in ['dsplibs_debug_level','dsplibs_debug_printf']}
            assert records==brecords,(rootname,family,label,'type/binding/visibility')
            for key in ['nontext','nontext_relocations','allocated_sizes']:
                nonstrings={k:v for k,v in report[key].items() if not k.startswith('.rodata.str') and not (family=='v23modem' and k=='.data' and key=='nontext')}
                bnon={k:v for k,v in baseline[key].items() if not k.startswith('.rodata.str') and not (family=='v23modem' and k=='.data' and key=='nontext')}
                assert nonstrings==bnon,(rootname,family,label,key)
            addedstrings={x for name,raw in report['nontext'].items() if name.startswith('.rodata.str') for x in bytes.fromhex(raw).split(b'\0') if x}-{x for name,raw in baseline['nontext'].items() if name.startswith('.rodata.str') for x in bytes.fromhex(raw).split(b'\0') if x}
            assert addedstrings<=blobstrings,(rootname,family,label,addedstrings-blobstrings)
            # Every added text relocation is an original blob diagnostic symbol/string.
            from collections import Counter
            additions=Counter(str(x) for x in report['text_relocations'])-Counter(str(x) for x in baseline['text_relocations'])
            permitted=['dsplibs_debug_level','dsplibs_debug_printf','V23FP Rx Created','v23: create','v23: V23STAT','V23ModemMain: modem state','V23FP version','Sep 22 2005','15:48:09','Generating answer tone','B103_STATE','default','Carrier Detection Time Out','v23 tone detected','Energy drop detected','V23 backward channel','V23Debug: Timeout','V23 no carrier']
            assert all(any(p in target for p in permitted) for target in additions),(rootname,family,label,additions)
            assert set(c.get('changed_bodies',[]))<=allowed_functions[family]
            assert not c.get('losses',[])
            isinvalid=rootname=='batch20-v23-composite' and family=='v23' and 'state' in label
            invalid+=isinvalid;count+=not isinvalid
            allreports[rootname+'/'+family+'/'+label]={'invalid_preimage':isinvalid,'functions':len(c['functions']),'named_data':len(report['objects']),'gains':c.get('gains',[]),'losses':c.get('losses',[]),'changed_bodies':c.get('changed_bodies',[]),'added_text_relocations':dict(additions)}
assert count==110 and invalid==2,(count,invalid)
(d.ROOT/'build/batch20-data-full-audit.json').write_text(json.dumps(allreports,indent=2)+'\n')
print('Full-TU audit:',count,'valid cells;',invalid,'invalid mapped-status controls excluded; metadata/named values stable; constructor table order recovers blob; added strings/relocations confined to original diagnostics; zero losses')
