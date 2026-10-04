#!/usr/bin/env python3
"""Audit all declared full-TU detector controls and selected RTL boundaries."""
import json
import re
from pathlib import Path
import playbook_small_patterns as driver
from gcc3_reload_trace import instructions
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection

b = driver.b
source = (driver.ROOT/'tools/playbook_bwch_unit_audit.py').read_text()
exec(source[source.index('def inspect'):source.index('reports=')])

reports = {}
cells = []
for suffix in ['', '-order']:
    root = driver.ROOT/'build'/('gcc3-v34-detector-boundaries'+suffix)
    ledger = json.loads((root/'results.json').read_text())
    family = ledger['families']['detector']
    baseline = root/'detector/baseline/candidate.o'
    base_report = inspect(baseline)
    assert len(b.sizes(str(baseline))) == 2
    assert len(base_report['objects']) == 0
    for label, cell in family['cells'].items():
        path = root/'detector'/label/'candidate.o'
        report = inspect(path)
        assert report == base_report, (suffix, label, 'metadata/data')
        # This TU has no relocation sections or allocated nontext bytes.
        # Refuse any future introduction rather than silently masking it.
        with path.open('rb') as stream:
            elf = ELFFile(stream)
            assert not any(isinstance(s, RelocationSection) for s in elf.iter_sections())
            assert all(s['sh_size']==0 for s in elf.iter_sections() if s['sh_flags']&2 and s.name!='.text')
        changed = [n for n in b.sizes(str(baseline)) if b.body(str(path),n)!=b.body(str(baseline),n)]
        assert set(changed) <= {'detectorinit'}, (suffix,label,changed)
        assert not cell.get('losses', [])
        report['changed_bodies'] = changed
        reports[suffix+'/'+label] = report
        cells.append(cell)
assert len(cells)==18
winner=driver.ROOT/'build/gcc3-v34-detector-boundaries-order/detector/state-armed-limit-lo-hi/candidate.o'
assert b.verdict(*b.body(b.BLOB,'detectorinit'),*b.body(str(winner),'detectorinit')) == ('EXACT',0)
assert sum(c['verdicts']['detectorinit'][0]=='EXACT' for c in cells)==1


def nodes(pattern):
    if isinstance(pattern,list):
        yield pattern
        for child in pattern[1:]:
            yield from nodes(child)


def store_offsets(patterns):
    result=[]
    for pattern in patterns.values():
        if pattern[0]!='set' or not pattern[1][0].startswith('mem'):
            continue
        address=pattern[1][1]
        if address[0].startswith('reg') and 'd' in address:
            result.append(0)
        elif address[0]=='plus:SI' and address[1][0].startswith('reg') and 'd' in address[1] and address[2][0]=='const_int':
            result.append(int(address[2][1]))
    return result

stage_records={}
for label in ['baseline','short-counters','state-armed-limit-lo-hi']:
    folder=driver.ROOT/'build/gcc3-v34-detector-boundaries-order/detector'/label
    for stage in ['01.rtl','20.combine','22.regmove','24.lreg','25.greg','33.sched2']:
        text=(folder/('detector.c.'+stage)).read_text()
        found=[s for s in re.split(r'^;; Function ',text,flags=re.M)[1:] if s.splitlines()[0].strip()=='detectorinit']
        assert len(found)==1, (label,stage)
        selected=';; Function '+found[0]
        (folder/('detectorinit.'+stage)).write_text(selected)
        stage_records[label+'/'+stage]=instructions(selected)
base=stage_records['baseline/20.combine']
short=stage_records['short-counters/20.combine']
exact=stage_records['state-armed-limit-lo-hi/20.combine']
for patterns, expected in [(base,0),(short,2),(exact,2)]:
    extensions=[p for p in patterns.values() if p[0]=='set' and any(n[0]=='sign_extend:SI' for n in nodes(p)) and any(v in p[1] for v in ('tap','section'))]
    assert len(extensions)==expected
assert store_offsets(base)==[0,4,6,8,10,12,14,16,18]
assert store_offsets(short)==store_offsets(base)
assert store_offsets(exact)==[0,4,8,12,6,10,16,14,18]
summary={'valid_cells':len(cells),'source_hashes':len({c['source_hash'] for c in cells}),
         'object_hashes':len({c['object_hash'] for c in cells}),'selected_stage_records':len(stage_records),
         'functions_per_cell':2,'named_data_per_cell':0,'causal_controls':5,
         'exact_cells':['state-armed-limit-lo-hi'],'reports':reports}
(driver.ROOT/'build/detector-full-tu-audit.json').write_text(json.dumps(summary,indent=2)+'\n')
(driver.ROOT/'build/detector-stage-patterns.json').write_text(json.dumps(stage_records,indent=2)+'\n')
print('18/18 full TUs: 2 functions/0 data; metadata, empty nontext/relocations and bystanders preserved')
print('18/18 stage records; 5/5 counter-extension/store-order controls pass')
print('18 valid cells /',summary['source_hashes'],'source hashes /',summary['object_hashes'],'objects; 1 exact candidate; 4 invalid-domain attempts excluded')
