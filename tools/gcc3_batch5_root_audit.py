#!/usr/bin/env python3
"""Audit batch 5 nonlinear and calling-tone complete TUs."""
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

roots = sorted(driver.ROOT.glob('build/gcc3-batch5-calling-tone*/results.json')) + sorted(driver.ROOT.glob('build/gcc3-batch5-nlencoder/results.json'))
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
            changed=[name for name in b.sizes(str(baseline))
                     if b.body(str(path),name)!=b.body(str(baseline),name)]
            allowed={'V34nlencoder'} if 'nlencoder' in str(root) else {'GenerateCallingTone'}
            assert set(changed)<=allowed,(root,case,changed)
            report['changed_bodies']=changed
            reports[case] = report
            for key in ('records', 'objects', 'allocated_sizes', 'nontext', 'nontext_relocations'):
                assert report[key] == reports['baseline'][key], (str(root), case, key)

        reports_by_family[str(root.relative_to(driver.ROOT))] = reports
        count += len(cases)
        print(root.name, ledger.parent.name, len(cases), 'cells', len(b.sizes(str(baseline))), 'functions', len(reports['baseline']['objects']), 'named data: controls pass')
assert count == 17,count
(driver.ROOT/'build/batch5-root-full-tu-audit.json').write_text(json.dumps(reports_by_family,indent=2)+'\n')
print('17/17 complete TUs: metadata/data/nontext canonical relocation controls pass')

def nodes(pattern):
    if isinstance(pattern,list):
        yield pattern
        for child in pattern[1:]:yield from nodes(child)

stages={}
root=driver.ROOT/'build/gcc3-batch5-calling-tone-tail/CallingTone'
for case in ['combined','in-place-amplitude','period-if-else','period-if-else-in-place-amplitude']:
    for suffix in ['01.rtl','20.combine','25.greg','31.bbro']:
        text=(root/case/('CallingTone.c.'+suffix)).read_text()
        blocks=[s for s in re.split(r'^;; Function ',text,flags=re.M)[1:] if s.splitlines()[0].strip()=='GenerateCallingTone']
        assert len(blocks)==1
        stream=';; Function '+blocks[0]
        (root/case/('GenerateCallingTone.'+suffix)).write_text(stream)
        stages[case+'/'+suffix]=instructions(stream)
    patterns=stages[case+'/20.combine']
    stores=[node for p in patterns.values() for node in nodes(p)
            if node[0]=='set' and node[1][0]=='mem:HI'
            and 'buf' in str(node[1]) and node[2][0]=='subreg:HI']
    assert len(stores)==1,(case,stores)
    assert ('amplitude' in str(stores[0][2]))==('in-place-amplitude' in case),(case,stores)
winner=root/'period-if-else-in-place-amplitude/candidate.o'
assert b.verdict(*b.body(b.BLOB,'GenerateCallingTone'),*b.body(str(winner),'GenerateCallingTone'))==('EXACT',0)
(driver.ROOT/'build/batch5-calling-tone-stage-patterns.json').write_text(json.dumps(stages,indent=2)+'\n')
print('16/16 selected stage records; 4/4 firing product-carrier controls; CallingTone exact215B')

# Preserve the negative V34 lifetime control through its canonicalization boundary.
nlroot=driver.ROOT/'build/gcc3-batch5-nlencoder/V34TX'
nlrecords={}
for case in ['baseline','update-mag','reverse-squares']:
    for suffix in ['01.rtl','20.combine','25.greg']:
        text=(nlroot/case/('V34TX.c.'+suffix)).read_text()
        blocks=[part for part in re.split(r'^;; Function ',text,flags=re.M)[1:] if part.splitlines()[0].strip()=='V34nlencoder']
        assert len(blocks)==1
        stream=';; Function '+blocks[0]
        (nlroot/case/('V34nlencoder.'+suffix)).write_text(stream)
        nlrecords[case+'/'+suffix]=instructions(stream)
assert (nlroot/'baseline/candidate.o').read_bytes()==(nlroot/'update-mag/candidate.o').read_bytes()
assert len(nlrecords['baseline/01.rtl'])!=len(nlrecords['update-mag/01.rtl'])
(driver.ROOT/'build/batch5-nlencoder-stage-patterns.json').write_text(json.dumps(nlrecords,indent=2)+'\n')
print('9/9 V34 negative stage records; changed initial RTL canonicalizes to identical raw object')
