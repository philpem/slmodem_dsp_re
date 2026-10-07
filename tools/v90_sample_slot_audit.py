#!/usr/bin/env python3
"""Complete ADID TU audit for the finite early-slot domain."""
import json
import playbook_small_patterns as d
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
b = d.b

text = (d.ROOT/'tools/playbook_bwch_unit_audit.py').read_text()
exec(text[text.index('def inspect'):text.index('reports=')])
text = (d.ROOT/'tools/gcc3_batch20_v90_small_audit.py').read_text()
exec(text[text.index('def nontext'):text.index('\nreports=')])
root = d.ROOT/'build/v90-sample-slot'
family = root/'V90AutoDigitalImpDetector'
cells = json.loads((root/'results.json').read_text())['families']['V90AutoDigitalImpDetector']['cells']
base = family/'baseline/candidate.o'
target = '_ZN25V90AutoDigitalImpDetector26addReceivedSampleToStorageEshf'
report = []
for label, cell in cells.items():
    obj = family/label/'candidate.o'
    assert inspect(obj) == inspect(base)
    assert nontext(obj) == nontext(base)
    assert len(d.b.sizes(str(obj))) == 34 and not cell.get('losses')
    assert set(cell.get('changed_bodies', [])) <= {target}
    report.append({'cell': label, 'functions': 34, 'target': cell['verdicts'][target],
                   'gains': cell.get('gains', []), 'changed_bodies': cell.get('changed_bodies', [])})
assert len(report) == 3 and not any(row['gains'] for row in report)
(root/'complete-tu-audit.json').write_text(json.dumps({'cells': 3, 'body_grades': 102,
    'hypotheses': 2, 'raw_repeat': 1, 'reports': report,
    'bystanders_metadata_named_data_nontext_relocations_unchanged': True}, indent=2)+'\n')
print('3/3 complete TUs / 102 body grades; no gains/losses; 33 bystanders, metadata/data/nontext/relocations unchanged')
