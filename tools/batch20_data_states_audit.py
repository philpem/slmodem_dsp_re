#!/usr/bin/env python3
"""Full-TU byte, metadata, data, binding and relocation controls."""
import json
import playbook_small_patterns as d
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
b=d.b
src=(d.ROOT/'tools/playbook_bwch_unit_audit.py').read_text()
exec(src[src.index('def inspect'):src.index('reports=')])
root=d.ROOT/'build/batch20-data-states'
ledger=json.loads((root/'results.json').read_text());reports={};count=0
for family,data in ledger['families'].items():
    base=root/family/'baseline/candidate.o';base_report=inspect(base)
    def nontext(path):
        with path.open('rb') as stream:
            elf=ELFFile(stream)
            return {s.name:s.data().hex() for s in elf.iter_sections() if s['sh_flags']&2 and s.name!='.text'}
    base_bytes=nontext(base)
    for label,c in data['cells'].items():
        path=root/family/label/'candidate.o';report=inspect(path)
        assert report==base_report,(family,label,'metadata/data/binding')
        assert nontext(path)==base_bytes,(family,label,'allocated bytes')
        assert not c.get('losses')
        changed=c.get('changed_bodies',[])
        assert set(changed)<=({'v23FP_tx_create'} if family=='v23tx' else {'V32FP_create'})
        # Every relocation is in a function or the unchanged nontext data.
        rels=[]
        with path.open('rb') as stream:
            elf=ELFFile(stream);symtab=elf.get_section_by_name('.symtab')
            for rs in elf.iter_sections():
                if not isinstance(rs,RelocationSection):continue
                section=elf.get_section(rs['sh_info']);raw=section.data()
                for r in rs.iter_relocations():
                    off=r['r_offset'];kind={1:'R_386_32',2:'R_386_PC32'}[r['r_info_type']]
                    symbol=symtab.get_symbol(r['r_info_sym']);target=symbol.name or elf.get_section(symbol['st_shndx']).name
                    rels.append((section.name,kind,b.relocation_target(kind,target,raw[off:off+4],b.section_symbols(str(path)))))
        # Relocations are multiset compared: source scheduling moves sites.
        rels=sorted(rels,key=str)
        if label=='baseline':base_rels=rels
        assert rels==base_rels,(family,label,'canonical relocation targets')
        reports[family+'/'+label]={'functions':len(c['functions']),'data_symbols':len(report['objects']),'changed_bodies':changed,'canonical_relocations':len(rels),'target_verdict':c['verdicts'].get('v23FP_tx_create',c['verdicts'].get('V32FP_create'))}
        count+=1
assert count==8
assert ledger['families']['v23tx']['cells']['stores-cfg']['verdicts']['v23FP_tx_create']==['EXACT',0]
(root/'full-tu-audit.json').write_text(json.dumps(reports,indent=2)+'\n')
print('full-TU controls:',count,'/8; V23 functions3/data1; V32 functions6/data8; target-only changes, zero exact losses')
