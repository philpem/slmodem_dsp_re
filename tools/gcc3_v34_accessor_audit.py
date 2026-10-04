#!/usr/bin/env python3
"""Audit issue252 complete accessor TUs, including canonical data relocations."""
import json
import re
from gcc3_reload_trace import instructions
import playbook_small_patterns as driver
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection

b = driver.b
# Reuse the existing bounded ELF metadata/data inspector.
source = (driver.ROOT/'tools/playbook_bwch_unit_audit.py').read_text()
exec(source[source.index('def inspect'):source.index('reports=')])

roots = sorted(driver.ROOT.glob('build/gcc3-v34-accessor-*/results.json'))
reports_by_family = {}
count = 0
for ledger in roots:
    if 'invalid' in str(ledger):
        continue
    roots_in_family = sorted(ledger.parent.glob('*/baseline/candidate.o'))
    for baseline in roots_in_family:
        root = baseline.parent.parent
        cases = ['baseline'] + sorted(p.parent.name for p in root.glob('*/candidate.o') if p.parent.name != 'baseline')
        reports = {}
        for case in cases:
            path = root/case/'candidate.o'
            report = inspect(path)
            report.update(nontext={}, nontext_relocations={})
            with path.open('rb') as stream:
                elf = ELFFile(stream)
                syms = elf.get_section_by_name('.symtab')
                names = {i:s.name for i,s in enumerate(elf.iter_sections())}
                for index, section in enumerate(elf.iter_sections()):
                    if not section['sh_flags'] & 2 or section.name == '.text':
                        continue
                    raw, canonical = bytearray(section.data()), {}
                    for relsec in elf.iter_sections():
                        if not isinstance(relsec, RelocationSection) or relsec['sh_info'] != index:
                            continue
                        for relocation in relsec.iter_relocations():
                            offset = relocation['r_offset']
                            symbol = syms.get_symbol(relocation['r_info_sym'])
                            kind = {1:'R_386_32', 2:'R_386_PC32'}[relocation['r_info_type']]
                            canonical[offset] = (kind, b.relocation_target(kind, symbol.name or names[symbol['st_shndx']], bytes(raw[offset:offset+4]), b.section_symbols(str(path))))
                            target = canonical[offset][1]
                            if target[:2] == ('section', '.text'):
                                address = target[2]
                                owners = [sym for sym in syms.iter_symbols()
                                          if sym['st_info']['type'] == 'STT_FUNC'
                                          and sym['st_shndx'] == elf.get_section_index('.text')
                                          and sym['st_value'] <= address < sym['st_value'] + sym['st_size']]
                                assert len(owners) == 1, (path, address, owners)
                                owner = owners[0]
                                canonical[offset] = (kind, ('function', owner.name, address-owner['st_value']))
                            raw[offset:offset+4] = b'\0'*4
                    report['nontext'][section.name] = raw.hex()
                    report['nontext_relocations'][section.name] = canonical
            reports[case] = report
            for key in ('records', 'objects', 'allocated_sizes', 'nontext', 'nontext_relocations'):
                assert report[key] == reports['baseline'][key], (str(root), case, key)

        reports_by_family[str(root.relative_to(driver.ROOT))] = reports
        count += len(cases)
        print(root.name, ledger.parent.name, len(cases), 'cells', len(b.sizes(str(baseline))), 'functions', len(reports['baseline']['objects']), 'named data: controls pass')
assert count == 10, count
(driver.ROOT/'build/v34-accessor-full-tu-audit.json').write_text(json.dumps(reports_by_family, indent=2)+'\n')
combined=str(driver.ROOT/'build/gcc3-v34-accessor-combined/VPcmV34Main/combined/candidate.o')
base=str(driver.ROOT/'build/gcc3-v34-accessor-combined/VPcmV34Main/baseline/candidate.o')
changed=[n for n in b.sizes(base) if b.body(base,n)!=b.body(combined,n)]
assert set(changed)=={'VPcmV34GetSNR','VPcmV34GetQuickConnectIndication'},changed
for n in changed:
    assert b.verdict(*b.body(b.BLOB,n),*b.body(combined,n))==('EXACT',0)
print('10/10 valid cells: full metadata/data/canonical relocation controls pass; only two accessor bodies change, both EXACT')


def nodes(pattern):
    if isinstance(pattern,list):
        yield pattern
        for child in pattern[1:]:
            yield from nodes(child)

stage_records={}
for family,cases,name in [('quick',['baseline','switch-return','switch-result'],'VPcmV34GetQuickConnectIndication'),('snr',['baseline','flat-first','flat-handoff'],'VPcmV34GetSNR'),('snr-product',['baseline','product-before-last'],'VPcmV34GetSNR')]:
    for case in cases:
        folder=driver.ROOT/'build'/('gcc3-v34-accessor-'+family)/'VPcmV34Main'/case
        for suffix in ['01.rtl','18.cse2','20.combine','22.regmove','24.lreg','25.greg']:
            text=(folder/('VPcmV34Main.cpp.'+suffix)).read_text()
            blocks=re.split(r'^;; Function ',text,flags=re.M)[1:]
            found=[block for block in blocks if re.search(r'\b'+name+r'\(',block.splitlines()[0])]
            assert len(found)==1,(family,case,name,suffix,len(found))
            stream=';; Function '+found[0]
            (folder/(name+'.'+suffix)).write_text(stream)
            stage_records[family+'/'+case+'/'+suffix]=instructions(stream)
assert len(stage_records)==48
mask_nodes=[node for pattern in stage_records['quick/switch-result/01.rtl'].values() for node in nodes(pattern) if node[0]=='and:SI']
assert [int(node[-1][1]) for node in mask_nodes]==[231,1032,784]
for case,expected in [('snr/flat-handoff','last'),('snr-product/product-before-last','v')]:
    patterns=stage_records[case+'/20.combine']
    mult=[(uid,node) for uid,pattern in patterns.items() for node in nodes(pattern) if node[0]=='mult:SI' and node[-1][:2]==['const_int','4115']]
    assert len(mult)==1
    uid,node=mult[0]
    assert expected in node[1],(case,node)
    if expected=='v':
        oldv=node[1][1]
        copies=[copy_uid for copy_uid,pattern in patterns.items() for n in nodes(pattern) if n[0]=='set' and 'last' in n[1] and n[2][0].startswith('reg') and n[2][1]==oldv]
        assert len(copies)==1 and list(patterns).index(copies[0])>list(patterns).index(uid)
ledgers=[json.loads(p.read_text()) for p in roots]
cells=[c for ledger in ledgers for family in ledger['families'].values() for c in family['cells'].values()]
assert len(cells)==10
assert len({c['source_hash'] for c in cells})==7
assert len({c['object_hash'] for c in cells})==6
(driver.ROOT/'build/v34-accessor-stage-patterns.json').write_text(json.dumps(stage_records,indent=2)+'\n')
print('48/48 selected stage records parsed; 3/3 mask/product-use controls pass; 10 cells / 7 sources / 6 raw objects')
