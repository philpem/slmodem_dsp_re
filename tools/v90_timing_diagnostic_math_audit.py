#!/usr/bin/env python3
"""Record timing controls while excluding the corrected false FABS nomination."""
import json
import playbook_small_patterns as d
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
b = d.b
text = (d.ROOT/'tools/playbook_bwch_unit_audit.py').read_text()
exec(text[text.index('def inspect'):text.index('reports=')])
text = (d.ROOT/'tools/gcc3_batch20_v90_small_audit.py').read_text()
exec(text[text.index('def nontext'):text.index('\nreports=')])
root = d.ROOT/'build/v90-timing-diagnostic-math'
family = root/'ResamplerTiming'
cells = json.loads((root/'results.json').read_text())['families']['ResamplerTiming']['cells']
base = family/'baseline/candidate.o'
meta, data = inspect(base), nontext(base)
target = '_ZN15ResamplerTiming21adjustHalfBaudBpfGainEf'
reports = []
for label, cell in cells.items():
    obj = family/label/'candidate.o'
    current = inspect(obj)
    assert current['records'] == meta['records']
    assert current['objects'] == meta['objects']
    assert len(b.sizes(str(obj))) == 14 and not cell.get('gains') and not cell.get('losses')
    assert set(cell.get('changed_bodies', [])) <= {target}
    nd = nontext(obj)
    changed = {k: [data.get(k), nd.get(k)] for k in set(data) | set(nd) if data.get(k) != nd.get(k)}
    assert all(k.startswith('.rodata.cst') for k in changed)
    rows = b.insns(str(obj), target)
    reports.append({'cell': label, 'target_grade': cell['verdicts'][target],
        'hardware_fabs': sum(op == 'fabs' for op, args in rows),
        'fldt': sum(op == 'fldt' for op, args in rows),
        'fldl': sum(op == 'fldl' for op, args in rows),
        'changed_constant_pools': changed,
        'whole_axis_source_witness_eligible': False if 'whole-double-1' in label else None})
assert len(reports) == 5 and all(row['hardware_fabs'] == 3 for row in reports)
(root/'complete-tu-audit.json').write_text(json.dumps({'full_tus': 5, 'body_grades': 70,
    'reports': reports, 'bystanders_named_data_bss_bindings_nonconstant_relocations_unchanged': True,
    'whole_axis_exclusion': 'early FCOMP incorrectly attributed to conditional fabs; all controls contain 3 FABS',
    'source_adoption': False}, indent=2)+'\n')
print('5/5 full TUs / 70 measured body grades; 13 bystanders/data/BSS/bindings/nonconstant relocations unchanged; no gains/losses')
print('All 5 contain 3 FABS: whole-axis nomination excluded from justified source evidence')
